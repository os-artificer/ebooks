# glibc malloc / free 机制解析：一次分配从用户代码走到内核的全程

> **作者：**岭南过客  
> **更新时间：**2026-04-21  
> **源码版本：**glibc 2.43

一行 `malloc`、一行 `free`，背后其实干了很多事：先查线程本地缓存，没有再去抢 arena 锁，还不够就找内核要；`free` 时要合并相邻空闲块，条件合适才还给内核。RSS 只涨不降、尾延迟抖动、偶发崩溃，很多时候根就在这里。

> **尾延迟抖动**：尾延迟（tail latency）指响应时间分布里最慢的那部分请求，通常用 P99、P999 衡量。"抖动"是指这个延迟不稳定、时高时低。合在一起的意思是：大多数请求都很快，但偶尔会有少量请求突然变慢，且无规律。在 `malloc/free` 场景里，常见原因是 tcache 未命中后需要竞争 arena 锁、偶发的 `brk`/`mmap` 系统调用、以及触发内存归还时的 TLB 刷新。

本文基于 glibc 2.43 的源码，从最浅的"一次 malloc 到底调了哪些东西"开始，逐层往下，一直讲到"内存不足时分配器怎么退路"。

开始正文前先说一个背景：**glibc 2.43 彻底移除了 fastbin**，原来 fastbin 承担的"小块快速回收"完全交给 tcache，而 tcache 自身也新增了 12 个"大块桶"，覆盖上限**最高可配置到约 4 MiB**——但**默认上限仍是 1032 字节**（用户请求），大块桶需要通过 `GLIBC_TUNABLES=glibc.malloc.tcache_max=...` 手动调大后才会生效。本文将按最新的实现来讲。

---

## 一、先看一眼全景：一次 malloc / free 到底发生了什么

站在业务视角，`malloc(n)` 只有两种结果：要么拿到一块可写的内存，要么返回 `NULL`、把 `errno` 设为 `ENOMEM`。

但往下看一层，调用路径其实是一个**三层漏斗**：

```
malloc(n)
  └─ __libc_malloc                   ← 入口：尝试线程本地 tcache 命中
       │
       ├─ 命中 ─→ 返回（完全不碰 arena 锁）
       │
       └─ 未命中 ─→ __libc_malloc2   ← 进入带锁路径，选一个 arena
                    └─ _int_malloc    ← 在 bin、top chunk 里找一块合适的
                          │
                          └─ 都没有 ─→ sysmalloc ← 向内核要地：mmap 或 brk
```

`free(p)` 的路径是对称的：

```
free(p)
  └─ __libc_free                     ← 入口：对齐 + 双释放检查
       │
       ├─ 小块 / 中块 ─→ 放回 tcache（O(1)）
       │
       └─ 其余 ─→ _int_free_chunk
                  ├─ 与前后相邻空闲块合并
                  ├─ 归位到某个 bin
                  └─ 若合并出的块够大，trim 回内核
```

这张图里已经出现了几个核心概念：**tcache**（线程本地缓存）、**arena**（多线程共用的堆管理单元）、**bin**（按尺寸分层的空闲链表）、**chunk**（分配器的最小记账单元）、**top chunk**（arena 末尾的未分配块），下面一层一层拆开看。

---

## 二、chunk：一切故事的起点

分配器不是按字节管理内存的，它是按"块"来管理的，每一个块都是一个独立的 `malloc_chunk`：

```c
struct malloc_chunk {
    INTERNAL_SIZE_T      mchunk_prev_size;  /* 前一 chunk 的 size，仅当前驱空闲时有效 */
    INTERNAL_SIZE_T      mchunk_size;       /* 本 chunk 大小，低 3 位做标志 */

    struct malloc_chunk* fd;                /* 空闲链表前向指针 */
    struct malloc_chunk* bk;                /* 空闲链表后向指针 */

    struct malloc_chunk* fd_nextsize;       /* largebin：按 size 降序的“跳表” */
    struct malloc_chunk* bk_nextsize;
};
```

`mchunk_size` 的低 3 位分别表示：

- `PREV_INUSE (0x1)`：前驱 chunk 是否在用；
- `IS_MMAPPED (0x2)`：是否来自独立 `mmap`；
- `NON_MAIN_ARENA (0x4)`：是否属于非主 arena。

返回给用户的指针不是 chunk 头，而是跳过 `prev_size + size` 这两个字段之后的位置：

```c
#define CHUNK_HDR_SZ   (2 * SIZE_SZ)
#define chunk2mem(p)   ((void *) ((char *) (p) + CHUNK_HDR_SZ))
#define mem2chunk(mem) ((mchunkptr) ((char *) (mem) - CHUNK_HDR_SZ))
```

这里有个关键设计：**chunk 在"空闲"和"在用"两种状态下，用的是同一片内存**。

空闲时，`fd` / `bk` / `fd_nextsize` / `bk_nextsize` 是链表指针；一旦被分配出去，这段空间就交给用户写业务数据。

另外，本 chunk 的"尾部大小"字段，是放在**下一个 chunk 的 `mchunk_prev_size`** 里的——这就是为什么合并算法要同时读下一个 chunk 的头。

这个"头部复用 + 尾部外移"的技巧，把每个 chunk 的元数据压到了 `2 * SIZE_SZ`（64 位系统就是 16 字节）。代价也很明确：**一旦越界写把相邻 chunk 的头碰坏，链表会立刻失去一致性**——后文会看到分配器怎么通过廉价校验把这种损坏截在第一时间。

`checked_request2size` 负责把用户请求转成 chunk 尺寸：

```c
#define request2size(req)                                      \
    (((req) + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE) ?         \
     MINSIZE :                                                 \
     ((req) + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK)
```

做三件事：**溢出检查**、**按 `MALLOC_ALIGNMENT`（64 位下是 16 字节）对齐**、**下限为 `MINSIZE`**。
这意味着 `malloc(1)` 和 `malloc(8)` 可能拿到同样大的 chunk。

---

## 三、内存对齐：每次分配背后的尺寸修整

### 1. 对齐粒度从哪里来

glibc 的对齐粒度由 `MALLOC_ALIGNMENT` 决定，定义在 `sysdeps/generic/malloc-alignment.h`：

```c
/* 取 2*SIZE_SZ 与 long double 对齐要求的较大值 */
#define MALLOC_ALIGNMENT \
    (2 * SIZE_SZ < __alignof__(long double) \
     ? __alignof__(long double) : 2 * SIZE_SZ)
```

