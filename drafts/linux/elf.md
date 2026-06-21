# ELF 文件格式数据结构全解析

> 每个 Linux 平台的开发者肯定会接触到 ELF。它不仅是可执行文件的载体，更是链接、加载、Core Dump、内核模块、vdso 等核心机制的基础。本文将从 Linux 内核源码出发，彻底搞懂 ELF 的数据结构。本文也是我的学习笔记，希望对有缘读到此文的你也有帮助。

---

## 一、什么是 ELF？

ELF（Executable and Linkable Format）是 Linux 系统下可执行文件、共享库、目标文件和 Core Dump 的标准格式。它定义了二进制文件中代码、数据、符号、重定位等信息的组织方式。

Linux 内核（v6.14）中，ELF 的核心数据结构定义在以下文件中：

| 文件 | 说明 |
|------|------|
| `include/uapi/linux/elf.h` | **核心**：所有 ELF 结构体与常量 |
| `include/uapi/linux/elf-em.h` | 支持的机器架构类型 |
| `include/linux/elfcore.h` | Core Dump 相关结构体 |
| `include/linux/elfnote.h` | ELF Note 段生成宏 |
| `fs/binfmt_elf.c` | ELF 可执行文件加载器 |

---

## 二、ELF 文件整体布局

一个 ELF 文件从宏观上看包含四个逻辑区域：

```
+-------------------+
|   ELF Header      |  文件头，位于文件最开头，描述整体布局
+-------------------+
|   Program Header  |  程序头表（加载视图），位置由 e_phoff 指定
|   Table           |
+-------------------+
|   Section 1       |
|   Section 2       |  各种节区（链接视图）
|   ...             |
+-------------------+
|   Section Header  |  节区头表，位置由 e_shoff 指定
|   Table           |
+-------------------+
```

> 注意：Program Header Table 和 Section Header Table 在文件中的位置并不固定，而是由 ELF Header 中的 `e_phoff` 和 `e_shoff` 字段分别指定，因此上图中的相对顺序仅是一种典型布局。

- **链接视图**（Section）：供编译器和链接器使用
- **加载视图**（Segment/Program Header）：供内核加载器使用

同一个文件，编译时看 Section，运行时看 Segment。

---

## 三、ELF 基础类型

ELF 规范定义了 32 位和 64 位两套基础类型：

```c
/* 32-bit ELF base types */
typedef __u32  Elf32_Addr;    // 地址
typedef __u16  Elf32_Half;    // 半字
typedef __u32  Elf32_Off;     // 文件偏移
typedef __s32  Elf32_Sword;   // 有符号字
typedef __u32  Elf32_Word;    // 无符号字

/* 64-bit ELF base types */
typedef __u64  Elf64_Addr;
typedef __u16  Elf64_Half;
typedef __s16  Elf64_SHalf;
typedef __u64  Elf64_Off;
typedef __s32  Elf64_Sword;
typedef __u32  Elf64_Word;
typedef __u64  Elf64_Xword;   // 64位字
typedef __s64  Elf64_Sxword;  // 有符号64位字
```

> **关键差异**：64 位增加了 `Xword`/`Sxword` 类型，地址和偏移都扩展到了 8 字节。

---

## 四、ELF Header —— 文件头

ELF Header 是文件的"身份证"，位于文件最开始，描述了整个文件的基本信息。

### 4.1 e_ident —— 魔数与身份标识

文件头的前 16 个字节是 `e_ident[]`，其索引定义如下：

```c
#define EI_NIDENT   16

#define EI_MAG0     0   // 魔数第1字节: 0x7f
#define EI_MAG1     1   // 魔数第2字节: 'E'
#define EI_MAG2     2   // 魔数第3字节: 'L'
#define EI_MAG3     3   // 魔数第4字节: 'F'
#define EI_CLASS    4   // 文件类别: 32位/64位
#define EI_DATA     5   // 字节序: 小端/大端
#define EI_VERSION  6   // ELF 版本
#define EI_OSABI    7   // 目标操作系统 ABI
#define EI_PAD      8   // 填充字节起始
```

魔数定义：

```c
#define ELFMAG0     0x7f
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define ELFMAG      "\177ELF"
#define SELFMAG     4
```

> 用 `readelf -h` 看到的文件头，开头就是 `7f 45 4c 46`，正是 `\x7fELF`。

**EI_CLASS 取值**：

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `ELFCLASSNONE` | 无效 |
| 1 | `ELFCLASS32` | 32 位 ELF |
| 2 | `ELFCLASS64` | 64 位 ELF |

**EI_DATA 取值**：

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `ELFDATANONE` | 无效 |
| 1 | `ELFDATA2LSB` | 小端字节序 |
| 2 | `ELFDATA2MSB` | 大端字节序 |

**EI_VERSION 取值**：

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `EV_NONE` | 无效 |
| 1 | `EV_CURRENT` | 当前版本 |

**EI_OSABI 取值**：

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `ELFOSABI_NONE` | 无/UNIX System V |
| 3 | `ELFOSABI_LINUX` | Linux |

### 4.2 Elf32_Ehdr / Elf64_Ehdr 结构体

```c
typedef struct elf32_hdr {
    unsigned char e_ident[EI_NIDENT];  // 魔数 + 标识信息
    Elf32_Half    e_type;              // 文件类型
    Elf32_Half    e_machine;           // 目标架构
    Elf32_Word    e_version;           // 版本号
    Elf32_Addr    e_entry;             // 入口点虚拟地址
    Elf32_Off     e_phoff;             // Program Header 表偏移
    Elf32_Off     e_shoff;             // Section Header 表偏移
    Elf32_Word    e_flags;             // 架构相关标志
    Elf32_Half    e_ehsize;            // ELF Header 大小
    Elf32_Half    e_phentsize;         // 每个 Program Header 大小
    Elf32_Half    e_phnum;             // Program Header 数量
    Elf32_Half    e_shentsize;         // 每个 Section Header 大小
    Elf32_Half    e_shnum;             // Section Header 数量
    Elf32_Half    e_shstrndx;          // 节区名字符串表索引
} Elf32_Ehdr;

typedef struct elf64_hdr {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half    e_type;
    Elf64_Half    e_machine;
    Elf64_Word    e_version;
    Elf64_Addr    e_entry;
    Elf64_Off     e_phoff;
    Elf64_Off     e_shoff;
    Elf64_Word    e_flags;
    Elf64_Half    e_ehsize;
    Elf64_Half    e_phentsize;
    Elf64_Half    e_phnum;
    Elf64_Half    e_shentsize;
    Elf64_Half    e_shnum;
    Elf64_Half    e_shstrndx;
} Elf64_Ehdr;
```

