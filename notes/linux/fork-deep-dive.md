# 深入 Linux 内核：一次 fork() 调用，内核到底做了什么？

> **作者：**岭南过客  
> **更新时间：**2026-04-10  
> **内核版本：**Linux v6.14

很多人学 Linux 编程，第一个"哇塞"的时刻就是 `fork()`——调用一次，返回两次；父进程拿到子 PID，子进程拿到 0。听起来像魔术，但内核里没有魔术，只有代码。

本文基于 **Linux v6.14** 源码，以 `kernel/fork.c` 为主线，把 `fork` 从用户态陷入内核，到子进程"活过来"的完整链路拆给你看。不是走马观花，而是钻进每一步，看它**做了什么**、**为什么这么做**、**不这么做会怎样**。

---

## 一、全景：调用链与数据流

先给一张全局地图，后面每一节都是在这张图的某个节点上展开。

```text
用户态
======
  应用程序: pid_t pid = fork();
       |
       v
  glibc / musl: 包装为 syscall(__NR_fork)
       |
  ─────|──────── 用户态 / 内核态 边界（syscall 指令 / SVC / ecall）────
       |
内核态
======
       v
  arch 入口: entry_SYSCALL_64 (x86) / el0_svc (arm64) ...
       |  保存用户态寄存器到 pt_regs，切换到内核栈
       v
  SYSCALL_DEFINE0(fork)                         ← kernel/fork.c:2897
       |  构造 kernel_clone_args { .exit_signal = SIGCHLD }
       v
  kernel_clone(&args)                           ← kernel/fork.c:2774
       |
       |─── (1) copy_process()                  ← kernel/fork.c:2147
       |         |
       |         |── 校验 clone_flags 合法性
       |         |── dup_task_struct()           ← kernel/fork.c:1112
       |         |     分配 task_struct + 内核栈
       |         |── copy_creds()               凭据（uid/gid/cap）
       |         |── sched_fork()               调度器初始化
       |         |── copy_files()               文件描述符表
       |         |── copy_fs()                  根目录 / cwd
       |         |── copy_sighand()             信号处理函数表
       |         |── copy_signal()              进程级信号状态
       |         |── copy_mm()                  ← kernel/fork.c:1725
       |         |     └─ dup_mm()              ← 分配 mm_struct + pgd
       |         |          └─ dup_mmap()       ← 复制 VMA (maple tree)
       |         |               └─ copy_page_range()  ← 复制页表 + COW 标记
       |         |── copy_namespaces()          命名空间
       |         |── copy_io()                  IO 上下文
       |         |── copy_thread()              ← arch 相关
       |         |     设置子进程 CPU 寄存器（返回值=0）
       |         |── alloc_pid()                分配 PID
       |         |── cgroup / tasklist 挂载
       |         └── 返回新 task_struct *p
       |
       |─── (2) wake_up_new_task(p)             放入运行队列
       |
       v
  父进程：系统调用返回 → 用户态拿到子 PID
  子进程：被调度后从 copy_thread 设好的返回点"醒来" → 用户态拿到 0
```

---

## 二、系统调用入口：fork 就是最简单的 clone

`kernel/fork.c:2897`：

```c
SYSCALL_DEFINE0(fork)
{
#ifdef CONFIG_MMU
    struct kernel_clone_args args = {
        .exit_signal = SIGCHLD,
    };
    return kernel_clone(&args);
#else
    return -EINVAL;
#endif
}
```

`kernel_clone_args` 是内核统一用来描述"创建新进程/线程"参数的结构体。`fork` 只填了一个字段：`exit_signal = SIGCHLD`，其余全为 0。

这意味着：

- **`flags = 0`**：不带任何 `CLONE_*` 标志 → 子进程什么都不与父进程共享
- **`exit_signal = SIGCHLD`**：子进程退出时给父进程发 `SIGCHLD`（经典 Unix 语义）
- **需要 MMU**：没有 MMU 的平台无法支持独立地址空间，直接返回 `-EINVAL`

对比同文件里的 `vfork` 和 `clone`：