| 平台 | `SIZE_SZ` | `__alignof__(long double)` | `MALLOC_ALIGNMENT` |
|------|-----------|----------------------------|--------------------|
| x86-64 | 8 字节 | 16 字节 | **16 字节** |
| i386 | 4 字节 | 16 字节（x87 扩展精度） | **16 字节**（硬编码） |
| 32 位通用 | 4 字节 | 8 字节 | **8 字节** |

x86-64 下 `MALLOC_ALIGNMENT = 16`，这与 System V ABI 要求 SSE/AVX 操作数按 16 字节对齐的规定一致。

### 2. 用户请求如何变成 chunk 尺寸

`request2size` 在 chunk 头（`SIZE_SZ = 8` 字节）的基础上，把总尺寸向上对齐到 `MALLOC_ALIGNMENT`，同时保证不小于 `MINSIZE`：

```
chunk 尺寸 = max(MINSIZE, round_up(req + SIZE_SZ, MALLOC_ALIGNMENT))
```

x86-64 具体数字（`MINSIZE = 32`，`MALLOC_ALIGNMENT = 16`）：

| `malloc(n)` | chunk 尺寸 | 用户可用 | 说明 |
|------------|-----------|---------|------|
| `malloc(0)` | 32 字节 | 24 字节 | 下限 MINSIZE |
| `malloc(1)` | 32 字节 | 24 字节 | 下限 MINSIZE |
| `malloc(8)` | 32 字节 | 24 字节 | 下限 MINSIZE |
| `malloc(16)` | 32 字节 | 24 字节 | (16+8+15)&~15 = 32 |
| `malloc(24)` | 32 字节 | 24 字节 | (24+8+15)&~15 = 32 |
| `malloc(25)` | 48 字节 | 40 字节 | (25+8+15)&~15 = 48 |
| `malloc(64)` | 80 字节 | 72 字节 | (64+8+15)&~15 = 80 |

> `malloc_usable_size(p)` 返回的是"用户可用字节数"，即 chunk 尺寸减去头部，通常比申请值大。这片多出来的空间可以合法使用，但不建议依赖（bin 的分级让实际分到的 chunk 往往比请求大一档）。

### 3. 对齐与 bin 粒度的关系

`MALLOC_ALIGNMENT` 同时决定了 smallbin 的步长：

```c
#define SMALLBIN_WIDTH    MALLOC_ALIGNMENT   /* 每个 smallbin 对应的 size 范围宽度 */
```

所以 x86-64 上 smallbin 按 16 字节一档划分，共 64 档，覆盖 32 ~ 1008 字节。

tcache 同理：

```c
#define tidx2usize(idx) (((size_t)idx) * MALLOC_ALIGNMENT + MINSIZE - SIZE_SZ)
```

这意味着 tcache 桶 0 对应用户请求 0~24 字节，桶 1 对应 25~40 字节，以此类推——对齐粒度直接影响缓存命中率，请求大小越"整"，命中同一个桶的概率越高。

### 4. 超对齐分配：memalign / aligned_alloc / posix_memalign

当需要比 16 字节更严格的对齐（例如 AVX-512 需要 64 字节、DMA 缓冲区需要页对齐）时，可用以下三个 API：

| API | 标准 | 对 alignment 的要求 |
|-----|------|-------------------|
| `memalign(align, size)` | POSIX（已过时） | 2 的幂次 |
| `aligned_alloc(align, size)` | C17 | 2 的幂次且非零；`size` 须是 `align` 的倍数 |
| `posix_memalign(&p, align, size)` | POSIX.1-2001 | 2 的幂次且 ≥ `sizeof(void*)` |

三者最终都经由 `_mid_memalign` → `_int_memalign` 完成分配。

`_int_memalign` 的策略是：

1. 若 `alignment <= MALLOC_ALIGNMENT`，直接退化为普通 `malloc`；
2. 否则，过量申请 `nb + alignment + MINSIZE` 字节，确保对齐点一定落在所分 chunk 内；
3. 找到第一个满足对齐的地址 `newp`，把前面多出的"头部填充"切成独立 chunk 释放回 bin；
4. 把尾部多余部分同样切分释放。

```c
/* _int_memalign 核心片段（malloc.c:4684）*/
void *m = _int_malloc(av, nb + alignment + MINSIZE);   // 过量申请
p = mem2chunk(m);

if (!PTR_IS_ALIGNED(m, alignment)) {
    newp = mem2chunk(ALIGN_UP((uintptr_t)m + MINSIZE, alignment));
    size_t leadsize = PTR_DIFF(newp, p);
    set_head(newp, (size - leadsize) | PREV_INUSE | arena_flag);
    set_head_size(p, leadsize | arena_flag);
    _int_free_merge_chunk(av, p, leadsize);   // 前缀填充还给 bin
    p = newp;
}
```

**注意**：超对齐分配的头部填充会被立刻释放，但这些零散小块往往难以被后续分配复用，长期调用会加速内存碎片化。对于性能敏感路径，应尽量把需要超对齐的对象统一到一个内存池，而不是频繁调用 `aligned_alloc`。

---

## 四、分配路径：从 tcache 到 sysmalloc，三层漏斗

glibc 2.43 的 `__libc_malloc` 入口写得很克制，主逻辑只有两条 tcache 分支：

```c
void *
__libc_malloc (size_t bytes)
{
#if USE_TCACHE
    size_t nb = checked_request2size (bytes);

    if (nb < mp_.tcache_max_bytes)
    {
        size_t tc_idx = csize2tidx (nb);

        if (__glibc_likely (tc_idx < TCACHE_SMALL_BINS))
        {
            if (tcache->entries[tc_idx] != NULL)
                return tag_new_usable (tcache_get (tc_idx));
        }
        else
        {
            tc_idx = large_csize2tidx (nb);
            void *victim = tcache_get_large (tc_idx, nb);
            if (victim != NULL)
                return tag_new_usable (victim);
        }
    }
#endif
    return __libc_malloc2 (bytes);
}
```

这就是那个"三层漏斗"的第一层入口。

### 1. 第一层 tcache：线程本地的快路径

每个线程都有一份独立的 `tcache_perthread_struct`，结构非常紧凑：