> 32 位和 64 位的 Header 结构体在逻辑上完全一致，只是字段宽度不同。64 位中 `e_entry`、`e_phoff`、`e_shoff` 都是 8 字节。

### 4.3 e_type —— 文件类型

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `ET_NONE` | 未知类型 |
| 1 | `ET_REL` | 可重定位文件（`.o`） |
| 2 | `ET_EXEC` | 可执行文件 |
| 3 | `ET_DYN` | 共享目标文件（`.so`） |
| 4 | `ET_CORE` | Core Dump 文件 |

> 现代 Linux 下，位置无关可执行文件（PIE）的 `e_type` 也是 `ET_DYN`，和 `.so` 一样。

### 4.4 e_machine —— 目标架构

架构常量定义在 `include/uapi/linux/elf-em.h` 中：

```c
#define EM_NONE        0    // 无
#define EM_386         3    // Intel 80386
#define EM_ARM         40   // ARM 32位
#define EM_X86_64      62   // AMD x86-64
#define EM_AARCH64     183  // ARM 64位
#define EM_RISCV       243  // RISC-V
#define EM_LOONGARCH   258  // LoongArch
#define EM_MIPS        8    // MIPS
#define EM_PPC         20   // PowerPC
#define EM_PPC64       21   // PowerPC64
#define EM_S390        22   // IBM S/390
#define EM_PARISC      15   // HP PA-RISC
#define EM_SPARC       2    // SPARC
#define EM_SPARCV9     43   // SPARC v9 64-bit
#define EM_OPENRISC    92   // OpenRISC
#define EM_BPF         247  // Linux BPF
#define EM_ALPHA       0x9026
// ... 共40+种架构
```

### 4.5 扩展编号机制

当 Program Header 数量超过或等于 65535（0xffff）时，使用扩展编号机制：

```c
#define PN_XNUM  0xffff
```

此时，`e_phnum` 字段设置为 `PN_XNUM`，真实的 Program Header 数量存储在 Section Header 表第 0 项的 `sh_info` 字段中。

对于 `e_shnum` 的超额，真实的 Section Header 数量则存储在 Section Header 表第 0 项的 `sh_size` 中。

如果 Program/Section Header 数量未超额，Section Header 表第 0 项应为零初始化（若存在）。

---

## 五、Program Header —— 程序头表

Program Header 描述的是**加载视图**，告诉内核如何将文件中的段映射到进程地址空间。

### 5.1 Elf32_Phdr / Elf64_Phdr 结构体

```c
typedef struct elf32_phdr {
    Elf32_Word p_type;    // 段类型
    Elf32_Off  p_offset;  // 在文件中的偏移
    Elf32_Addr p_vaddr;   // 虚拟地址
    Elf32_Addr p_paddr;   // 物理地址
    Elf32_Word p_filesz;  // 在文件中的大小
    Elf32_Word p_memsz;   // 在内存中的大小
    Elf32_Word p_flags;   // 权限标志
    Elf32_Word p_align;   // 对齐要求
} Elf32_Phdr;

typedef struct elf64_phdr {
    Elf64_Word  p_type;    // 段类型
    Elf64_Word  p_flags;   // 权限标志
    Elf64_Off   p_offset;  // 在文件中的偏移
    Elf64_Addr  p_vaddr;   // 虚拟地址
    Elf64_Addr  p_paddr;   // 物理地址
    Elf64_Xword p_filesz;  // 在文件中的大小
    Elf64_Xword p_memsz;   // 在内存中的大小
    Elf64_Xword p_align;   // 对齐要求
} Elf64_Phdr;
```

> 注意 64 位 Phdr 的字段顺序与 32 位不同：`p_flags` 被提到了 `p_offset` 前面。

### 5.2 p_type —— 段类型

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `PT_NULL` | 未使用 |
| 1 | `PT_LOAD` | 可加载段 |
| 2 | `PT_DYNAMIC` | 动态链接信息 |
| 3 | `PT_INTERP` | 解释器路径 |
| 4 | `PT_NOTE` | 附加信息 |
| 5 | `PT_SHLIB` | 保留 |
| 6 | `PT_PHDR` | 程序头表自身 |
| 7 | `PT_TLS` | 线程局部存储 |

**GNU 扩展类型**：

| 宏 | 含义 |
|-----|------|
| `PT_GNU_EH_FRAME` | 异常处理帧（`.eh_frame_hdr`） |
| `PT_GNU_STACK` | 栈是否可执行 |
| `PT_GNU_RELRO` | 重定位只读区域 |
| `PT_GNU_PROPERTY` | GNU 属性（如 CET、BTI） |

**范围定义**：

```c
#define PT_LOOS    0x60000000   // OS 特定段起始
#define PT_HIOS    0x6fffffff   // OS 特定段结束
#define PT_LOPROC  0x70000000   // 处理器特定段起始
#define PT_HIPROC  0x7fffffff   // 处理器特定段结束
```

### 5.3 p_flags —— 段权限

```c
#define PF_X  0x1   // 可执行
#define PF_W  0x2   // 可写
#define PF_R  0x4   // 可读
```

组合示例：

| 组合 | 含义 | 典型段 |
|------|------|--------|
| `PF_R + PF_X` (5) | 读+执行 | `.text` |
| `PF_R + PF_W` (6) | 读+写 | `.data` |
| `PF_R` (4) | 只读 | `.rodata` |

