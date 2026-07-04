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
 * 方案 A：动态库加载替换（dlopen + dlsym）
 *
 * 原理：
 *   将需要热更新的模块编译成独立的 .so 文件。主程序通过 dlopen
 *   加载该 .so，用 dlsym 查找工厂函数并调用。需要更新时，
 *   dlclose 卸载旧 .so，再 dlopen 加载新 .so。
 *
 *   为保证本示例可独立编译为单个二进制，v1/v2 模块源码以内嵌
 *   字符串形式存放，运行时先写出为 .cpp 再调用 g++ 编译为 .so。
 *
 * 编译：make dynamic_loader
 *   或：g++ -std=c++17 -O2 -Wall -Wextra -o dynamic_loader dynamic_loader.cpp -ldl
 * 运行：./dynamic_loader
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <fstream>
#include <dlfcn.h>
#include <unistd.h>

// ============================================================
// 热更新模块的统一接口（主程序与 .so 共享此抽象基类）
// ============================================================
// 主程序只依赖此抽象接口类，不依赖任何具体实现。
// 新旧 .so 各自提供实现，通过工厂函数创建实例，
// 主程序通过虚函数表（vtable）分发调用。
class HotModule {
public:
    virtual ~HotModule() = default;
    virtual std::string process(const std::string& input) = 0;
    virtual int version() const = 0;
};

// 工厂函数类型别名
// 对应 .so 中的 create_module / destroy_module
using CreateFn  = HotModule* (*)();
using DestroyFn = void (*)(HotModule*);

// ============================================================
// 模块加载器：封装 dlopen/dlsym/dlclose
// ============================================================
class ModuleLoader {
    void* _handle = nullptr;
    HotModule* _module = nullptr;
    CreateFn  _create  = nullptr;
    DestroyFn _destroy = nullptr;

public:
    ~ModuleLoader() { unload(); }

    bool load(const char* path) {
        // 如果已有旧模块，先卸载
        if (_handle) unload();

        // RTLD_NOW: 立即解析所有符号（而非延迟）
        // 尽早发现符号缺失问题，避免运行时崩溃
        _handle = dlopen(path, RTLD_NOW);
        if (!_handle) {
            std::cerr << "dlopen: " << dlerror() << "\n";
            return false;
        }

        // 清空残留错误标记
        // dlsym 无法区分"找不到符号"和"符号恰为 NULL"
        dlerror();

        // 查找工厂函数地址
        _create = reinterpret_cast<CreateFn>(
            dlsym(_handle, "create_module"));
        // 查找销毁函数地址
        _destroy = reinterpret_cast<DestroyFn>(
            dlsym(_handle, "destroy_module"));
        if (!_create || !_destroy) {
            std::cerr << "dlsym: " << dlerror() << "\n";
            dlclose(_handle);
            _handle = nullptr;
            return false;
        }

        // 调用工厂函数，创建模块实例
        _module = _create();
        return true;
    }

    void unload() {
        // 顺序很重要：先销毁模块实例，再关闭 .so 句柄
        // 否则 destroy_module 的代码已被卸载，调用会段错误
        if (_module) {
            _destroy(_module);
            _module = nullptr;
        }
        if (_handle) {
            dlclose(_handle);
            _handle = nullptr;
        }
        _create  = nullptr;
        _destroy = nullptr;
    }

    HotModule* module() const { return _module; }
};

// ============================================================
// 内嵌的 v1 模块源码（编译为 module_v1.so）
// ============================================================
// extern "C" 防止 C++ name mangling，
// 确保 dlsym 能按原名查找 create_module / destroy_module
static const char* kModuleV1Src = R"SRC(
#include <string>

class HotModule {
public:
    virtual ~HotModule() = default;
    virtual std::string process(const std::string& input) = 0;
    virtual int version() const = 0;
};

class HotModuleV1 : public HotModule {
public:
    std::string process(const std::string& input) override {
        return "v1: " + input;
    }
    int version() const override { return 1; }
};

extern "C" HotModule* create_module() {
    return new HotModuleV1();
}
extern "C" void destroy_module(HotModule* p) {
    delete p;
}
)SRC";

// ============================================================
// 内嵌的 v2 模块源码（编译为 module_v2.so）
// ============================================================
static const char* kModuleV2Src = R"SRC(
#include <string>

class HotModule {
public:
    virtual ~HotModule() = default;
    virtual std::string process(const std::string& input) = 0;
    virtual int version() const = 0;
};

class HotModuleV2 : public HotModule {
public:
    std::string process(const std::string& input) override {
        return "v2: " + input;
    }
    int version() const override { return 2; }
};

extern "C" HotModule* create_module() {
    return new HotModuleV2();
}
extern "C" void destroy_module(HotModule* p) {
    delete p;
}
)SRC";

// ============================================================
// 辅助函数：将内嵌源码写出为 .cpp 并编译为 .so
// ============================================================
static bool build_so(const char* src, const char* cpp_name,
                     const char* so_name)
{
    // 写出源码文件
    std::ofstream ofs(cpp_name);
    if (!ofs) {
        std::cerr << "无法写入 " << cpp_name << "\n";
        return false;
    }
    ofs << src;
    ofs.close();

    // 调用 g++ 编译为 .so
    // -shared: 生成共享库
    // -fPIC: 生成位置无关代码（.so 必需）
    std::string cmd = "g++ -shared -fPIC -o ";
    cmd += so_name;
    cmd += " ";
    cmd += cpp_name;
    cmd += " 2>&1";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "popen 失败\n";
        return false;
    }
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) {
        std::cerr << "  " << buf;
    }
    int rc = pclose(pipe);
    if (rc != 0) {
        std::cerr << "编译 " << so_name << " 失败\n";
        return false;
    }
    return true;
}

int main() {
    std::cout << "=== 方案 A：动态库加载替换 ===\n\n";

    // 步骤 1：将内嵌源码编译为 .so
    std::cout << "[1] 编译 module_v1.so ...\n";
    if (!build_so(kModuleV1Src, "module_v1.cpp", "module_v1.so"))
        return 1;

    std::cout << "[2] 编译 module_v2.so ...\n";
    if (!build_so(kModuleV2Src, "module_v2.cpp", "module_v2.so"))
        return 1;

    // 步骤 2：加载 v1 模块
    ModuleLoader loader;
    std::cout << "\n[3] 加载 module_v1.so ...\n";
    if (!loader.load("./module_v1.so")) {
        std::cerr << "加载 v1 失败\n";
        return 1;
    }

    std::cout << "process: "
              << loader.module()->process("hello") << "\n";
    std::cout << "version: "
              << loader.module()->version() << "\n";

    // 步骤 3：热更新——替换为 v2 模块
    std::cout << "\n[4] 热更新：替换为 module_v2.so ...\n";
    if (!loader.load("./module_v2.so")) {
        std::cerr << "加载 v2 失败\n";
        return 1;
    }

    std::cout << "process: "
              << loader.module()->process("hello") << "\n";
    std::cout << "version: "
              << loader.module()->version() << "\n";

    // 步骤 4：清理
    std::cout << "\n[5] 清理临时文件 ...\n";
    loader.unload();
    unlink("module_v1.so");
    unlink("module_v2.so");
    unlink("module_v1.cpp");
    unlink("module_v2.cpp");

    std::cout << "\n完成。\n";
    return 0;
}