```c
#define TCACHE_SMALL_BINS    64
#define TCACHE_LARGE_BINS    12      /* 可扩展覆盖到约 4 MiB，但默认不启用 */
#define TCACHE_MAX_BINS      (TCACHE_SMALL_BINS + TCACHE_LARGE_BINS)
#define TCACHE_FILL_COUNT    16

typedef struct tcache_perthread_struct
{
    uint16_t       num_slots[TCACHE_MAX_BINS];
    tcache_entry  *entries  [TCACHE_MAX_BINS];
} tcache_perthread_struct;
```

- **small tcache（64 个桶）**：每个桶存一种固定尺寸，步长 `MALLOC_ALIGNMENT = 16`。命中就是一次单链表弹出，O(1)。覆盖 chunk 尺寸 32 ~ 1040 字节，对应**用户请求 0 ~ 1032 字节**。
- **large tcache（12 个桶，2.43 新增）**：使用 `__builtin_clz` 做 log₂ 映射，每档覆盖约一倍大小区间，共 12 档，理论上限约 4 MiB。**默认不启用**——`mp_.tcache_max_bytes` 初始值为 `MAX_TCACHE_SMALL_SIZE + 1 = 1041`，只有通过 `GLIBC_TUNABLES=glibc.malloc.tcache_max=...` 把上限调大后，large bins 才会被实际使用。
- 每个桶默认缓存上限是 16 个 chunk（`TCACHE_FILL_COUNT`），可通过 `glibc.malloc.tcache_count` 调整（源码上限 `UINT16_MAX`）。

tcache 命中率越高，分配器就越"轻"——因为**这条路径完全不碰 arena 锁**，不会因为别的线程在分配而等待，因此 ptmalloc 的工程目标之一，就是尽可能把请求挡在这一层。

tcache 用单链表，存指针需要保护：

```c
#define PROTECT_PTR(pos, ptr) \
    ((__typeof (ptr)) ((((size_t) pos) >> 12) ^ ((size_t) ptr)))
#define REVEAL_PTR(ptr)  PROTECT_PTR (&ptr, ptr)
```

这就是 **Safe-Linking**。
它用"链表节点自身地址 `>> 12`"和 `next` 做 XOR，混淆出一个加密后的指针，读取时再异或一次恢复。
`>> 12` 是为了丢掉页内偏移，让剩下的高位对应 ASLR 带来的随机基址——攻击者想通过越界写把 `next` 指向任意地址，必须先泄露 ASLR 的 `mmap_base`。

tcache 每个 chunk 还带一个 `key` 字段，回收时写 `e->key = tcache_key`（进程级高熵随机数），下次 free 到来时 O(1) 判定是不是重复释放：

```c
if (__glibc_unlikely (e->key == tcache_key))
    return tcache_double_free_verify (e);
```

### 2. 第二层 \_int\_malloc：ptmalloc 的决策中心

tcache miss 之后走 `__libc_malloc2`，它的工作就两件：**挑一个 arena、拿着锁调 `_int_malloc`**，失败了还能换一个 arena 再试一次。

```c
static void * __attribute_noinline__
__libc_malloc2 (size_t bytes)
{
    mstate ar_ptr;
    void *victim;

    if (SINGLE_THREAD_P) {
        /* 单线程进程完全跳过锁，走 main_arena */
        return tag_new_usable (_int_malloc (&main_arena, bytes));
    }

    arena_get (ar_ptr, bytes);                    /* 选一个 arena 并加锁 */
    victim = _int_malloc (ar_ptr, bytes);

    if (!victim && ar_ptr != NULL) {
        /* 本 arena 要不出来，换一个再试 */
        ar_ptr = arena_get_retry (ar_ptr, bytes);
        victim = _int_malloc (ar_ptr, bytes);
    }

    if (ar_ptr != NULL)
        __libc_lock_unlock (ar_ptr->mutex);

    return tag_new_usable (victim);
}
```

`_int_malloc` 是整个分配器里最核心的决策函数，顺序搜索这么几级。

**(1) smallbin 精确匹配** 如果请求落在 smallbin 范围（64 位下约 32～1008 字节），直接查对应桶。

```c
if (in_smallbin_range (nb)) {
    idx = smallbin_index (nb);
    bin = bin_at (av, idx);
    if ((victim = last (bin)) != bin) {
        bck = victim->bk;
        if (__glibc_unlikely (bck->fd != victim))
            malloc_printerr ("malloc(): smallbin double linked list corrupted");
        ...
        /* 顺手把同尺寸剩余 chunk 塞进 tcache，摊薄下次的锁开销 */
        while (tcache->num_slots[tc_idx] != 0
               && (tc_victim = last (bin)) != bin) {
            ...
            tcache_put (tc_victim, tc_idx);
        }
        return chunk2mem (victim);
    }
}
```

注意那段"顺手填充 tcache"——**既然已经拿到锁，就多搬几个到线程本地**，下次再来同尺寸请求就能在第一层命中，这是 ptmalloc 对小对象吞吐优化的关键工程细节。

**(2) unsorted bin 扫描** unsorted bin 是整个分配器里唯一的"缓冲池"——最近 free 的 chunk、largebin 切剩的 remainder，都先落到这里。

`_int_malloc` 会在一个 `MAX_ITERS = 10000` 的循环里遍历它，做四件事：

- 校验大小、校验双向链表一致性（`bck->fd != victim` 就 abort）；
- 如果是小请求且 unsorted 只剩 `last_remainder`，用它切分以提升局部性；
- 如果 chunk 尺寸精确匹配，直接用（小块会先填满 tcache 再返回）；
- 否则把 chunk 正式归位到 smallbin 或 largebin，**largebin 归位时还要维护 `fd_nextsize` 跳表**。

`MAX_ITERS` 这个上限的作用是：防止某次 malloc 被一条恶意构造的超长 unsorted 链卡住。

**(3) largebin best-fit** 大请求走 largebin 的跳表搜索。

```c
victim = victim->bk_nextsize;
while (((unsigned long) (size = chunksize (victim)) < (unsigned long) nb))
    victim = victim->bk_nextsize;
```

命中后若剩余 `>= MINSIZE`，就切一块出来、把 remainder 塞回 unsorted bin——这是 unsorted bin 的**另一个入口**。

**(4) binmap 扫描** 如果对应桶是空的，走位图扫描找到下一个非空桶，切出合适大小。