---

## 六、Section Header —— 节区头表

Section Header 描述的是**链接视图**，供编译器和链接器使用。

### 6.1 Elf32_Shdr / Elf64_Shdr 结构体

```c
typedef struct elf32_shdr {
    Elf32_Word sh_name;       // 节区名（字符串表索引）
    Elf32_Word sh_type;       // 节区类型
    Elf32_Word sh_flags;      // 节区标志
    Elf32_Addr sh_addr;       // 虚拟地址
    Elf32_Off  sh_offset;     // 文件偏移
    Elf32_Word sh_size;       // 节区大小
    Elf32_Word sh_link;       // 关联节区索引
    Elf32_Word sh_info;       // 额外信息
    Elf32_Word sh_addralign;  // 对齐要求
    Elf32_Word sh_entsize;    // 固定表项大小时的表项大小
} Elf32_Shdr;

typedef struct elf64_shdr {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;
```

### 6.2 sh_type —— 节区类型

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `SHT_NULL` | 无效节区 |
| 1 | `SHT_PROGBITS` | 程序数据（代码、数据等） |
| 2 | `SHT_SYMTAB` | 符号表 |
| 3 | `SHT_STRTAB` | 字符串表 |
| 4 | `SHT_RELA` | 带加数的重定位表 |
| 5 | `SHT_HASH` | 符号哈希表 |
| 6 | `SHT_DYNAMIC` | 动态链接信息 |
| 7 | `SHT_NOTE` | 注释信息 |
| 8 | `SHT_NOBITS` | 占位（不占文件空间，如 `.bss`） |
| 9 | `SHT_REL` | 不带加数的重定位表 |
| 10 | `SHT_SHLIB` | 保留 |
| 11 | `SHT_DYNSYM` | 动态符号表 |
| 12 | `SHT_NUM` | 已定义类型数 |

范围定义：

```c
#define SHT_LOPROC  0x70000000   // 处理器特定
#define SHT_HIPROC  0x7fffffff
#define SHT_LOUSER  0x80000000   // 用户自定义
#define SHT_HIUSER  0xffffffff
```

### 6.3 sh_flags —— 节区标志

```c
#define SHF_WRITE           0x1          // 可写
#define SHF_ALLOC           0x2          // 运行时占用内存
#define SHF_EXECINSTR       0x4          // 可执行指令
#define SHF_RELA_LIVEPATCH  0x00100000   // 内核热补丁相关
#define SHF_RO_AFTER_INIT   0x00200000   // 初始化后只读
#define SHF_MASKPROC        0xf0000000   // 处理器特定掩码
```

> `SHF_ALLOC` 是区分"链接时使用"和"运行时使用"的关键标志。没有此标志的节区在加载后不占内存。

### 6.4 特殊节区索引

```c
#define SHN_UNDEF       0       // 未定义/未关联
#define SHN_LORESERVE   0xff00  // 保留索引起始
#define SHN_LOPROC      0xff00  // 处理器特定起始
#define SHN_HIPROC      0xff1f  // 处理器特定结束
#define SHN_LIVEPATCH   0xff20  // 内核热补丁
#define SHN_ABS         0xfff1  // 绝对值（不受重定位影响）
#define SHN_COMMON      0xfff2  // 公共块
#define SHN_HIRESERVE   0xffff  // 保留索引结束
```

---

## 七、符号表 —— Symbol Table

符号表记录了文件中所有符号（函数、变量等）的名称、类型、绑定属性。

### 7.1 Elf32_Sym / Elf64_Sym 结构体

```c
typedef struct elf32_sym {
    Elf32_Word    st_name;   // 符号名（字符串表索引）
    Elf32_Addr    st_value;  // 符号值（地址/偏移）
    Elf32_Word    st_size;   // 符号大小
    unsigned char st_info;   // 类型 + 绑定属性
    unsigned char st_other;  // 可见性信息（低2位为STV_*标志）
    Elf32_Half    st_shndx;  // 所在节区索引
} Elf32_Sym;

typedef struct elf64_sym {
    Elf64_Word    st_name;
    unsigned char st_info;
    unsigned char st_other;  // 可见性信息（低2位为STV_*标志）
    Elf64_Half    st_shndx;
    Elf64_Addr    st_value;
    Elf64_Xword   st_size;
} Elf64_Sym;
```

> 64 位 Sym 的字段顺序与 32 位不同，`st_value` 和 `st_size` 被移到了最后。

### 7.2 st_info 解析

```c
#define ELF_ST_BIND(x)  ((x) >> 4)      // 高4位：绑定属性
#define ELF_ST_TYPE(x)  ((x) & 0xf)     // 低4位：符号类型
```

**绑定属性（STB_）**：

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `STB_LOCAL` | 局部符号（外部不可见） |
| 1 | `STB_GLOBAL` | 全局符号 |
| 2 | `STB_WEAK` | 弱符号（可被同名全局符号覆盖） |

**符号类型（STT_）**：

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `STT_NOTYPE` | 未指定类型 |
| 1 | `STT_OBJECT` | 数据对象（变量） |
| 2 | `STT_FUNC` | 函数 |
| 3 | `STT_SECTION` | 节区 |
| 4 | `STT_FILE` | 源文件名 |
| 5 | `STT_COMMON` | 公共块 |
| 6 | `STT_TLS` | 线程局部存储 |

---

## 八、重定位表 —— Relocation

重定位表告诉链接器或动态加载器如何修改代码/数据中的地址引用。

### 8.1 Rel（隐式加数）与 Rela（显式加数）

```c
// 不带显式加数的重定位项
typedef struct elf32_rel {
    Elf32_Addr r_offset;  // 需要重定位的位置
    Elf32_Word r_info;    // 符号索引 + 重定位类型
} Elf32_Rel;

typedef struct elf64_rel {
    Elf64_Addr  r_offset;
    Elf64_Xword r_info;
} Elf64_Rel;

// 带显式加数的重定位项
typedef struct elf32_rela {
    Elf32_Addr  r_offset;
    Elf32_Word  r_info;
    Elf32_Sword r_addend;   // 显式加数
} Elf32_Rela;

typedef struct elf64_rela {
    Elf64_Addr   r_offset;
    Elf64_Xword  r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;
```