```text
系统调用        flags                           含义
──────────    ──────────────────────────        ──────
fork          0                                 全部复制，不共享
vfork         CLONE_VFORK | CLONE_VM            共享地址空间，父等子
clone         用户传入                           精细控制每项资源
clone3        用户通过结构体传入                  clone 的扩展版
```

最终全部汇入 `kernel_clone()`。**`fork` 只是参数最简单的一种调用方式。**

---

## 三、kernel_clone()：创建 + 唤醒的调度者

`kernel/fork.c:2774`，简化骨架：

```c
pid_t kernel_clone(struct kernel_clone_args *args)
{
    u64 clone_flags = args->flags;
    struct task_struct *p;
    pid_t nr;

    // --- ptrace 事件类型判定 ---
    // fork → PTRACE_EVENT_FORK
    // vfork → PTRACE_EVENT_VFORK
    // clone(exit_signal != SIGCHLD) → PTRACE_EVENT_CLONE
    // 若没有 tracer 在监听，trace = 0，跳过上报

    // --- 核心：创建子进程 ---
    p = copy_process(NULL, trace, NUMA_NO_NODE, args);
    if (IS_ERR(p))
        return PTR_ERR(p);

    // --- 获取可见 PID ---
    pid = get_task_pid(p, PIDTYPE_PID);
    nr = pid_vnr(pid);  // 转成当前 PID 命名空间下的数值

    // --- CLONE_VFORK 同步（fork 不走这条）---
    if (clone_flags & CLONE_VFORK) {
        p->vfork_done = &vfork;
        init_completion(&vfork);
    }

    // --- LRU gen 多代页面回收（非 CLONE_VM 时）---
    if (IS_ENABLED(CONFIG_LRU_GEN_WALKS_MMU) &&
        !(clone_flags & CLONE_VM))
        lru_gen_add_mm(p->mm);

    // --- 唤醒子进程 ---
    wake_up_new_task(p);

    // --- CLONE_VFORK：父进程等待子进程 exec/exit ---
    if (clone_flags & CLONE_VFORK)
        wait_for_vfork_done(p, &vfork);

    return nr;  // 父进程拿到子 PID
}
```

关键设计决策：

**为什么先 `copy_process` 再 `wake_up_new_task`，不能合并？** 因为 `copy_process` 只负责"造出一个完整但未运行的 `task_struct`"，中间任何一步失败都可以干净地回退（函数末尾有 `bad_fork_*` 一系列 goto 清理标签）。只有全部成功，才把它交给调度器。这是典型的"两阶段提交"思路：先准备，再提交。

---

## 四、copy_process()：核心十一步详解

这是 `fork` 真正干活的地方，`kernel/fork.c:2147` 起，近 500 行。下面逐步展开。

### 1. 校验 clone_flags 组合

```c
if ((clone_flags & CLONE_THREAD) && !(clone_flags & CLONE_SIGHAND))
    return ERR_PTR(-EINVAL);

if ((clone_flags & CLONE_SIGHAND) && !(clone_flags & CLONE_VM))
    return ERR_PTR(-EINVAL);

if ((clone_flags & (CLONE_NEWNS|CLONE_FS)) == (CLONE_NEWNS|CLONE_FS))
    return ERR_PTR(-EINVAL);
```

内核强制了一条**包含链**：`CLONE_THREAD` ⊃ `CLONE_SIGHAND` ⊃ `CLONE_VM`。

```text
   CLONE_THREAD
       |  要求
       v
   CLONE_SIGHAND   （线程组必须共享信号处理）
       |  要求
       v
   CLONE_VM        （共享信号处理就必须共享地址空间）
```

原因：如果两个线程有不同的信号处理函数表，但共享线程组 ID，信号投递会出现语义混乱——投给进程的信号该用谁的 handler？所以内核直接禁止这种组合。

`CLONE_NEWNS | CLONE_FS` 也互斥：新挂载命名空间意味着挂载点独立，但 `CLONE_FS` 要求共享根目录/cwd，逻辑矛盾。

对 `fork` 来说 `flags = 0`，这些检查自然都能通过。

### 2. 信号延迟处理