**(5) top chunk 切分** 到这一步了，说明所有 bin 都没合适的，只能从 arena 末尾的 top chunk 切一刀。top 不够大？进最后一层漏斗。

### 3. 第三层 sysmalloc：向内核要地

`sysmalloc` 是最后的出口，它有两条主要路径：**mmap** 和 **brk 扩堆**，路径选择由请求大小决定。

```c
if (av == NULL
    || ((unsigned long) nb >= (unsigned long) mp_.mmap_threshold
        && mp_.n_mmaps < mp_.n_mmaps_max))
{
    char *mm;
    if (mp_.hp_pagesize > 0 && nb >= mp_.hp_pagesize) {
        /* 透明大页路径：直接 mmap 大页，避免后续 madvise */
        mm = sysmalloc_mmap (nb, mp_.hp_pagesize, mp_.hp_flags);
        if (mm != MAP_FAILED) return mm;
    }
    mm = sysmalloc_mmap (nb, pagesize, 0);
    if (mm != MAP_FAILED) return mm;
    tried_mmap = true;
}
```

**大于等于 `mmap_threshold`（默认 128 KiB，动态上调到最多 32 MiB）就直接 `mmap`**，这种 chunk 在头上打 `IS_MMAPPED` 位，free 时直接 `munmap`，不进入任何 bin、也不与别的 chunk 合并——完全独立的生命周期。

不够大（或 mmap 失败）就扩堆。

- **主 arena**：`MORECORE(size)`，在 Linux 上就是 `sbrk`。
  核心代码：

    ```c
    if ((ssize_t) size > 0) {
        brk = (char *) MORECORE ((long) size);
        if (brk != (char *) MORECORE_FAILURE)
            madvise_thp (brk, size);
        LIBC_PROBE (memory_sbrk_more, 2, brk, size);
    }
    ```
    
    扩堆成功后，把新得到的地址段并入 top chunk。

- **非主 arena**：先 `grow_heap` 扩当前 heap（本质是 `mprotect` 把之前预留的地址范围变可读写），失败再 `new_heap` 申请一整块对齐的 `HEAP_MAX_SIZE` 区域。

两条路径都失败了怎么办？进入"内存不足"那一节再细讲。

---

## 五、回收路径：free 背后的工作

`__libc_free` 的结构很清晰，它是一条 "先便宜检查、再分级归位、最后合并 trim" 的流水线。

```c
void
__libc_free (void *mem)
{
    if (mem == NULL) return;

    mchunkptr p = mem2chunk (mem);
    INTERNAL_SIZE_T size = chunksize (p);

    if (__glibc_unlikely (misaligned_chunk (p)))
        return malloc_printerr_tail ("free(): invalid pointer");

#if USE_TCACHE
    if (__glibc_likely (size < mp_.tcache_max_bytes))
    {
        tcache_entry *e = (tcache_entry *) chunk2mem (p);
        if (__glibc_unlikely (e->key == tcache_key))
            return tcache_double_free_verify (e);         /* 双释放检测 */

        size_t tc_idx = csize2tidx (size);
        if (__glibc_likely (tc_idx < TCACHE_SMALL_BINS)) {
            if (__glibc_likely (tcache->num_slots[tc_idx] != 0))
                return tcache_put (p, tc_idx);            /* 小块进 tcache */
        } else {
            tc_idx = large_csize2tidx (size);
            if (size >= MINSIZE
                && __glibc_likely (tcache->num_slots[tc_idx] != 0))
                return tcache_put_large (p, tc_idx);      /* 大块进 large tcache */
        }
    }
#endif

    _int_free_chunk (arena_for_chunk (p), p, size, 0);
}
```

### 1. 对齐检查 + 双释放检测

这两条是最便宜、也最有效的前置检查：

- `misaligned_chunk`：返回的用户指针如果对齐不正确，基本就是非法指针。
- `e->key == tcache_key`：如果这块内存上次是进 tcache 的，`key` 还在，当前又被 free 一次，直接 O(1) 判定为 tcache 内双释放。

这是把"常见缺陷立刻打断"的关键位置，成本极低、收益极高。

### 2. 分级放回

- 尺寸在 tcache 覆盖范围内，且对应桶没满，就直接进 tcache，free 路径在此结束；
- 否则走 `_int_free_chunk`，进入合并流程。

### 3. 合并：_int_free_merge_chunk

合并是这套分配器的"骨骼"，核心代码如下：

```c
static void
_int_free_merge_chunk (mstate av, mchunkptr p, INTERNAL_SIZE_T size)
{
    mchunkptr nextchunk = chunk_at_offset (p, size);

    /* 三道廉价防御：避免 top 双释放、越界 chunk、被标记为空闲的再次 free */
    if (__glibc_unlikely (p == av->top))
        malloc_printerr ("double free or corruption (top)");
    if (__glibc_unlikely (contiguous (av)
                          && (char *) nextchunk
                          >= ((char *) av->top + chunksize (av->top))))
        malloc_printerr ("double free or corruption (out)");
    if (__glibc_unlikely (!prev_inuse (nextchunk)))
        malloc_printerr ("double free or corruption (!prev)");

    INTERNAL_SIZE_T nextsize = chunksize (nextchunk);

    /* 向后合并：前驱若空闲，回退并 unlink */
    if (!prev_inuse (p)) {
        INTERNAL_SIZE_T prevsize = prev_size (p);
        size += prevsize;
        p = chunk_at_offset (p, -((long) prevsize));
        if (__glibc_unlikely (chunksize (p) != prevsize))
            malloc_printerr ("corrupted size vs. prev_size while consolidating");
        unlink_chunk (av, p);
    }

    size = _int_free_create_chunk (av, p, size, nextchunk, nextsize);
    _int_free_maybe_trim (av, size);
}
```

`_int_free_create_chunk` 再处理"向前合并"与最终归位。

```c
if (nextchunk != av->top) {
    bool nextinuse = inuse_bit_at_offset (nextchunk, nextsize);
    if (!nextinuse) { unlink_chunk (av, nextchunk); size += nextsize; }
    else            clear_inuse_bit_at_offset (nextchunk, 0);

    if (!in_smallbin_range (size)) {
        bck = unsorted_chunks (av); fwd = bck->fd;   /* 大块先进 unsorted */
    } else {
        int idx = smallbin_index (size);
        bck = bin_at (av, idx); fwd = bck->fd;       /* 小块直接进 smallbin */
        mark_bin (av, idx);
    }
    p->bk = bck; p->fd = fwd; bck->fd = p; fwd->bk = p;
    set_head (p, size | PREV_INUSE);
    set_foot (p, size);
} else {
    /* 紧邻 top，直接并入 top */
    size += nextsize;
    set_head (p, size | PREV_INUSE);
    av->top = p;
}
```