### 8.2 r_info 解析

```c
// 32位
#define ELF32_R_SYM(x)  ((x) >> 8)      // 符号索引（高24位）
#define ELF32_R_TYPE(x) ((x) & 0xff)    // 重定位类型（低8位）

// 64位
#define ELF64_R_SYM(i)  ((i) >> 32)     // 符号索引（高32位）
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)  // 重定位类型（低32位）
```

> x86-64 下常见的重定位类型包括 `R_X86_64_PC32`（32位PC相对）、`R_X86_64_PLT32`（PLT调用）等，这些在各架构的 `asm/elf.h` 中定义。

---

## 九、动态段 —— Dynamic Section

`.dynamic` 段是动态链接的核心，包含动态链接器需要的信息。

### 9.1 Elf32_Dyn / Elf64_Dyn 结构体

```c
typedef struct {
    Elf32_Sword d_tag;
    union {
        Elf32_Sword d_val;
        Elf32_Addr  d_ptr;
    } d_un;
} Elf32_Dyn;

typedef struct {
    Elf64_Sxword d_tag;
    union {
        Elf64_Xword d_val;
        Elf64_Addr  d_ptr;
    } d_un;
} Elf64_Dyn;
```

每个 `d_tag` 表示一种信息类型，`d_un` 根据类型不同，解释为数值或地址。

### 9.2 常用 d_tag 取值

| 值 | 宏 | 含义 |
|----|-----|------|
| 0 | `DT_NULL` | 结束标记 |
| 1 | `DT_NEEDED` | 依赖的共享库名 |
| 2 | `DT_PLTRELSZ` | PLT 重定位总大小 |
| 3 | `DT_PLTGOT` | PLT/GOT 地址 |
| 4 | `DT_HASH` | 符号哈希表地址 |
| 5 | `DT_STRTAB` | 字符串表地址 |
| 6 | `DT_SYMTAB` | 符号表地址 |
| 7 | `DT_RELA` | Rela 重定位表地址 |
| 12 | `DT_INIT` | 初始化函数地址 |
| 13 | `DT_FINI` | 终止函数地址 |
| 14 | `DT_SONAME` | 共享库自身的 SONAME |
| 15 | `DT_RPATH` | 库搜索路径（已废弃） |
| 17 | `DT_REL` | Rel 重定位表地址 |
| 21 | `DT_DEBUG` | 调试信息（供 debugger 使用） |
| 22 | `DT_TEXTREL` | 代码段需要重定位 |
| 23 | `DT_JMPREL` | 仅 PLT 的重定位表 |

**范围定义**：

```c
#define DT_LOOS       0x6000000d   // OS 特定
#define DT_HIOS       0x6ffff000
#define DT_LOPROC     0x70000000   // 处理器特定
#define DT_HIPROC     0x7fffffff
```

---

## 十、ELF Note —— 注释段

Note 段用于在 ELF 文件中嵌入元数据，如 GNU 属性、内核版本信息、Core Dump 寄存器数据等。

### 10.1 Elf32_Nhdr / Elf64_Nhdr 结构体

```c
typedef struct elf32_note {
    Elf32_Word n_namesz;  // Name 长度（含 '\0'）
    Elf32_Word n_descsz;  // 描述数据长度
    Elf32_Word n_type;    // Note 类型
} Elf32_Nhdr;

typedef struct elf64_note {
    Elf64_Word n_namesz;
    Elf64_Word n_descsz;
    Elf64_Word n_type;
} Elf64_Nhdr;
```

Note 在文件中的布局为：

```
+----------+----------+----------+-----------+-----------+
| n_namesz | n_descsz | n_type   | name...   | desc...   |
+----------+----------+----------+-----------+-----------+
  4 bytes     4 bytes    4 bytes   变长(4对齐)  变长(4对齐)
```

> 32 位和 64 位的 Note Header 结构体大小完全相同——三个字段都是 `Elf32/64_Word`（4 字节），因此 Note Header 始终为 12 字节。

### 10.2 Note 类型（n_type）

**Core Dump 相关（Name="CORE" 或 "LINUX"）**：

```c
#define NT_PRSTATUS     1         // 进程状态（寄存器等）
#define NT_PRFPREG      2         // 浮点寄存器
#define NT_PRPSINFO     3         // 进程信息
#define NT_TASKSTRUCT   4         // 任务结构体
#define NT_AUXV         6         // 辅助向量
#define NT_SIGINFO      0x53494749
#define NT_FILE         0x46494c45
```

**GNU 属性（Name="GNU"）**：

```c
#define NT_GNU_PROPERTY_TYPE_0  5
```

### 10.3 内核中的 Note 生成宏

内核提供了便利宏来生成 Note，定义在 `include/linux/elfnote.h`：

```c
// C 代码中使用
ELFNOTE32("LINUX", NT_PRSTATUS, my_prstatus_data);
ELFNOTE64("GNU", NT_GNU_PROPERTY_TYPE_0, property_data);
```

---

## 十一、Core Dump 结构体

Linux 内核在生成 Core Dump 时，使用以下结构体来描述进程状态：

```c
struct elf_siginfo {
    int si_signo;   // 信号编号
    int si_code;    // 信号代码
    int si_errno;   // 错误号
};

struct elf_prstatus {
    struct elf_prstatus_common common;  // 通用进程信息
    elf_gregset_t pr_reg;               // 通用寄存器
    int pr_fpvalid;                     // 是否使用了数学协处理器
};

struct elf_prpsinfo {
    char  pr_state;                     // 进程状态
    char  pr_sname;                     // 状态缩写
    char  pr_zomb;                      // 僵尸标志
    char  pr_nice;                      // Nice 值
    unsigned long pr_flag;              // 标志
    __kernel_uid_t pr_uid;              // UID
    __kernel_gid_t pr_gid;              // GID
    pid_t pr_pid, pr_ppid, pr_pgrp, pr_sid;
    char  pr_fname[16];                 // 可执行文件名
    char  pr_psargs[ELF_PRARGSZ];       // 命令行参数
};
```

