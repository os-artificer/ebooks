/*
 * Copyright 2026 ebooks and courses
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * 方案 B：GOT/PLT 劫持（函数级热替换）
 *
 * 原理：
 *   动态链接的程序调用外部函数时，通过 GOT 间接跳转。
 *   在运行时找到 GOT 中目标函数的条目，把地址改成新函数
 *   的地址，后续所有调用自动跳到新函数。
 *
 *   本示例演示 GOT 劫持的核心流程：
 *   1. 通过 dladdr 获取目标函数地址信息
 *   2. 解析 ELF 的 .rela.plt 段定位 GOT 条目
 *   3. mprotect 修改 GOT 页权限并替换函数指针
 *
 * 编译：make got_hijack
 *   或：g++ -std=c++17 -O2 -Wall -Wextra -o got_hijack got_hijack.cpp -ldl
 * 运行：./got_hijack
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>

// ============================================================
// 目标函数（动态链接的外部函数，走 GOT/PLT）
// ============================================================
// 选用 libc 的 rand() 作为劫持目标。
// 调用 rand() 时通过 PLT → GOT 间接跳转，
// 修改 GOT 条目即可劫持。

// 新函数：替换 rand()，总是返回 42
extern "C" int patched_rand() {
    return 42;
}

// ============================================================
// 通过 dl_iterate_phdr + ELF 解析定位 GOT 条目
// ============================================================

// 传递给 dl_iterate_phdr 回调的上下文
struct GotSearchContext {
    const char* func_name;  // 要查找的函数名
    void**      got_entry;  // 输出：找到的 GOT 条目地址
    void*       base_addr;  // 输出：.so 的加载基址
};

// 辅助：解析动态段中的地址
// 对于 PIE 主程序，d_ptr 已经是绝对地址（动态链接器加载时重定位过）
// 对于 .so，d_ptr 是相对地址，需要加 dlpi_addr
// 判断方法：如果 d_ptr >= dlpi_addr 且 dlpi_addr != 0，
// 说明 d_ptr 已经是绝对地址
static uintptr_t resolve_dyn_addr(uintptr_t base,
                                   ElfW(Addr) d_ptr)
{
    if (base != 0 && d_ptr >= base) {
        return d_ptr;  // 已经是绝对地址
    }
    return base + d_ptr;  // 相对地址，需要加基址
}

// dl_iterate_phdr 回调：遍历每个已加载的共享对象
static int phdr_callback(struct dl_phdr_info* info,
                         size_t /*size*/, void* data)
{
    auto* ctx = static_cast<GotSearchContext*>(data);

    // dl_phdr_info 没有 dlpi_dynamic 成员
    // 需要遍历 dlpi_phdr 数组找到 PT_DYNAMIC 段
    const ElfW(Dyn)* dyn = nullptr;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
        if (phdr->p_type == PT_DYNAMIC) {
            dyn = reinterpret_cast<const ElfW(Dyn)*>(
                info->dlpi_addr + phdr->p_vaddr);
            break;
        }
    }
    if (!dyn) return 0;

    // 从动态段中提取所需段的地址
    // DT_JMPREL: .rela.plt 段（函数重定位表）
    // DT_PLTRELSZ: .rela.plt 的大小
    // DT_SYMTAB: 符号表
    // DT_STRTAB: 字符串表
    const ElfW(Rela)* jmprel = nullptr;
    size_t pltrelsz = 0;
    const ElfW(Sym)* symtab = nullptr;
    const char* strtab = nullptr;

    for (const ElfW(Dyn)* d = dyn; d->d_tag != DT_NULL; ++d) {
        uintptr_t addr = resolve_dyn_addr(
            info->dlpi_addr, d->d_un.d_ptr);
        switch (d->d_tag) {
            case DT_JMPREL:
                jmprel = reinterpret_cast<const ElfW(Rela)*>(addr);
                break;
            case DT_PLTRELSZ:
                pltrelsz = d->d_un.d_val;
                break;
            case DT_SYMTAB:
                symtab = reinterpret_cast<const ElfW(Sym)*>(addr);
                break;
            case DT_STRTAB:
                strtab = reinterpret_cast<const char*>(addr);
                break;
        }
    }

    if (!jmprel || !symtab || !strtab || pltrelsz == 0)
        return 0;

    // 遍历 .rela.plt 中的每个重定位项
    size_t count = pltrelsz / sizeof(ElfW(Rela));
    for (size_t i = 0; i < count; ++i) {
        const ElfW(Rela)* rel = &jmprel[i];

        // 从重定位项中提取符号索引
        // ELF64_R_SYM: 从 r_info 中提取符号表索引
        ElfW(Word) sym_idx = ELF64_R_SYM(rel->r_info);

        // 从符号表中获取符号名
        const char* sym_name = strtab + symtab[sym_idx].st_name;

        // 匹配目标函数名
        if (strcmp(sym_name, ctx->func_name) == 0) {
            // r_offset 对于 PIE 主程序是绝对地址，对于 .so 是相对地址
            // 用 resolve_dyn_addr 统一处理
            uintptr_t got_addr = resolve_dyn_addr(
                info->dlpi_addr, rel->r_offset);
            ctx->got_entry = reinterpret_cast<void**>(got_addr);
            ctx->base_addr = reinterpret_cast<void*>(info->dlpi_addr);
            return 1;  // 找到了，停止遍历
        }
    }

    return 0;  // 继续遍历下一个 .so
}