**这就是所谓的 immediate coalescing——只要物理相邻的块都是空闲的，立即合并成一个更大的块**，它依赖两个不变量：

1. `mchunk_prev_size` 与前驱 `chunksize` 一致；
2. 双向链表 `fd->bk == p && bk->fd == p`。

任何越界写、UAF 都会在这两条校验里触发 `malloc_printerr` 直接 abort，把"数据损坏扩散"限制在第一次错误附近，不让它延迟爆炸到几小时甚至几天之后。

### 4. trim：把内存还给内核

合并出来的块如果够大（`size >= ATTEMPT_TRIMMING_THRESHOLD`，即 64 KiB），会尝试把 top 之后的部分还给内核。

```c
static void
_int_free_maybe_trim (mstate av, INTERNAL_SIZE_T size)
{
    if (size >= ATTEMPT_TRIMMING_THRESHOLD) {
        if (av == &main_arena) {
#ifndef MORECORE_CANNOT_TRIM
            if (chunksize (av->top) >= mp_.trim_threshold)
                systrim (mp_.top_pad, av);       /* brk 回退 */
#endif
        } else {
            heap_info *heap = heap_for_ptr (top (av));
            heap_trim (heap, mp_.top_pad);       /* 可部分 munmap */
        }
    }
}
```

主 arena 走 `systrim`，其实就是 `sbrk(-n)`，把 brk 指针往回缩；非主 arena 走 `heap_trim`，可以把当前 heap 中 top 之后的整段 `munmap`。

mmap 出来的大 chunk 走独立分支。

```c
if (chunk_is_mmapped (p)) {
    /* 动态调高 mmap_threshold：这次的大 chunk 暗示“这个量级值得直接 mmap” */
    if (!mp_.no_dyn_threshold
        && chunksize_nomask (p) > mp_.mmap_threshold
        && chunksize_nomask (p) <= DEFAULT_MMAP_THRESHOLD_MAX) {
        mp_.mmap_threshold = chunksize (p);
        mp_.trim_threshold = 2 * mp_.mmap_threshold;
    }
    munmap_chunk (p);
}
```

**这是一个很多人踩过的坑**：程序一旦分配过一个很大的块再释放，`mmap_threshold` 会被拉高到那个量级，之后同量级的分配不再走 mmap、而是留在堆上——看起来像"内存泄漏"或者"RSS 只涨不降"，其实只是分配器在自适应。要关这套启发式，可以用 `mallopt(M_MMAP_THRESHOLD, ...)` 或 `GLIBC_TUNABLES` 把 `mmap_threshold` 钉死。

---

## 六、bin：按尺寸分层的空闲链表

上面反复出现的 "bin"，整体是一组按尺寸分层的空闲链表，每个 arena 有 128 个。

```c
#define NBINS             128
#define NSMALLBINS         64
#define SMALLBIN_WIDTH    MALLOC_ALIGNMENT
#define MIN_LARGE_SIZE    ((NSMALLBINS - SMALLBIN_CORRECTION) * SMALLBIN_WIDTH)
```

这 128 个 bin 的分工是：

- **smallbin（约 64 个）**：每个 bin 只存一种固定尺寸，步长 `MALLOC_ALIGNMENT`。命中即用，不需要遍历。
- **largebin（约 63 个）**：每个 bin 存一段尺寸区间，内部**按 chunk 尺寸降序双向链表排列**，并通过 `fd_nextsize` / `bk_nextsize` 形成"同尺寸段的跳表"，支持 best-fit 扫描。
- **unsorted bin（1 号 bin）**：近期 free 的 chunk 与 largebin 切分的 remainder，在正式归位前先落到这里。给 malloc 一次"快速复用"机会。
- **top chunk**：不在任何 bin 里，是 arena 末尾待用的最后一块，所有 bin 都匹配不上时才切它。

再加一张位图 `binmap`，记录每个 bin 是否非空，扫描时可以一跳跳过空桶。

这整套结构的总体节奏是：

```
free → 可能先去 tcache → 不行就去 unsorted bin → 下次 malloc 扫描时
        → 精确匹配就拿走 → 否则按 smallbin / largebin 归位 → 下次再按桶查找
```

换句话说，**unsorted bin 是延迟归位的缓冲池，tcache 是延迟合并的缓冲池**，它们的共同目标都是：把尽可能多的请求挡在便宜路径上。

---

## 七、arena：多线程下的可扩展性

单线程进程直接用 `main_arena` 就够了。
`main_arena` 靠 `brk` 扩堆，所有状态都集中在那里，但一旦多线程并发分配，锁竞争就会成瓶颈——这就是 arena 设计的出发点。

### 1. 主 arena 与非主 arena

- **main\_arena**：用 `brk/sbrk` 扩堆，地址空间连续；
- **非主 arena**：每个用 `mmap` 一整块 `HEAP_MAX_SIZE`（64 位默认 64 MiB）对齐区域做 heap，arena 控制块嵌在 heap 头部。需要更多空间时用 `mprotect` 把预留的地址范围变可读写，相当于按页增量扩堆。

arena 的数量有上限：

```c
#define NARENAS_FROM_NCORES(n) ((n) * (sizeof (long) == 4 ? 2 : 8))
```

64 位系统的自动上限按 `8×CPU 核数`推导，32 位按 `2×CPU` 推导。若显式设置 `arena_max`，则以该值为准（可通过 `GLIBC_TUNABLES` 配置）。

### 2. arena 选择：arena_get2

线程第一次 malloc 时要挑一个 arena。
`arena_get2` 的策略是"复用优先、不够再开新的、开够了只能排队"：

```c
static mstate
arena_get2 (size_t size, mstate avoid_arena)
{
    mstate a;
    static size_t narenas_limit;

    a = get_free_list ();                    /* ① 先看 free_list 有没有闲置 arena */
    if (a == NULL) {
        /* ② 算出 arena 上限，没到就开一个新 arena */
        ...
    repeat:;
        size_t n = narenas;
        if (__glibc_unlikely (n <= narenas_limit - 1)) {
            if (atomic_compare_and_exchange_bool_acq (&narenas, n + 1, n))
                goto repeat;
            a = _int_new_arena (size);
            if (__glibc_unlikely (a == NULL))
                atomic_fetch_add_relaxed (&narenas, -1);
        } else
            a = reused_arena (avoid_arena);  /* ③ 上限到了，找一个现有的共用 */
    }
    return a;
}
```