---

## 十二、内核 ELF 加载流程

`fs/binfmt_elf.c` 是内核加载 ELF 可执行文件的核心代码。

加载过程概述：

1. **验证魔数**：检查文件前 4 字节是否为 `\x7fELF`
2. **解析 ELF Header**：读取 `e_type`、`e_machine`、`e_entry` 等
3. **解析 Program Headers**：遍历所有 `PT_LOAD` 段
4. **映射段到内存**：通过 `elf_map()` 将段映射到用户空间
5. **加载解释器**（如为动态链接）：加载 `PT_INTERP` 指定的 `ld-linux.so`
6. **设置栈**：初始化辅助向量（AT_*）和环境变量
7. **跳转到入口点**：`e_entry` 或解释器入口

---

## 总结

| 概念 | 结构体 | 核心作用 |
|------|--------|----------|
| 文件头 | `Elf32/64_Ehdr` | 描述文件整体布局 |
| 程序头 | `Elf32/64_Phdr` | 加载视图，内存映射 |
| 节区头 | `Elf32/64_Shdr` | 链接视图，编译链接 |
| 符号表 | `Elf32/64_Sym` | 函数/变量符号信息 |
| 重定位 | `Elf32/64_Rel` / `Rela` | 地址修正信息 |
| 动态段 | `Elf32/64_Dyn` | 动态链接信息 |
| Note | `Elf32/64_Nhdr` | 元数据/调试信息 |

理解这些数据结构，是深入理解 Linux 程序加载、动态链接、调试等机制的基础。建议配合 `readelf` 和 `objdump` 工具，在实际的二进制文件上验证这些结构体。

---

## 实战：亲手解剖一个 ELF 文件

理论知识再多，不如亲手操刀解剖一次。下面我们写一个最简单的 Hello World 程序，然后用 `readelf` 把它的 ELF 结构一层层剥开。

### 12.1 准备样本

```c
// hello.c
#include <stdio.h>

int main(void)
{
    printf("Hello, ELF!\n");
    return 0;
}
```

编译（保留二进制，不剥离符号）：

```bash
gcc -o hello hello.c
```

### 12.2 ELF Header——第一眼

```bash
$ readelf -h hello
ELF Header:
  Magic:   7f 45 4c 46 02 01 01 00 00 00 00 00 00 00 00 00
  Class:                             ELF64
  Data:                              2's complement, little endian
  Version:                           1 (current)
  OS/ABI:                            UNIX - System V
  ABI Version:                       0
  Type:                              DYN (Position-Independent Executable file)
  Machine:                           Advanced Micro Devices X86-64
  Version:                           0x1
  Entry point address:               0x1060
  Start of program headers:          64 (bytes into file)
  Start of section headers:          13976 (bytes into file)
  Flags:                             0x0
  Size of this header:               64 (bytes)
  Size of program headers:           56 (bytes)
  Number of program headers:         13
  Size of section headers:           64 (bytes)
  Number of section headers:         31
  Section header string table index: 30
```

> **提示**：具体的地址和偏移值会因编译器版本、编译选项而异。本文实验环境为 GCC 14.2.1，你在自己机器上复现时数值可能略有不同，但结构完全一致。

逐一解读：

| 字段 | 值 | 含义 |
|------|-----|------|
| Magic | `7f 45 4c 46` | ELF 魔数，ASCII 即 `\x7fELF` |
| Class | ELF64 | 64 位文件（`ELFCLASS64` = 2） |
| Data | 小端 | x86 使用小端字节序（`ELFDATA2LSB` = 1） |
| Type | DYN | 动态链接的可执行文件（`ET_DYN` = 3）。现代 Linux 发行版默认编译为 PIE |
| Machine | X86-64 | AMD64 架构（`EM_X86_64` = 62） |
| Entry | 0x1060 | 入口地址，即 `_start` 的位置 |
| e_phoff | 64 | 程序头表紧跟在 ELF Header 后面（64 = sizeof(Elf64_Ehdr)） |
| e_phentsize | 56 | 每个 Phdr 56 字节，对应 `sizeof(Elf64_Phdr)` |
| e_phnum | 13 | 共 13 个段 |
| e_shentsize | 64 | 每个 Shdr 64 字节，对应 `sizeof(Elf64_Shdr)` |
| e_shnum | 31 | 共 31 个节区 |

### 12.3 Program Headers——内核视角

```bash
$ readelf -l hello

Elf file type is DYN (Position-Independent Executable file)
Entry point 0x1060
There are 13 program headers, starting at offset 64

Program Headers:
  Type           Offset             VirtAddr           PhysAddr
                 FileSiz            MemSiz              Flags  Align
  PHDR           0x0000000000000040 0x0000000000000040 0x0000000000000040
                 0x00000000000002d8 0x00000000000002d8  R      0x8
  INTERP         0x0000000000000318 0x0000000000000318 0x0000000000000318
                 0x000000000000001c 0x000000000000001c  R      0x1
      [Requesting program interpreter: /lib64/ld-linux-x86-64.so.2]
  LOAD           0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000628 0x0000000000000628  R      0x1000
  LOAD           0x0000000000001000 0x0000000000001000 0x0000000000001000
                 0x0000000000000175 0x0000000000000175  R E    0x1000
  LOAD           0x0000000000002000 0x0000000000002000 0x0000000000002000
                 0x00000000000000f4 0x00000000000000f4  R      0x1000
  LOAD           0x0000000000002db8 0x0000000000003db8 0x0000000000003db8
                 0x0000000000000258 0x0000000000000260  RW     0x1000
  DYNAMIC        0x0000000000002dc8 0x0000000000003dc8 0x0000000000003dc8
                 0x00000000000001f0 0x00000000000001f0  RW     0x8
  NOTE           0x0000000000000338 0x0000000000000338 0x0000000000000338
                 0x0000000000000030 0x0000000000000030  R      0x8
  GNU_STACK      0x0000000000000000 0x0000000000000000 0x0000000000000000
                 0x0000000000000000 0x0000000000000000  RW     0x10
  GNU_RELRO      0x0000000000002db8 0x0000000000003db8 0x0000000000003db8
                 0x0000000000000248 0x0000000000000248  R      0x1
  ...
```

