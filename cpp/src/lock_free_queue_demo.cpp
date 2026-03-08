/**
 * 无锁队列简单 demo：SPSC（单生产者单消费者）链表实现
 *
 * 编译（在项目根目录执行）：
 *   g++ -std=c++11 -O2 -pthread -o lock_free_queue_demo cpp/src/lock_free_queue_demo.cpp
 *
 * 编译（在 cpp/src 目录下执行）：
 *   g++ -std=c++11 -O2 -pthread -o lock_free_queue_demo lock_free_queue_demo.cpp
 *
 * 运行：./lock_free_queue_demo
 */
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

template <typename T>
class LockFreeQueue {
public:
    LockFreeQueue() {
        Node* dummy = new Node();
        _head.store(dummy);
        _tail.store(dummy);
    }

    ~LockFreeQueue() {
        T tmp;
        while (Pop(tmp)) {}
        delete _head.load();
    }

    void Push(T value) {
        Node* node = new Node(std::move(value));
        Node* prev = _tail.load(std::memory_order_acquire);
        prev->_next.store(node, std::memory_order_release);
        _tail.store(node, std::memory_order_release);
    }

    bool Pop(T& out) {
        Node* head = _head.load(std::memory_order_acquire);
        Node* next = head->_next.load(std::memory_order_acquire);
        if (next == nullptr) return false;
        out = std::move(next->_value);
        _head.store(next, std::memory_order_release);
        delete head;
        return true;
    }

private:
    struct Node {
        T _value;
        std::atomic<Node*> _next{nullptr};
        Node() = default;
        explicit Node(T v) : _value(std::move(v)) {}
    };

    std::atomic<Node*> _head;
    std::atomic<Node*> _tail;
};

int main() {
    LockFreeQueue<int> queue;
    const int count = 100000;

    std::thread producer([&queue, count]() {
        for (int i = 0; i < count; ++i) {
            queue.Push(i);
        }
    });

    std::thread consumer([&queue, count]() {
        int last = -1;
        int received = 0;
        while (received < count) {
            int v;
            if (queue.Pop(v)) {
                if (v != last + 1) {
                    std::cerr << "order error: expected " << (last + 1) << " got " << v << "\n";
                }
                last = v;
                ++received;
            }
        }
        std::cout << "consumer received " << received << " in order, last=" << last << "\n";
    });

    producer.join();
    consumer.join();
    std::cout << "demo done.\n";
    return 0;
}