`reused_arena` 会尝试 `trylock` 每个已有 arena，抢到哪个就用哪个，如果都抢不到那就阻塞等下一个。

**这个设计有一个必须理解的工程含义** arena 数量封顶以后，多出来的线程会开始共用，锁竞争就回来了，因此"线程数 > 核数"的典型服务端场景，若分配压力大，要么调大 `arena_max`，要么缩小 tcache 以外的分配量（把更多请求挡在线程本地）。

### 3. arena 切换重试：arena_get_retry

如果本 arena 要不出来，`__libc_malloc2` 会换一个 arena 再试一次：

```c
static mstate
arena_get_retry (mstate ar_ptr, size_t bytes)
{
    LIBC_PROBE (memory_arena_retry, 2, bytes, ar_ptr);
    __libc_lock_unlock (ar_ptr->mutex);
    if (ar_ptr != &main_arena) {
        ar_ptr = &main_arena;
        __libc_lock_lock (ar_ptr->mutex);
    } else {
        ar_ptr = arena_get2 (bytes, ar_ptr);
    }
    return ar_ptr;
}
```

源码的注释点出了设计思路：**main arena 要不到，多半是 `sbrk` 失败了（地址空间被占或 `RLIMIT_DATA` 顶到），换别的 arena 还能 `mmap` 一块新 heap 试试；别的 arena 要不到，多半是 `mmap` 区太满（`mp_.n_mmaps_max`），回 main arena 走 `sbrk` 反而可能成功**。

这是一条简单却实用的退路。

---

## 八、内存不足时：分配器的应激反应

这是本文最关心的一节，因为"`malloc` 什么时候会真的返回 `NULL`" 的答案比想象中复杂得多。glibc 的分配器是一个多级退路系统，层层降级：

**第一级：tcache 命中** 不碰 arena，拿不到就走第二级。

**第二级：`_int_malloc` 在 bins / top chunk 里找** 只要当前 arena 曾经有过足够的空间，又恰好有空闲、又能合并出一块够大的 chunk，就成功。

**第三级：`sysmalloc` 的 mmap 路径** 大请求（`nb >= mmap_threshold`）或者"完全没有可用 arena"（`av == NULL`）时优先走 mmap：

```c
if (av == NULL
    || ((unsigned long) nb >= (unsigned long) mp_.mmap_threshold
        && mp_.n_mmaps < mp_.n_mmaps_max))
{
    ...
    mm = sysmalloc_mmap (nb, pagesize, 0);
    if (mm != MAP_FAILED) return mm;
    tried_mmap = true;
}
```

`n_mmaps` 如果已经到了 `n_mmaps_max`（默认 65536，防 VMA 爆炸）或 mmap 失败，这一级出局。

**第四级：扩当前 heap。**非主 arena 走 `grow_heap`，本质是 `mprotect` 把 heap 内还没启用的地址范围变可读写：

```c
if ((long) (MINSIZE + nb - old_size) > 0
    && grow_heap (old_heap, MINSIZE + nb - old_size) == 0)
    ...
```

每个非主 heap 最大 `HEAP_MAX_SIZE`，到顶了就失败。

**第五级：开新 heap / MORECORE。**

- 非主 arena：`new_heap` 申请一整块新的 heap 对齐区域；
- 主 arena：`MORECORE(size)` = `sbrk(size)` 扩堆。

主 arena 里 sbrk 失败还有一条备份路径——`sysmalloc_mmap_fallback`：

```c
if (brk == (char *) MORECORE_FAILURE) {
    /*
     * sbrk 失败了，但系统里可能还有离散空间可 mmap。
     * 这里会忽略 n_mmaps_max 和 mmap_threshold，因为这块空间不会作为独立 mmap chunk 使用。
     */
    size_t fallback_size = nb + mp_.top_pad + MINSIZE;
    char *mbrk = MAP_FAILED;
    if (mp_.hp_pagesize > 0)
        mbrk = sysmalloc_mmap_fallback (&size, fallback_size,
                                        mp_.hp_pagesize,
                                        mp_.hp_pagesize, mp_.hp_flags);
    if (mbrk == MAP_FAILED)
        mbrk = sysmalloc_mmap_fallback (&size, fallback_size,
                                        MMAP_AS_MORECORE_SIZE,
                                        pagesize, 0);
    if (mbrk != MAP_FAILED) {
        __set_vma_name (mbrk, fallback_size, " glibc: malloc");
        set_noncontiguous (av);                    /* 从此 arena 不再假设连续 */
        ...
    }
}
```

**换 arena 再试一次。**`__libc_malloc2` 里还会调用 `arena_get_retry`，把整条链重跑一遍，利用"主 arena 与非主 arena 用两条不同系统调用路径"的特点做第二次尝试。

**最后的终点：`ENOMEM`。**

```c
/* catch all failure paths */
__set_errno (ENOMEM);
return NULL;
```

所以当业务代码看到 `malloc` 返回 `NULL` 的时候，它背后实际上已经走完了：

```
tcache miss → _int_malloc 在 bin 里找不到 → top chunk 不够
     → sysmalloc 尝试 mmap 失败
     → 扩当前 heap 失败
     → 开新 heap 失败
     → sbrk 失败 + mmap fallback 失败
     → 换另一个 arena 把上面重跑一次
     → 还不行 → errno = ENOMEM
```

这条链的中间任何一环成功，malloc 就返回了。也就是说：**返回 `NULL` 的条件其实相当苛刻**——进程地址空间、内核 commit 策略（`vm.overcommit_memory`）、`RLIMIT_AS` / `RLIMIT_DATA`、VMA 数量上限（`vm.max_map_count`），至少有一项"卡死到底"，才会触发 ENOMEM。

分配器本身**不会主动触发 OOM killer** —— OOM 是内核在物理内存紧张时的行为。
在典型的 Linux 默认配置下（`overcommit_memory = 0`），`mmap` / `sbrk` 会先乐观返回成功，等你真正写入这些页面、内核发现没物理页时再触发 OOM。
这就是为什么"进程莫名被杀"有时候**跟 malloc 的返回值对不上号** —— malloc 没返回 NULL，但第一次 `memset` 就把进程带走了。

