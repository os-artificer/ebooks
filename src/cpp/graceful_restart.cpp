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
 * 方案 E：进程级热替换（优雅重启）
 *
 * 原理：
 *   不修改单个函数或模块，而是启动一个新进程，把旧进程
 *   的状态迁移过去，然后让旧进程优雅退出。
 *
 *   旧进程退出前把状态写入 POSIX 共享内存，
 *   新进程启动后从共享内存读取，实现状态"接力"。
 *   共享内存在创建进程退出后仍然存活，
 *   直到被 shm_unlink 显式删除。
 *
 * 编译：make graceful_restart
 *   或：g++ -std=c++17 -O2 -Wall -Wextra -o graceful_restart \
 *        graceful_restart.cpp -lrt
 * 运行：./graceful_restart
 */

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

// ============================================================
// 需要在新旧进程间迁移的运行状态
// ============================================================
struct SharedState {
    int      session_count;    // 当前活跃会话数
    long     total_processed;  // 累计处理的请求数
    int      generation;       // 进程代数（第几代）
    char     message[256];     // 自定义消息
};

// 共享内存名称（/dev/shm/ 下的路径）
static const char* kShmName = "/hot_update_state";

// ============================================================
// 旧进程调用：将状态保存到 POSIX 共享内存
// ============================================================
static bool save_state_to_shm(const SharedState& state) {
    // shm_open 创建/打开一个命名共享内存对象
    // O_CREAT: 不存在则创建
    // O_RDWR: 可读写
    // 0666: 所有用户可读写
    int fd = shm_open(kShmName, O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        perror("shm_open");
        return false;
    }

    // 设置共享内存大小
    // 新创建的 shm 对象大小为 0，必须 ftruncate 才能写入
    if (ftruncate(fd, sizeof(SharedState)) != 0) {
        perror("ftruncate");
        close(fd);
        return false;
    }

    // 将共享内存映射到本进程地址空间
    // MAP_SHARED: 修改写回底层对象（其他映射者也可见）
    // PROT_WRITE: 可写入
    void* ptr = mmap(nullptr, sizeof(SharedState),
                     PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return false;
    }

    // 将状态拷贝到共享内存
    memcpy(ptr, &state, sizeof(SharedState));

    // 解除映射并关闭 fd
    // 共享内存对象本身仍然存活，供新进程读取
    munmap(ptr, sizeof(SharedState));
    close(fd);
    return true;
}

// ============================================================
// 新进程调用：从 POSIX 共享内存恢复状态
// ============================================================
static bool load_state_from_shm(SharedState& state) {
    // 默认初始化，防止读取失败时返回垃圾值
    memset(&state, 0, sizeof(state));

    // 打开已存在的共享内存对象
    // O_RDONLY 只读即可，不需 O_CREAT
    // mode 参数在未指定 O_CREAT 时被忽略，传 0
    int fd = shm_open(kShmName, O_RDONLY, 0);
    if (fd < 0) {
        // 没有共享状态，以全新状态启动
        return false;
    }

    // 映射为只读
    void* ptr = mmap(nullptr, sizeof(SharedState),
                     PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return false;
    }

    // 从共享内存拷贝状态到本地变量
    memcpy(&state, ptr, sizeof(SharedState));

    // 解除映射并关闭 fd
    munmap(ptr, sizeof(SharedState));
    close(fd);

    // 删除共享内存对象
    // 读取一次后即清理，避免残留
    shm_unlink(kShmName);
    return true;
}

// ============================================================
// 收集当前进程的运行状态
// ============================================================
static SharedState collect_state(int generation) {
    SharedState state{};
    state.session_count = 42;
    state.total_processed = 12345 * generation;
    state.generation = generation;
    snprintf(state.message, sizeof(state.message),
             "第 %d 代进程的遗留状态", generation);
    return state;
}

// ============================================================
// 进程主逻辑
// ============================================================
// 通过环境变量 HOT_RESTART_GEN 标记进程代数
// 如果环境变量不存在，说明是第一代进程
static int run_process() {
    // 读取进程代数
    const char* gen_str = getenv("HOT_RESTART_GEN");
    int generation = gen_str ? atoi(gen_str) : 1;

    std::cout << "=== 方案 E：进程级热替换（优雅重启）===\n";
    std::cout << "PID = " << getpid()
              << ", 第 " << generation << " 代进程\n\n";

    // 尝试从共享内存恢复状态
    SharedState state{};
    bool restored = load_state_from_shm(state);

    if (restored) {
        std::cout << "[恢复状态] 从上一代进程继承：\n";
        std::cout << "  session_count: "
                  << state.session_count << "\n";
        std::cout << "  total_processed: "
                  << state.total_processed << "\n";
        std::cout << "  generation: "
                  << state.generation << "\n";
        std::cout << "  message: "
                  << state.message << "\n\n";
    } else {
        std::cout << "[全新启动] 无继承状态\n\n";
    }

    // 模拟运行一段时间
    std::cout << "运行中 ...\n";
    for (int i = 1; i <= 3; ++i) {
        std::cout << "  tick " << i << "\n";
        sleep(1);
    }

    // 如果已经是第 2 代，就不再重启，直接退出
    if (generation >= 2) {
        std::cout << "\n第 2 代进程运行完毕，演示结束。\n";
        return 0;
    }

    // 保存状态到共享内存
    std::cout << "\n[优雅重启] 保存状态到共享内存 ...\n";
    SharedState new_state = collect_state(generation);
    if (!save_state_to_shm(new_state)) {
        std::cerr << "保存状态失败\n";
        return 1;
    }
    std::cout << "  状态已保存（generation="
              << new_state.generation << "）\n";

    // fork + exec 启动新进程
    std::cout << "\n[优雅重启] fork + exec 启动新进程 ...\n";
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // 子进程：设置代数环境变量并 exec 自身
        char gen_buf[16];
        snprintf(gen_buf, sizeof(gen_buf), "%d", generation + 1);

        // 设置环境变量 HOT_RESTART_GEN=<generation+1>
        setenv("HOT_RESTART_GEN", gen_buf, 1);

        // exec 自身（用 /proc/self/exe 获取当前可执行文件路径）
        execl("/proc/self/exe", "graceful_restart", nullptr);

        // exec 成功不会返回，到这里说明失败
        perror("execl");
        _exit(1);
    }

    // 父进程：等待子进程（新进程）完成
    std::cout << "  新进程已启动 (PID="
              << pid << ")，等待其完成 ...\n";
    int status;
    waitpid(pid, &status, 0);

    std::cout << "\n第 " << generation
              << " 代进程退出。\n";
    return 0;
}

int main() {
    return run_process();
}