```c
sigemptyset(&delayed.signal);
INIT_HLIST_NODE(&delayed.node);

spin_lock_irq(&current->sighand->siglock);
if (!(clone_flags & CLONE_THREAD))
    hlist_add_head(&delayed.node, &current->signal->multiprocess);
recalc_sigpending();
spin_unlock_irq(&current->sighand->siglock);

if (task_sigpending(current))
    goto fork_out;
```

**作用**：在 fork 执行期间收集发往"多进程"的信号，延迟到 fork 完成后投递。这保证信号不会在父子进程之间出现"看到一半"的状态。如果进入 `copy_process` 之前已有致命信号 pending，直接放弃 fork（返回 `-ERESTARTNOINTR`，系统调用重启机制会处理）。

### 3. dup_task_struct()——复制进程描述符

`kernel/fork.c:1112`：

```c
static struct task_struct *dup_task_struct(
    struct task_struct *orig, int node)
{
    struct task_struct *tsk;

    tsk = alloc_task_struct_node(node);   // slab 分配 task_struct
    arch_dup_task_struct(tsk, orig);      // 把父进程内容整体拷过来
    alloc_thread_stack_node(tsk, node);   // 分配内核栈（通常 2 页 = 16KB）
    setup_thread_stack(tsk, orig);        // 设置栈底 thread_info
    set_task_stack_end_magic(tsk);        // 栈末尾写魔数，用于溢出检测

    refcount_set(&tsk->rcu_users, 2);    // 用户态可见 + 调度器各持一计数
    refcount_set(&tsk->usage, 1);

    return tsk;
}
```

**`task_struct`** 是内核里"进程"的完整描述——调度信息、内存描述符指针、打开文件、信号、cgroup、命名空间……几乎所有进程状态都从这个结构体出发。v6.14 里这个结构体大约 **8~10KB**（取决于 config）。

`alloc_thread_stack_node` 分配的**内核栈**是子进程在内核态执行时使用的栈空间（系统调用、中断处理等都在上面跑）。x86_64 默认 **2 页 = 16KB**，开了 `VMAP_STACK` 的话还带 guard page 做溢出保护。

**`set_task_stack_end_magic`** 在栈底写入 `STACK_END_MAGIC`（`0x57AC6E9D`）。如果后续检测到这个值被覆盖，说明栈溢出了——这是最后一道防线。

### 4. copy_creds()——凭据复制

复制父进程的 `struct cred`（uid、gid、euid、supplementary groups、capability 集等）。子进程继承父进程的身份。`struct cred` 是引用计数管理的，`copy_creds` 在非 `CLONE_THREAD` 时会 `prepare_creds()` 分配一份新的副本。

### 5. 资源限制与调度准备

```c
if (is_rlimit_overlimit(task_ucounts(p), UCOUNT_RLIMIT_NPROC,
                         rlimit(RLIMIT_NPROC))) {
    if (p->real_cred->user != INIT_USER &&
        !capable(CAP_SYS_RESOURCE) && !capable(CAP_SYS_ADMIN))
        goto bad_fork_cleanup_count;
}
```

**`RLIMIT_NPROC`** 检查：当前用户的进程数是否已达上限。这是防 **fork 炸弹**（`:(){ :|:& };:`）的第一道关卡。注意 root（`INIT_USER`）和有 `CAP_SYS_RESOURCE` 的进程豁免。

```c
if (data_race(nr_threads >= max_threads))
    goto bad_fork_cleanup_count;
```

全局线程数上限（`max_threads`），由系统内存量在启动时计算，`/proc/sys/kernel/threads-max` 可调。

接下来 `sched_fork()` 初始化调度相关字段：

- 重置子进程的虚拟运行时间（vruntime，CFS 调度器的核心指标）
- 如果父进程设了 `SCHED_RESET_ON_FORK`，子进程恢复到 `SCHED_NORMAL` + nice 0
- 分配初始时间片

### 6. copy_files() / copy_fs()——文件表与文件系统上下文

**`copy_files`**（`kernel/fork.c:1785`）：

```c
if (clone_flags & CLONE_FILES) {
    atomic_inc(&oldf->count);  // 共享：引用计数 +1
    return 0;
}
// 不共享：复制整个 fdtable
newf = dup_fd(oldf, NR_OPEN_MAX, &error);
```