---

## 九、realloc：原地优先，复制兜底

`realloc` 的决策树在 `_int_realloc`（`malloc.c:4557`，已省略 arena flag、tag、校验等次要细节）：

```c
/* 1. 已够大：直接切掉尾部剩余，原地返回 */
if ((unsigned long) oldsize >= (unsigned long) nb) {
    newp = oldp;
    newsize = oldsize;
}
else {
    /* 2. 后继块是 top chunk：把 top 向后推，零拷贝扩展 */
    if (next == av->top &&
        (unsigned long)(newsize = oldsize + nextsize) >= (unsigned long)(nb + MINSIZE)) {
        set_head_size (oldp, nb | (av != &main_arena ? NON_MAIN_ARENA : 0));
        av->top = chunk_at_offset (oldp, nb);
        set_head (av->top, (newsize - nb) | PREV_INUSE);
        return chunk2mem (oldp);                            /* 零拷贝 */
    }
    /* 3. 后继块空闲：unlink 并入，零拷贝扩展 */
    else if (next != av->top && !inuse (next) &&
             (unsigned long)(newsize = oldsize + nextsize) >= (unsigned long) nb) {
        newp = oldp;
        unlink_chunk (av, next);                            /* 零拷贝 */
    }
    /* 4. 兜底：_int_malloc 新块 + memcpy + _int_free_chunk 旧块 */
    else {
        newmem = _int_malloc (av, nb - MALLOC_ALIGN_MASK);
        if (newmem == NULL) return NULL;

        newp = mem2chunk (newmem);
        newsize = chunksize (newp);

        /* 特殊情况：新 chunk 恰好紧接在 oldp 后面，直接合并，仍零拷贝 */
        if (newp == next) {
            newsize += oldsize;
            newp = oldp;
        } else {
            memcpy (newmem, chunk2mem (oldp), memsize (oldp));  /* 真正拷贝 */
            _int_free_chunk (av, oldp, chunksize (oldp), 1);
            return newmem;
        }
    }
}

/* 5. 切分尾部剩余（remainder_size >= MINSIZE 才切） */
remainder_size = newsize - nb;
if (remainder_size < MINSIZE) {
    set_head_size (newp, newsize | ...);
} else {
    remainder = chunk_at_offset (newp, nb);
    set_head_size (newp, nb | ...);
    set_head (remainder, remainder_size | PREV_INUSE | ...);
    _int_free_chunk (av, remainder, chunksize (remainder), 1);
}
return chunk2mem (newp);
```

mmap 出来的 chunk 走独立的 `mremap_chunk` 分支——能 `mremap` 就零拷贝完成。

性能上能不能命中"原地扩展"，关键在于**后继块是否空闲、或者它就是 top**。
而 tcache 会延迟"相邻 chunk 归还合并"，所以即便你顺序 free 了大量相邻小块，其中一部分仍可能卡在各个线程的 tcache 里、不与主块合并——**这是容器（`std::vector` 之类）在某些工作负载下 reserve 收益依然显著的底层原因**。

---

## 十、安全机制：廉价校验 + 出错立即 abort

分配器不追求"每次分配都体检一遍"。在热路径上只放最便宜、收益最明显的校验：

- **对齐检查**：`misaligned_chunk (p)`——`free` 的第一道关，非法指针直接挡下。
- **`unlink_chunk` 双向一致性**：`fd->bk == p && bk->fd == p`，阻断经典的 unlink 攻击。
- **smallbin / largebin / unsorted 的双向链表校验**：在分配路径上顺手检查，`bck->fd != victim` 即 abort。
- **tcache 双释放检测**：`e->key == tcache_key`，O(1) 识别同线程内的 tcache 双释放。
- **tcache 链表的 Safe-Linking**：`PROTECT_PTR` 把 `next` 与节点地址 `>> 12` 异或，让 UAF 改写 `next` 变得不能直接控制目标。
- **size 与 prev\_size 一致性**：合并前检查 `chunksize (p) != prevsize` 就 abort。
- **`malloc_printerr`**：一旦元数据损坏立刻 abort，避免后续分配继续放大损坏。

这些检查的共同特点：**成本极低，但能把"数据损坏无限扩散"限制在一个 chunk 范围内**。它不是安全沙箱，而是一套"止血"机制。

---

## 十一、调优与可观测

### 1. 关键 tunables

都以 `glibc.malloc.*` 的形式通过 `GLIBC_TUNABLES` 环境变量配置：

| 键 | 默认 | 说明 |
|---|---|---|
| `tcache_count` | 16 | 每个 tcache bin 的默认缓存上限（可调，源码上限 `UINT16_MAX`） |
| `tcache_max` | **1032 字节**（用户请求；对应 chunk 尺寸 1040） | 默认 1032 B 对应 chunk 总尺寸 1040 B（含头）；调大后 tcache 覆盖的最大用户请求尺寸最高可设到约 4 MiB |
| `tcache_unsorted_limit` | 0（无限） | unsorted 扫描时预填 tcache 的迭代上限 |
| `mmap_threshold` | 128 KiB 起，动态上调至 32 MiB | 超过此值直接 `mmap` |
| `trim_threshold` | 128 KiB 起，动态上调 | top chunk 超过此值触发 `sbrk(-n)` |
| `mmap_max` | 65536 | 并存 mmap 区域上限 |
| `arena_max` / `arena_test` | `arena_max=0`，`arena_test=8`（64 位） | 默认先按测试阈值启用，超过阈值后按 8×CPU 推导上限（或由 `arena_max` 显式钉死） |
| `top_pad` | 0 | 扩展 top 时额外填充量 |
| `perturb` | 0 | 分配 / 释放时填充的 byte，辅助捕获 UAF |

实战经验：

- **RSS 只涨不降**：优先看动态 `mmap_threshold` 是否被拉高；可以 `GLIBC_TUNABLES=glibc.malloc.mmap_threshold=131072` 钉死。
- **多线程争用**：`arena_max` 与 `tcache_count` 是最直接的两个旋钮。
- **碎片抬高**：`tcache_unsorted_limit` 能约束 tcache 过度囤积带来的延迟回收。

### 2. 可观测手段