关键发现：

- **PHDR**：程序头表本身，位于文件偏移 0x40（即紧跟 ELF Header 之后）
- **INTERP**：指定动态链接器 `/lib64/ld-linux-x86-64.so.2`
- **LOAD × 4**：四个加载段，分别是：
  - 只读段（R）：ELF Header + 只读数据，大小 0x628
  - 代码段（R E）：`.text` 等，地址 0x1000，大小 0x175
  - 只读数据段（R）：`.rodata`、`.eh_frame` 等，大小 0xf4
  - 读写段（RW）：`.data` + `.bss` + `.got`，注意 `MemSiz(0x260) > Filesz(0x258)`——多出的 8 字节就是 `.bss` 段在内存中的空间
- **GNU_STACK**：标记栈不可执行（`RW` 无 `E`），这是 NX 安全机制
- **GNU_RELRO**：标记重定位只读区域，加载后立即 `mprotect` 为只读，防止 GOT 覆写攻击

这里能直观看到前面文章讲的 **"FileSiz vs MemSiz"**——最后一个 LOAD 段的 `FileSiz=0x258`，`MemSiz=0x260`，多出的 8 字节就是 `.bss` 段在内存中的空间。

### 12.4 Section Headers——链接器视角

```bash
$ readelf -S hello
There are 31 section headers, starting at offset 0x3698:

Section Headers:
  [Nr] Name              Type             Address           Offset
       Size              EntSize          Flags  Link  Info  Align
  [ 0]                   NULL             0000000000000000  00000000
       0000000000000000  0000000000000000           0     0     0
  [ 1] .interp           PROGBITS         0000000000000318  00000318
       000000000000001c  0000000000000000   A       0     0     1
  [ 2] .note.gnu.pr[...] NOTE             0000000000000338  00000338
       0000000000000030  0000000000000000   A       0     0     8
  [ 3] .note.gnu.bu[...] NOTE             0000000000000368  00000368
       0000000000000024  0000000000000000   A       0     0     4
  [ 4] .note.ABI-tag     NOTE             000000000000038c  0000038c
       0000000000000020  0000000000000000   A       0     0     4
  [ 5] .gnu.hash         GNU_HASH         00000000000003b0  000003b0
       0000000000000024  0000000000000000   A       6     0     8
  [ 6] .dynsym           DYNSYM           00000000000003d8  000003d8
       00000000000000a8  0000000000000018   A       7     1     8
  [ 7] .dynstr           STRTAB           0000000000000480  00000480
       000000000000008d  0000000000000000   A       0     0     1
  ...
  [16] .text             PROGBITS         0000000000001060  00001060
       0000000000000107  0000000000000000  AX       0     0     16
  [17] .fini             PROGBITS         0000000000001168  00001168
       000000000000000d  0000000000000000  AX       0     0     4
  [18] .rodata           PROGBITS         0000000000002000  00002000
       0000000000000010  0000000000000000   A       0     0     4
  ...
  [25] .data             PROGBITS         0000000000004000  00003000
       0000000000000010  0000000000000000  WA       0     0     8
  [26] .bss              NOBITS           0000000000004010  00003010
       0000000000000008  0000000000000000  WA       0     0     1
  ...
```

对照本文第 4 章：

| 节区 | sh_type | 含义 |
|------|---------|------|
| `.interp` | PROGBITS(1) | 解释器路径字符串 |
| `.dynsym` | DYNSYM(11) | 动态符号表，`EntSize=24` = `sizeof(Elf64_Sym)` |
| `.dynstr` | STRTAB(3) | 动态字符串表 |
| `.text` | PROGBITS(1) | 代码段，Flags `AX` = 可分配+可执行 |
| `.rodata` | PROGBITS(1) | 只读数据（我们的字符串 `"Hello, ELF!\n"` 就在这里） |
| `.data` | PROGBITS(1) | 已初始化全局变量，Flags `WA` = 可写+可分配 |
| `.bss` | NOBITS(8) | 未初始化全局变量，不占文件空间 |

**亮点**：`.text` 的地址 0x1060 正是 ELF Header 中 `e_entry` 的值——入口点就落在 `.text` 段的起始位置。

### 12.5 符号表——函数和变量从哪来

```bash
$ readelf -s hello

Symbol table '.dynsym' contains 7 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
     0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND
     1: 0000000000000000     0 FUNC    GLOBAL DEFAULT  UND __libc_start[...]@GLIBC_2.34 (2)
     2: 0000000000000000     0 NOTYPE  WEAK   DEFAULT  UND _ITM_deregisterT[...]
     3: 0000000000000000     0 FUNC    GLOBAL DEFAULT  UND puts@GLIBC_2.2.5 (3)
     4: 0000000000000000     0 NOTYPE  WEAK   DEFAULT  UND __gmon_start__
     5: 0000000000000000     0 NOTYPE  WEAK   DEFAULT  UND _ITM_registerTMC[...]
     6: 0000000000000000     0 FUNC    WEAK   DEFAULT  UND __cxa_finalize@GLIBC_2.2.5 (3)

Symbol table '.symtab' contains 36 entries:
   Num:    Value          Size Type    Bind   Vis      Ndx Name
    29: 0000000000001060    38 FUNC    GLOBAL DEFAULT   16 _start
    31: 0000000000001149    30 FUNC    GLOBAL DEFAULT   16 main
    21: 0000000000000000     0 FUNC    GLOBAL DEFAULT  UND puts@GLIBC_2.2.5
```