// 查找指定函数在 GOT 中的条目地址
static void* find_got_entry(const char* func_name) {
    GotSearchContext ctx;
    ctx.func_name = func_name;
    ctx.got_entry = nullptr;
    ctx.base_addr = nullptr;

    dl_iterate_phdr(phdr_callback, &ctx);
    return ctx.got_entry;
}

// ============================================================
// 修改 GOT 条目，将旧函数地址替换为新函数地址
// ============================================================
static bool patch_got(void** got_entry, void* new_func_addr) {
    // 获取内存页大小（通常 4096）
    long page_size = sysconf(_SC_PAGESIZE);

    // 将 got_entry 向下对齐到页边界
    // mprotect 以页为单位操作权限
    uintptr_t addr = reinterpret_cast<uintptr_t>(got_entry);
    void* page = reinterpret_cast<void*>(
        addr & ~(page_size - 1));

    // GOT 所在页可能只读（尤其开了 RELRO 时）
    // 需要先改为可写
    if (mprotect(page, page_size,
                 PROT_READ | PROT_WRITE) != 0) {
        std::cerr << "mprotect(RW) 失败\n";
        return false;
    }

    // 备份旧地址（用于回滚和验证）
    void* old_addr = *got_entry;
    std::cout << "  旧 GOT 条目值: " << old_addr << "\n";

    // 写入新函数地址
    // 后续所有通过 PLT 的调用都会跳转到新函数
    *got_entry = new_func_addr;

    std::cout << "  新 GOT 条目值: " << new_func_addr << "\n";

    // 恢复只读权限
    // 注意：实际工程应还原原始权限，此处简化
    mprotect(page, page_size, PROT_READ);

    return true;
}

// ============================================================
// 辅助：确保 rand() 走 PLT（而非被内联）
// ============================================================
// 声明为外部函数，防止编译器内联优化
extern "C" int rand(void) noexcept __attribute__((noinline));

int main() {
    std::cout << "=== 方案 B：GOT/PLT 劫持 ===\n\n";

    // 步骤 1：劫持前调用 rand()
    // 编译时必须用 -O0 或确保 rand() 不被内联
    int v1 = rand();
    std::cout << "[1] 劫持前: rand() = " << v1 << "\n";
    int v2 = rand();
    std::cout << "    rand() = " << v2 << "\n";

    // 步骤 2：查找 rand() 在 GOT 中的条目
    std::cout << "\n[2] 查找 rand() 的 GOT 条目 ...\n";
    void** got_entry = static_cast<void**>(
        find_got_entry("rand"));
    if (!got_entry) {
        std::cerr << "未找到 rand() 的 GOT 条目\n";
        std::cerr << "（请确保用 -O0 编译，或 rand() 来自动态链接）\n";
        return 1;
    }
    std::cout << "  GOT 条目地址: " << got_entry << "\n";

    // 步骤 3：劫持 GOT
    std::cout << "\n[3] 劫持 GOT：rand → patched_rand ...\n";
    if (!patch_got(got_entry,
                   reinterpret_cast<void*>(patched_rand))) {
        return 1;
    }

    // 步骤 4：劫持后调用 rand()
    // 现在所有 rand() 调用都会跳转到 patched_rand()，返回 42
    std::cout << "\n[4] 劫持后: rand() = " << rand() << "\n";
    std::cout << "    rand() = " << rand() << "\n";
    std::cout << "    rand() = " << rand() << "\n";

    std::cout << "\n（patched_rand 始终返回 42）\n";
    std::cout << "\n完成。\n";
    return 0;
}