`fork` 的 `flags` 恒为 0，天然不含 `CLONE_FILES`，所以一定走 `dup_fd` 分支。这里涉及两层数据结构，复制的粒度不同：

- **`fdtable`（文件描述符表）**：被**整表复制**了一份。`dup_fd` 分配新的 `files_struct` + `fdtable`，把父进程的 fd 数组逐项拷贝过来。此后父子进程各自打开/关闭 fd 互不影响。
- **`struct file`（打开文件对象）**：**没有复制**，只是引用计数 +1（`get_file`）。父子进程的同编号 fd 仍然指向同一个 `struct file`，共享文件偏移量（`f_pos`）和文件状态标志。

所以 fork 后父子进程的 fd 0/1/2 指向同一个终端的同一个 `struct file`——这就是为什么父子进程写同一个文件时输出可能会交叉：它们在竞争同一个 `f_pos`。

**`copy_fs`**（`kernel/fork.c:1762`）：

```c
if (clone_flags & CLONE_FS) {
    fs->users++;
    return 0;
}
tsk->fs = copy_fs_struct(fs);  // 复制 root / pwd / umask
```

同理，`fork` 的 `flags` 不含 `CLONE_FS`，复制出独立的 `fs_struct`。子进程之后 `chdir` 不影响父进程。

### 7. copy_sighand() / copy_signal()——信号处理

```text
copy_sighand:  复制 sigaction 表（每个信号的处理函数/标志/掩码）
copy_signal:   分配新的 struct signal_struct（进程级共享的信号状态、
               POSIX 定时器、资源统计、rlimit、作业控制等）
```

`fork` 两个都不共享，各自复制。但线程（`CLONE_SIGHAND`）会共享 `sighand_struct`——这就是为什么同进程内所有线程的 `signal()` / `sigaction()` 互相可见。

### 8. copy_mm()——地址空间，fork 性能的核心

`kernel/fork.c:1725`，这是最值得深入的一步：

```c
static int copy_mm(unsigned long clone_flags, struct task_struct *tsk)
{
    struct mm_struct *mm, *oldmm;

    tsk->mm = NULL;
    tsk->active_mm = NULL;

    oldmm = current->mm;
    if (!oldmm)
        return 0;        // 内核线程没有 mm

    if (clone_flags & CLONE_VM) {
        mmget(oldmm);
        mm = oldmm;      // 线程/vfork：共享同一个 mm
    } else {
        mm = dup_mm(tsk, current->mm);  // fork：创建新 mm
        if (!mm)
            return -ENOMEM;
    }

    tsk->mm = mm;
    tsk->active_mm = mm;
    return 0;
}
```

`fork` 的 `flags` 恒为 0，不含 `CLONE_VM`，因此走 `dup_mm()` → `dup_mmap()` → `copy_page_range()` 三层调用链，各层职责不同：

**`dup_mm()`**（`kernel/fork.c:1684`）——mm_struct 级别的复制：

1. `allocate_mm()` 分配新的 `mm_struct`
2. `memcpy(mm, oldmm, sizeof(*mm))` 把父进程的 mm 内容整体拷过来
3. `mm_init()` 重新初始化不能共享的字段（分配新的页表根 `pgd`、初始化锁、引用计数等）
4. 调用 `dup_mmap()` 完成 VMA 和页表的复制

**`dup_mmap()`**（`kernel/fork.c:633`）——VMA 级别的复制：

1. 对父进程的 `mmap_lock` 加写锁（期间父进程的 `mmap`/`munmap` 等操作被阻塞）
2. `__mt_dup()` 复制父进程的 maple tree（VMA 查找用的索引结构）
3. `for_each_vma` 遍历每个 VMA：`vm_area_dup()` 复制 VMA 元数据（地址范围、权限、映射文件等），处理 `anon_vma`、userfaultfd、hugetlb 等
4. 对每个 VMA 调用 `copy_page_range()`（`mm/memory.c`）复制该区域的页表项

**`copy_page_range()`**——页表级别的复制，写时复制（COW）的核心就在这里：