解读：

- **`.dynsym`**（动态符号表）有 7 个条目，除第 0 个保留条目外全是 UND（未定义）——这些都是需要动态链接器解析的外部符号
- **`.symtab`**（完整符号表）有 36 个条目，其中 `main` 在索引 31（`.text` 节区），Value 为 0x1149，Size 为 30 字节；`_start` 在 0x1060，即 ELF 入口点
- 注意：`.dynsym` 中调用的是 `puts` 而非 `printf`——这是因为编译器发现我们的 `printf` 调用没有格式化参数，自动优化成了 `puts`。这正是编译器优化的一种体现
- `puts` 的 Ndx 为 UND——它定义在 libc.so 中，需要运行时解析

对照第 7 章的结构体：

```
Elf64_Sym 中：
  st_name  → 符号名字符串在 .dynstr/.strtab 中的偏移
  st_info  → 高 4 位 = STB_GLOBAL(1)，低 4 位 = STT_FUNC(2)
  st_value → main 的地址 = 0x1149
  st_size  → main 的大小 = 30 字节
  st_shndx → 16（.text 节区）
```

### 12.6 重定位表——地址怎么填

```bash
$ readelf -r hello

Relocation section '.rela.dyn' at offset 0x550 contains 8 entries:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000003db8  000000000008 R_X86_64_RELATIVE                    1140
000000003dc0  000000000008 R_X86_64_RELATIVE                    1100
000000004008  000000000008 R_X86_64_RELATIVE                    4008
000000003fd8  000100000006 R_X86_64_GLOB_DAT 0000000000000000 __libc_start_main@GLIBC_2.34 + 0
...

Relocation section '.rela.plt' at offset 0x610 contains 1 entry:
  Offset          Info           Type           Sym. Value    Sym. Name + Addend
000000003fd0  000300000007 R_X86_64_JUMP_SLO 0000000000000000 puts@GLIBC_2.2.5 + 0
```

关键发现：

- **`.rela.dyn`**：8 个重定位，包含 3 个相对重定位（`R_X86_64_RELATIVE`，运行时加基址即可）和 5 个 GLOB_DAT（全局数据符号，如 `__libc_start_main`）
- **`.rela.plt`**：PLT 重定位，只有 1 条——`puts`（编译器将 `printf("Hello, ELF!\n")` 优化成了 `puts`）。Offset `0x3fd0` 就是 GOT 表中 `puts` 条目的地址，动态链接器首次调用时填充真实地址

对照第 8 章 `Elf64_Rela` 结构体：

```
r_offset = 0x3fd0    → 需要修正的 GOT 条目地址
r_info   = 0x300000007
  → 低 32 位 = 7 = R_X86_64_JUMP_SLOT
  → 高 32 位 = 3 = .dynsym 中 puts 的符号索引
r_addend = 0          → 加数
```

### 12.7 动态段——运行时依赖

```bash
$ readelf -d hello

Dynamic section at offset 0x2dc8 contains 27 entries:
  Tag        Type                         Name/Value
 0x0000000000000001 (NEEDED)             Shared library: [libc.so.6]
 0x000000000000000c (INIT)               0x1000
 0x000000000000000d (FINI)               0x1168
 0x0000000000000019 (INIT_ARRAY)         0x3db8
 0x000000000000001b (INIT_ARRAYSZ)       8 (bytes)
 0x000000000000001a (FINI_ARRAY)         0x3dc0
 0x000000000000001c (FINI_ARRAYSZ)       8 (bytes)
 0x000000006ffffef5 (GNU_HASH)           0x3b0
 0x0000000000000005 (STRTAB)             0x480
 0x0000000000000006 (SYMTAB)             0x3d8
 0x000000000000000a (STRSZ)              141 (bytes)
 0x000000000000000b (SYMENT)             24 (bytes)
 0x0000000000000003 (PLTGOT)             0x3fb8
 0x0000000000000002 (PLTRELSZ)           24 (bytes)
 0x0000000000000014 (PLTREL)             RELA
 0x0000000000000017 (JMPREL)             0x610
 0x0000000000000007 (RELA)               0x550
 0x0000000000000008 (RELASZ)             192 (bytes)
 0x0000000000000009 (RELAENT)            24 (bytes)
 0x000000000000001e (FLAGS)              BIND_NOW
 0x000000006ffffffb (FLAGS_1)            Flags: NOW PIE
 0x000000006ffffffe (VERNEED)            0x520
 0x000000006fffffff (VERNEEDNUM)         1
 0x000000006ffffff0 (VERSYM)             0x50e
 0x000000006ffffff9 (RELACOUNT)          3
 0x0000000000000000 (NULL)               0x0
```

对照第 9 章的 `d_tag` 表格：

| Tag | 常量 | 值 | 含义 |
|-----|------|-----|------|
| NEEDED | `DT_NEEDED` = 1 | `libc.so.6` | 依赖的共享库 |
| STRTAB | `DT_STRTAB` = 5 | 0x480 | 动态字符串表地址 |
| SYMTAB | `DT_SYMTAB` = 6 | 0x3d8 | 动态符号表地址（即 `.dynsym`） |
| SYMENT | `DT_SYMENT` = 11 | 24 | 每个符号 24 字节 = `sizeof(Elf64_Sym)` |
| PLTGOT | `DT_PLTGOT` = 3 | 0x3fb8 | GOT 表地址 |
| JMPREL | `DT_JMPREL` = 23 | 0x610 | PLT 重定位表地址（即 `.rela.plt`） |

### 12.8 Note 段——ABI 标签

```bash
$ readelf -n hello

Displaying notes found in: .note.gnu.property
  Owner                Data size        Description
  GNU                  0x00000020       NT_GNU_PROPERTY_TYPE_0
      Properties: x86 feature: IBT, SHSTK
        x86 ISA needed: x86-64-baseline

Displaying notes found in: .note.gnu.build-id
  Owner                Data size        Description
  GNU                  0x00000014       NT_GNU_BUILD_ID
    Build ID: 8f8343d771373842a7bf84c89b49a203d239fb71

Displaying notes found in: .note.ABI-tag
  Owner                Data size        Description
  GNU                  0x00000010       NT_GNU_ABI_TAG
    OS: Linux, ABI: 3.2.0
```