- `mallinfo2()` / `malloc_info (0, FILE *)`：查看 arena、chunks 统计。
- `LIBC_PROBE` 静态探针：`memory_malloc_retry`、`memory_sbrk_more`、`memory_arena_new`、`memory_mallopt_free_dyn_thresholds` 等，可以用 systemtap 或 perf probe 挂钩。
- `MALLOC_PERTURB_` / `MALLOC_CHECK_`：诊断模式下捕获经典越界与双释放。
- `malloc_trim (0)`：手动触发一次 trim，检查 RSS 能不能掉下来。

### 3. 常见故障模式与定位

1. **`free(): invalid pointer` / `corrupted double-linked list`**：对齐或 unlink 校验命中，优先查 UAF 和越界写，而不是调参数。
2. **短时内存峰值**：tcache 把 chunk 压在线程本地、延迟合并；在突发分配后，若线程没有退出、`tcache_thread_shutdown` 没触发，这部分不会回到 arena。
3. **RSS 只涨不降**：动态 `mmap_threshold` + `trim_threshold` 的自适应结果。
4. **尾延迟抖动**：典型来源是 arena 锁争用、largebin 扫描、`sysmalloc`（尤其是新 heap 分配或 `sbrk`）。
5. **`malloc` 返回 NULL 但系统内存看着够**：检查 `vm.max_map_count`、`RLIMIT_AS`、`RLIMIT_DATA` 和 `n_mmaps_max`。
6. **大对象 realloc 慢**：看能不能命中 `next == av->top` 或 `!inuse (next)` 路径；容器预留往往比调分配器参数更有效。

---

## 十二、结语

glibc 的分配器可以总结为三句话：

1. **malloc / free 是一个三层漏斗**：tcache 挡在第一层做线程本地闭环，`_int_malloc` 在 arena 级的 bins 和 top chunk 里决策，`sysmalloc` 做最终的向内核索取。每一层都力图在最便宜的位置解决问题。
2. **整条链路上布满了"廉价校验 + 立即 abort"**：对齐、双链表一致性、size / prev\_size 一致、tcache\_key、Safe-Linking——它们不试图面面俱到，只力求把"数据损坏扩散"截在第一时间。
3. **工程表现由不变量维护、启发式阈值、工作负载三者共同决定**。调优之前要先定位热点：是 tcache 命中率、是 arena 锁争用、是 sysmalloc 频率，还是 OOM 之前的 `mmap` 饱和——不同热点对应完全不同的旋钮。

把这三层想清楚，再回头看一行 `malloc(n)` 和 `free(p)`，就能看出它在不同工作负载下能走多少条不同的路。

---

## 扩展：从 `free -lh` 看 malloc / free 对系统内存的影响

```
              total    used    free   shared  buff/cache  available
Mem:          7.5Gi   3.7Gi  287Mi    19Mi       3.9Gi      3.8Gi
Low:          7.5Gi   7.2Gi  287Mi
High:           0B      0B     0B
Swap:         4.0Gi   1.2Gi  2.8Gi
```

说明：为便于解释字段，示例保留了 `Low/High` 行；很多 64 位发行版的 `free -lh` 实际输出可能不显示这两行。

### 1. 各字段含义

| 字段 | 含义 |
|------|------|
| `total` | 系统物理内存总量 |
| `used` | 已被进程实际占用的物理页（不含 buff/cache） |
| `free` | 内核完全未使用的物理页 |
| `shared` | `tmpfs` 等共享内存占用 |
| `buff/cache` | 内核 page cache + 块设备 buffer，可在内存紧张时回收 |
| `available` | 估算的可供新分配使用的内存：`free + 可回收 buff/cache` |
| `Low` / `High` | 64 位系统已不使用 LowMem/HighMem 分区，此字段通常为 0（`High: 0B`）；该行是 32 位内核时代的遗留字段，可忽略 |
| `Swap` | 交换分区总量 / 已用 / 剩余 |

从图中数据看：`used(3.7GiB) + free(287MiB) + buff/cache(3.9GiB) ≈ total(7.5GiB)`。`free` 只有 287 MiB，但 `available` 有 3.8 GiB——内核可随时回收 page cache 让出内存，因此"真正可用"远比 `free` 大。

### 2. malloc 如何影响这些数字

**虚拟地址 ≠ 物理页**。`malloc` 返回指针的那一刻，内核只承诺了虚拟地址空间，物理页是在第一次写入时由缺页中断分配（demand paging）。因此：

- `malloc` 之后、写入之前：`used` **不变**；
- 写入后：物理页被 fault-in，`used` **增加**；
- `free` 之后：ptmalloc 把 chunk 留在 tcache / bin 里复用，物理页**依然在进程手里**，`used` **不变**；
- 仅当 ptmalloc 调用 `brk()` 缩堆或 `munmap()` 归还大块时，物理页才真正释放，`used` **才减少**。

这正是"RSS 只涨不降"的根本原因：`free(p)` 不等于还给操作系统。

### 3. Swap 已用 1.2 GiB 意味着什么

图中 Swap 已用 1.2 GiB，说明系统在某个时刻物理内存已不够用，内核把部分冷页换出。对 malloc 的影响：

- 若进程的 heap 中有长期未访问的 chunk（比如 tcache / bin 里缓存的空闲块），这些页可能已被换出；
- 下次访问这些 chunk（例如 tcache 命中后写入）会触发 swap-in，耗时从纳秒级跳升到毫秒级，直接表现为**尾延迟抖动**；
- `malloc_trim()` / `MADV_FREE` / `MADV_DONTNEED` 主动释放空闲 chunk 对应的物理页，可减少 swap 压力，但代价是下次分配需重新 fault-in。

### 4. `available` 是 malloc 真正能用的上限

`malloc` 向内核要内存（`sysmalloc`）时，内核参考的是 `available`，而不是 `free`。从图中看：

- `available = 3.8 GiB`：当前还有足够余量；
- 若 `available` 趋近于 0，`mmap` / `brk` 开始失败，`sysmalloc` 返回 `NULL`，`malloc` 最终返回 `NULL`；
- 同时 OOM Killer 可能介入，优先终止 `oom_score` 最高的进程。

**实践建议**：在内存敏感的服务里，监控 `available` 比监控 `free` 更有意义；当 `available < 总内存 × 10%` 时，应触发告警并考虑主动调用 `malloc_trim()` 或降低 `mmap_threshold`。
