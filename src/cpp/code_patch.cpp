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
 * 方案 C：代码段直接补丁（mprotect + 机器码改写）
 *
 * 原理：
 *   直接修改进程代码段中的机器指令。把目标函数的前几条
 *   指令改成一个跳转（jmp [rip+0]），跳到新函数的地址。
 *
 *   x86-64 的 jmp [rip+0] 编码为 FF 25 00 00 00 00，
 *   后跟 8 字节绝对地址，共 14 字节。
 *
 *   与 kpatch / livepatch 内核热补丁同源，
 *   区别仅在内核空间 vs 用户空间。
 *
 * 编译：make code_patch
 *   或：g++ -std=c++17 -O0 -Wall -Wextra -o code_patch code_patch.cpp
 * 注意：必须用 -O0 编译，防止函数被内联或优化掉
 * 运行：./code_patch
 */

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <sys/mman.h>
#include <unistd.h>

// ============================================================
// 目标函数：补丁前返回 100
// ============================================================
// noinline 防止编译器内联，确保有独立函数体
// 必须用 -O0 编译，否则函数体可能太短（不足 14 字节）
__attribute__((noinline))
int target_func() {
    return 100;
}

// ============================================================
// 新函数：补丁后返回 999
// ============================================================
__attribute__((noinline))
int patched_func() {
    return 999;
}

// ============================================================
// 备份原始指令（用于回滚）
// ============================================================
// 14 字节 = 6 字节 jmp 指令 + 8 字节地址
static constexpr int kPatchSize = 14;
static uint8_t g_backup[kPatchSize];
static bool g_backed_up = false;

// ============================================================
// patch_function：用跳转指令覆盖目标函数入口
// ============================================================
// 构造 x86-64 的 jmp [rip+0] 指令（共 14 字节）：
//   字节 0-1:   FF 25           → jmp [rip+disp32] 操作码
//   字节 2-5:   00 00 00 00     → disp32=0，地址紧跟其后
//   字节 6-13:  新函数地址       → 8 字节绝对地址
static bool patch_function(void* old_func, void* new_func) {
    long page_size = sysconf(_SC_PAGESIZE);

    // 将函数地址向下对齐到页边界
    // mprotect 以页为单位
    uintptr_t addr = reinterpret_cast<uintptr_t>(old_func);
    void* page = reinterpret_cast<void*>(
        addr & ~(page_size - 1));

    // 代码段默认权限 PROT_READ | PROT_EXEC，不可写
    // 必须先通过 mprotect 添加 PROT_WRITE 权限
    if (mprotect(page, page_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        std::cerr << "mprotect(RWX) 失败\n";
        return false;
    }

    // 备份原始指令（回滚时需要）
    if (!g_backed_up) {
        memcpy(g_backup, old_func, kPatchSize);
        g_backed_up = true;
    }

    // 构造跳转指令——使用字节数组而非结构体
    // 避免内存对齐 padding 破坏指令编码
    uint8_t patch[kPatchSize] = {
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // jmp [rip+0]
        0, 0, 0, 0, 0, 0, 0, 0               // 占位：新函数地址
    };

    // 写入新函数地址到字节 6-13
    uint64_t target = reinterpret_cast<uint64_t>(new_func);
    memcpy(patch + 6, &target, sizeof(target));

    // 用跳转指令覆盖目标函数入口的前 14 字节
    memcpy(old_func, patch, kPatchSize);

    // 恢复代码段为只读+可执行权限
    mprotect(page, page_size, PROT_READ | PROT_EXEC);

    return true;
}

// ============================================================
// unpatch_function：回滚，恢复原始指令
// ============================================================
static bool unpatch_function(void* old_func) {
    if (!g_backed_up) {
        std::cerr << "未备份原始指令，无法回滚\n";
        return false;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t addr = reinterpret_cast<uintptr_t>(old_func);
    void* page = reinterpret_cast<void*>(
        addr & ~(page_size - 1));

    if (mprotect(page, page_size,
                 PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        std::cerr << "mprotect(RWX) 失败\n";
        return false;
    }

    // 把备份的原始指令写回
    memcpy(old_func, g_backup, kPatchSize);

    mprotect(page, page_size, PROT_READ | PROT_EXEC);
    return true;
}

int main() {
    std::cout << "=== 方案 C：代码段直接补丁 ===\n\n";

    // 步骤 1：补丁前调用
    std::cout << "[1] 补丁前: target_func() = "
              << target_func() << "\n";
    std::cout << "    target_func() = "
              << target_func() << "\n";

    // 步骤 2：打补丁
    std::cout << "\n[2] 打补丁：target_func → patched_func ...\n";
    if (!patch_function(
            reinterpret_cast<void*>(target_func),
            reinterpret_cast<void*>(patched_func))) {
        return 1;
    }
    std::cout << "  补丁已写入（14 字节 jmp 指令）\n";

    // 步骤 3：补丁后调用
    // 现在所有 target_func() 调用都会跳转到 patched_func()
    std::cout << "\n[3] 补丁后: target_func() = "
              << target_func() << "\n";
    std::cout << "    target_func() = "
              << target_func() << "\n";

    // 步骤 4：回滚
    std::cout << "\n[4] 回滚：恢复原始指令 ...\n";
    if (!unpatch_function(
            reinterpret_cast<void*>(target_func))) {
        return 1;
    }
    std::cout << "  原始指令已恢复\n";

    // 步骤 5：回滚后调用
    std::cout << "\n[5] 回滚后: target_func() = "
              << target_func() << "\n";
    std::cout << "    target_func() = "
              << target_func() << "\n";

    std::cout << "\n完成。\n";
    return 0;
}