对照第 10 章：

- `NT_GNU_PROPERTY_TYPE_0` = 5，包含 CPU 特性（Intel CET 的 IBT 和 SHSTK），以及 ISA 要求
- `NT_GNU_BUILD_ID` = 3，编译时生成的唯一标识（每次编译都不同，你的值会不一样）
- `NT_GNU_ABI_TAG` = 1，记录编译时的内核 ABI 版本（这里显示最低要求 Linux 3.2.0）
- Note Header 12 字节 + name（"GNU\0"，4 字节对齐）+ desc 数据

### 12.9 二进制层面的手动验证

用 `xxd` 直接查看 ELF Header 的前 64 字节：

```bash
$ xxd -l 64 hello
00000000: 7f45 4c46 0201 0100 0000 0000 0000 0000  .ELF............
00000010: 0300 3e00 0100 0000 6010 0000 0000 0000  ..>.....`.......
00000020: 4000 0000 0000 0000 9836 0000 0000 0000  @........6......
00000030: 0000 0000 4000 3800 0d00 4000 1f00 1e00  ....@.8...@.....
```

对照 `Elf64_Ehdr` 逐字节解析：

```
偏移  字节            字段              值
0x00  7f 45 4c 46     e_ident[EI_MAG]    ELF 魔数
0x04  02              e_ident[EI_CLASS]   ELFCLASS64
0x05  01              e_ident[EI_DATA]    ELFDATA2LSB（小端）
0x06  01              e_ident[EI_VERSION] EV_CURRENT
0x07  00              e_ident[EI_OSABI]   ELFOSABI_NONE
0x10  0300            e_type              ET_DYN (3)，小端表示
0x12  3e00            e_machine           EM_X86_64 (62 = 0x3E)
0x18  6010 0000 0000 0000  e_entry       0x1060
0x20  4000 0000 0000 0000  e_phoff       0x40 = 64
0x28  9836 0000 0000 0000  e_shoff       0x3698 = 13976
0x34  4000            e_ehsize           0x40 = 64
0x36  3800            e_phentsize        0x38 = 56
0x38  0d00            e_phnum            0x0D = 13
0x3A  4000            e_shentsize        0x40 = 64
0x3C  1f00            e_shnum            0x1F = 31
0x3E  1e00            e_shstrndx         0x1E = 30
```

### 12.10 完整结构映射图

把以上信息拼在一起，Hello World 程序的完整 ELF 布局就一目了然：

```
文件偏移         内存地址          内容                    所属 Segment   所属 Section
────────         ────────          ────                    ────────────   ───────────
0x0000 ┌────┐   0x0000 ┌────┐    ELF Header              LOAD(R)        (无)
0x0040 │    │           │    │    Program Header Table    PHDR+LOAD(R)   (无)
0x0318 │    │           │    │    ".interp" 字符串        INTERP+LOAD(R) .interp
0x0338 │    │           │    │    Note 段                 NOTE+LOAD(R)   .note.*
0x03b0 │    │           │    │    .gnu.hash               LOAD(R)        .gnu.hash
0x03d8 │    │           │    │    .dynsym                 LOAD(R)        .dynsym
0x0480 │    │           │    │    .dynstr                 LOAD(R)        .dynstr
       │    │           │    │    ... 更多只读数据 ...
0x1000 ├────┤   0x1000 ├────┤  ── LOAD(R E) 边界 ──
0x1060 │    │           │    │    _start / main 入口!     LOAD(R E)      .text
0x1168 │    │           │    │    _fini                   LOAD(R E)      .fini
0x2000 ├────┤   0x2000 ├────┤  ── LOAD(R) 边界 ──
0x2000 │    │           │    │    "Hello, ELF!\n"         LOAD(R)        .rodata
       │    │           │    │    ... 只读数据 ...
0x2db8 ├────┤   0x3db8 ├────┤  ── LOAD(RW) 边界 ──
0x2dc8 │    │           │    │    .dynamic                LOAD(RW)       .dynamic
0x2fb8 │    │           │    │    .got                    LOAD(RW)       .got
0x3000 │    │           │    │    .data                    LOAD(RW)       .data
0x3010 │    │           │    │    .bss (文件不占空间!)      LOAD(RW)       .bss
       └────┘           └────┘
                            │    │
                            │    │ 0x4018  ── 内存结束（.bss 之后）
```

> 注意：文件偏移和虚拟地址在第一个 LOAD 段中相同（p_offset = p_vaddr = 0），但从 RW 段开始两者不同——文件偏移 0x2db8 映射到虚拟地址 0x3db8。这正是 `p_offset` 和 `p_vaddr` 的区别。

### 12.11 小结

通过这个 Hello World 程序的完整解剖，我们把前面 11 章讲的所有结构体都"摸"了一遍：

1. `Elf64_Ehdr` 告诉我们入口在哪、程序头/节区头在哪
2. `Elf64_Phdr` 把文件映射成 4 个内存段，内核据此完成 `mmap`
3. `Elf64_Shdr` 组织了 31 个节区，链接器据此完成符号解析
4. `Elf64_Sym` 记录了 `main` 和 `puts` 的符号信息
5. `Elf64_Rela` 告诉动态链接器：`puts` 的地址需要填入 GOT
6. `Elf64_Dyn` 串联了动态链接的全部依赖
7. `Elf64_Nhdr` 记录了编译时的 ABI 属性

纸上得来终觉浅，绝知此事要躬行。建议你也用 `readelf` 对你手头的任意 ELF 文件做一次这样的解剖，你会发现那些看似枯燥的结构体定义，每一个字节都在真实地工作着。

---

*本文基于 Linux v6.14 内核源码编写，数据结构和常量定义均来自 `include/uapi/linux/elf.h` 及关联头文件。*