```text
对于可写的匿名页（用户堆栈、malloc 出来的内存等）：
  1. 父进程页表项的写权限被清除（变成只读）
  2. 子进程页表项也指向同一物理页，同样只读
  3. 物理页的引用计数 +1

任何一方尝试写入时：
  → 触发 page fault（缺页异常）
  → 内核发现是 COW 页（引用计数 > 1）
  → 分配新物理页，拷贝内容
  → 更新写入方的页表项指向新页，恢复写权限
  → 引用计数 -1；如果降到 1，另一方也可以直接恢复写权限
```

**为什么这个设计如此重要？** 因为现实中 `fork` 之后最常见的操作是立刻 `exec`——`exec` 会丢弃整个旧地址空间，加载新程序。如果 `fork` 时就真的把所有物理页都复制一遍，那些页马上就会被丢弃，完全浪费。COW 让"fork + exec"模式几乎是**零拷贝**的。

**复杂度**：`dup_mmap` 的时间复杂度与父进程的 **VMA 数量**成正比（不是内存量）。一个内存映射密集的进程（比如 JVM、浏览器）可能有数千个 VMA，此时 fork 会比较慢——这也是 `vfork` 和 `posix_spawn` 存在的原因之一。

### 9. copy_namespaces() / copy_io()

`fork` 的 `flags` 同样不含任何 `CLONE_NEW*`，子进程直接继承父进程所在的全部命名空间（PID、网络、挂载、用户、UTS、IPC、cgroup、time）。`copy_io` 则处理 IO 调度上下文（CFQ 等调度器已淘汰，目前影响较小）。

### 10. copy_thread()——"子进程返回 0"的秘密

```c
retval = copy_thread(p, args);
```

这是**架构相关**的函数。以 x86_64 为例（`arch/x86/kernel/process.c`），核心工作：

```text
1. 把父进程陷入内核时保存的 pt_regs 复制到子进程内核栈顶
2. 将子进程 pt_regs 中的 ax 寄存器设为 0
   → 这就是 fork() 在子进程侧返回 0 的原因
3. 设置子进程的 thread.sp 指向其内核栈上 pt_regs 的位置
4. 设置子进程的 thread.ip 指向 ret_from_fork
   → 子进程第一次被 schedule() 选中时，从 ret_from_fork 开始执行
   → ret_from_fork 最终走到 syscall_return_slowpath → 回到用户态
```

**关键不变量**：父子进程从**同一个用户态指令地址**恢复执行（`pt_regs->ip` 相同），唯一的区别是 `%rax`（系统调用返回值寄存器）——父进程是子 PID，子进程是 0。用户态的 C 代码看到的 `fork()` 返回值就是从这个寄存器来的。

### 11. alloc_pid() + 收尾

```c
pid = alloc_pid(p->nsproxy->pid_ns_for_children,
                args->set_tid, args->set_tid_size);
```

在目标 PID 命名空间中分配一个新 PID。如果进程跨多层 PID namespace，每层都会分配一个 PID 号（`pid_vnr` 返回当前命名空间里看到的那个值）。

之后的收尾工作：

- **`cgroup_can_fork` / `sched_cgroup_fork`**：检查 cgroup 策略是否允许创建，关联调度组
- **加入 tasklist**：`write_lock_irq(&tasklist_lock)` 加锁后挂到进程树上，设置 `parent` / `real_parent`、`exit_signal`，加入 PID hash
- **`copy_seccomp`**：继承沙箱过滤规则
- **`ptrace_init_task`**：如果父进程正被 ptrace，处理子进程的调试状态

至此 `copy_process` 返回一个**完整但未运行**的 `task_struct`。

---

## 五、wake_up_new_task()——最后一脚

回到 `kernel_clone()`：

```c
wake_up_new_task(p);
```

这个函数（`kernel/sched/core.c`）做的事：

1. 把子进程状态设为 `TASK_RUNNING`
2. 选择一个 CPU（`select_task_rq`），放入该 CPU 的运行队列
3. 如果子进程应该抢占当前运行的任务，标记 `TIF_NEED_RESCHED`

