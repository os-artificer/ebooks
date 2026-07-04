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
 * 方案 D：信号驱动的模块替换
 *
 * 原理：
 *   方案 A 的变体，用信号（SIGUSR1 等）作为触发器。
 *   进程在信号处理器中标记"需要更新"，主循环中检查标记
 *   并执行 dlclose + dlopen + 状态恢复。
 *
 *   信号处理器中只能调用 async-signal-safe 函数，
 *   dlopen/malloc/cout 均不在此列，因此只设置原子标志，
 *   主循环在安全同步点检查标志后执行实际更新。
 *
 * 编译：make signal_reload
 *   或：g++ -std=c++17 -O2 -Wall -Wextra -o signal_reload \
 *        signal_reload.cpp -ldl
 * 运行：./signal_reload
 *   另开终端：kill -USR1 <pid>
 */

#include <atomic>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <dlfcn.h>
#include <unistd.h>

// ============================================================
// 热更新模块的统一接口
// ============================================================
class HotModule {
public:
    virtual ~HotModule() = default;
    virtual std::string process(const std::string& input) = 0;
    virtual int version() const = 0;
};

using CreateFn  = HotModule* (*)();
using DestroyFn = void (*)(HotModule*);

// ============================================================
// 模块加载器
// ============================================================
class ModuleLoader {
    void* _handle = nullptr;
    HotModule* _module = nullptr;
    CreateFn  _create  = nullptr;
    DestroyFn _destroy = nullptr;

public:
    ~ModuleLoader() { unload(); }

    bool load(const char* path) {
        if (_handle) unload();

        _handle = dlopen(path, RTLD_NOW);
        if (!_handle) {
            std::cerr << "dlopen: " << dlerror() << "\n";
            return false;
        }

        dlerror();
        _create = reinterpret_cast<CreateFn>(
            dlsym(_handle, "create_module"));
        _destroy = reinterpret_cast<DestroyFn>(
            dlsym(_handle, "destroy_module"));
        if (!_create || !_destroy) {
            std::cerr << "dlsym: " << dlerror() << "\n";
            dlclose(_handle);
            _handle = nullptr;
            return false;
        }

        _module = _create();
        return true;
    }

    void unload() {
        if (_module) { _destroy(_module); _module = nullptr; }
        if (_handle) { dlclose(_handle); _handle = nullptr; }
        _create = nullptr;
        _destroy = nullptr;
    }

    HotModule* module() const { return _module; }
};

// ============================================================
// 内嵌的 v1/v2 模块源码
// ============================================================
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
extern "C" HotModule* create_module() { return new HotModuleV1(); }
extern "C" void destroy_module(HotModule* p) { delete p; }
)SRC";

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
extern "C" HotModule* create_module() { return new HotModuleV2(); }
extern "C" void destroy_module(HotModule* p) { delete p; }
)SRC";

// 将内嵌源码编译为 .so
static bool build_so(const char* src, const char* cpp_name,
                     const char* so_name)
{
    FILE* f = fopen(cpp_name, "w");
    if (!f) return false;
    fputs(src, f);
    fclose(f);

    std::string cmd = "g++ -shared -fPIC -o ";
    cmd += so_name;
    cmd += " ";
    cmd += cpp_name;
    cmd += " 2>&1";
    return system(cmd.c_str()) == 0;
}

// ============================================================
// 全局更新器（信号处理器需要访问）
// ============================================================
class HotUpdater {
    // 原子标志：信号处理器写 true，主循环读并清零
    // lock-free 的 atomic 操作在 Linux + GCC/Clang 上
    // 是 async-signal-safe 的
    std::atomic<bool> _pending_update{false};
    ModuleLoader _loader;
    std::string _next_so;

public:
    bool init(const char* so_path) {
        _next_so = so_path;
        return _loader.load(so_path);
    }

    // 信号处理器调用此函数——仅设置标志
    void request_update(const char* so_path) {
        _next_so = so_path;
        _pending_update.store(true);
    }

    // 在主循环的安全同步点检查标志并执行实际热更新
    void check_update() {
        // exchange(false)：原子读取旧值并设为 false
        // 返回旧值——如果旧值为 true 表示有待处理的更新
        if (!_pending_update.exchange(false))
            return;

        std::cout << "\n[hot-update] 收到更新信号，"
                     "正在重新加载模块 ...\n";
        if (_loader.load(_next_so.c_str())) {
            std::cout << "[hot-update] 新版本: "
                      << _loader.module()->version() << "\n\n";
        } else {
            std::cerr << "[hot-update] 加载失败\n\n";
        }
    }

    HotModule* module() const { return _loader.module(); }
};

static HotUpdater* g_updater = nullptr;

// ============================================================
// SIGUSR1 信号处理器——只做标记
// ============================================================
// 信号处理器中只能调用 async-signal-safe 函数
// dlopen、malloc、cout 均不在此列，因此只设置原子标志
static void sigusr1_handler(int) {
    if (g_updater) {
        g_updater->request_update("./module_v2.so");
    }
}

int main() {
    std::cout << "=== 方案 D：信号驱动的模块替换 ===\n\n";

    // 编译内嵌模块
    std::cout << "[1] 编译 module_v1.so ...\n";
    if (!build_so(kModuleV1Src, "module_v1.cpp", "module_v1.so")) {
        std::cerr << "编译 v1 失败\n";
        return 1;
    }
    std::cout << "[2] 编译 module_v2.so ...\n";
    if (!build_so(kModuleV2Src, "module_v2.cpp", "module_v2.so")) {
        std::cerr << "编译 v2 失败\n";
        return 1;
    }

    // 初始化加载 v1
    HotUpdater updater;
    g_updater = &updater;
    std::cout << "\n[3] 加载 module_v1.so ...\n";
    if (!updater.init("./module_v1.so")) {
        return 1;
    }
    std::cout << "当前版本: "
              << updater.module()->version() << "\n";

    // 注册信号处理器
    struct sigaction sa{};
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    // 被信号中断的系统调用自动重启
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, nullptr);

    std::cout << "\n[4] 主循环运行中 ...\n";
    std::cout << "    PID = " << getpid() << "\n";
    std::cout << "    发送信号触发热更新: "
                 "kill -USR1 " << getpid() << "\n";
    std::cout << "    5 秒后自动触发热更新\n";
    std::cout << "    按 Ctrl+C 退出\n\n";

    // 主循环：每次迭代检查热更新标记
    int tick = 0;
    bool auto_triggered = false;
    bool updated_done = false;
    while (true) {
        updater.check_update();

        if (updater.module()) {
            std::cout << "  [tick " << tick << "] "
                      << updater.module()->process("hello")
                      << "\n";
        }

        // 3 秒后自动触发热更新（方便演示）
        if (!auto_triggered && tick >= 3) {
            std::cout << "\n  >>> 自动触发热更新 <<<\n";
            updater.request_update("./module_v2.so");
            auto_triggered = true;
        }

        // 热更新完成后再跑 2 个 tick 就退出
        if (auto_triggered && updater.module() &&
            updater.module()->version() == 2) {
            if (updated_done) {
                std::cout << "\n演示完成，退出。\n";
                // 清理临时文件
                unlink("module_v1.so");
                unlink("module_v2.so");
                unlink("module_v1.cpp");
                unlink("module_v2.cpp");
                return 0;
            }
            updated_done = true;
        }

        ++tick;
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    return 0;
}