从这一刻起，子进程可以被调度器选中执行。它的第一条内核态指令是 `ret_from_fork`（由 `copy_thread` 设定），最终通过系统调用返回路径回到用户态，带着返回值 0。

---

## 六、错误处理：goto 清理链

`copy_process` 里有一长串 `bad_fork_*` 标签：

```text
bad_fork_core_free
bad_fork_cancel_cgroup
bad_fork_put_pidfd
bad_fork_free_pid
bad_fork_cleanup_thread
bad_fork_cleanup_io
bad_fork_cleanup_namespaces
bad_fork_cleanup_mm
bad_fork_cleanup_signal
bad_fork_cleanup_sighand
bad_fork_cleanup_fs
bad_fork_cleanup_files
bad_fork_cleanup_semundo
bad_fork_cleanup_security
bad_fork_cleanup_audit
bad_fork_sched_cancel_fork
bad_fork_cleanup_policy
bad_fork_cleanup_delayacct
bad_fork_cleanup_count
bad_fork_cleanup_creds
bad_fork_free
fork_out
```

这是内核 C 代码里经典的**反向清理模式**：每一步资源分配都对应一个清理标签。如果第 N 步失败，`goto bad_fork_cleanup_N` 会从第 N-1 步开始反向释放已分配的资源。。

---

## 七、fork 的性能特征与设计权衡

### COW 不是万能的

COW 让 fork 很快，但有代价：

- **fork 后双方都触发大量写 fault**：如果父子进程都要频繁写入，COW 反而比直接复制慢（每次 fault 有 TLB flush 等开销）
- **VMA 数量是瓶颈**：`dup_mmap` 遍历所有 VMA，JVM 类应用可能有几千个 VMA，fork 耗时可达毫秒级
- **`mmap_lock` 争用**：`dup_mmap` 需要持有父进程的 `mmap_lock`（读锁），期间父进程的 `mmap` / `munmap` 等操作被阻塞

### 为什么不直接用 posix_spawn？

`posix_spawn` 在内核层面（`fork + exec`）可以避免复制地址空间的开销，但 API 能力受限——无法在 fork 和 exec 之间执行任意操作（如关闭 fd、改信号掩码、`setuid` 等）。传统 Unix 的 `fork + exec` 模型灵活性更高，而 COW 让它的性能也可以接受。

### THP（透明大页）与 fork

如果父进程使用了 2MB 的透明大页，fork 的 COW 会在 huge page 粒度上操作。这意味着写 fault 时可能需要复制一整个 2MB 页（而不是 4KB），对延迟敏感的应用（如 Redis）影响显著。这就是 Redis 建议关注 `fork` 延迟的原因。

---

## 八、总结

一句话概括 fork 的内核实现：

> **分配新的进程描述符和内核栈，按"全部复制"策略逐项拷贝父进程资源（凭据、文件、信号、地址空间等），利用写时复制避免立即拷贝物理内存，由架构代码设好子进程首次运行时的 CPU 寄存器（使 fork 在子进程侧返回 0），最后交给调度器唤醒。**

关键路径树：

```text
sys_fork  →  kernel_clone  →  copy_process
                                  ├── dup_task_struct    分配 task_struct + 内核栈
                                  ├── copy_creds         uid/gid/capability
                                  ├── sched_fork         调度器字段初始化
                                  ├── copy_files         文件描述符表
                                  ├── copy_fs            根目录 / cwd
                                  ├── copy_sighand       信号处理函数
                                  ├── copy_signal        进程级信号状态
                                  ├── copy_mm + COW      地址空间（写时复制）
                                  ├── copy_namespaces    命名空间
                                  ├── copy_thread        CPU 寄存器（返回值=0）
                                  ├── alloc_pid          分配 PID
                                  └── cgroup / tasklist  进程树挂载
                              wake_up_new_task           放入运行队列
```

---

*本文基于 Linux v6.14 内核源码分析。
核心文件：`kernel/fork.c`
关联文件：`mm/memory.c`（COW / `copy_page_range`）、`arch/x86/kernel/process.c`（`copy_thread`）、`kernel/sched/core.c`（`wake_up_new_task`）。*
