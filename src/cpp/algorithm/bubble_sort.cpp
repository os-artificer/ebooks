// 冒泡排序（Bubble Sort）完整可运行示例
//
// 编译：进入 src/cpp/ 目录执行  make bin/bubble_sort
// 运行：./bin/bubble_sort
//
// 特点：
//   1. 原地（in-place）排序，额外空间 O(1)
//   2. 稳定排序（相等元素不交换）
//   3. 带“提前退出”优化：某轮没有任何交换即说明已有序，直接结束
//
// 注意：本文件即文章里提到的“完整可运行示例代码”，
//       文中代码片段为说明原理的伪代码，正式可编译版本以本文件为准。

#include <iostream>
#include <vector>

// 打印数组：sortedFrom 之后的元素视为“已就位”，用 [x] 高亮
static void printArray(const std::vector<int>& a, int sortedFrom) {
    for (size_t i = 0; i < a.size(); ++i) {
        if (static_cast<int>(i) >= sortedFrom)
            std::cout << "[" << a[i] << "] ";
        else
            std::cout << " " << a[i] << "  ";
    }
    std::cout << "\n";
}

// 标准冒泡排序（带提前退出优化）
// arr: 待排序数组，原地排序
static void bubbleSort(std::vector<int>& arr) {
    const size_t n = arr.size();
    for (size_t i = 0; i < n; ++i) {
        bool swapped = false;  // 本轮是否发生过交换
        // 每轮把当前最大的元素“冒泡”到末尾，
        // 已就位的 (n-i) 个元素无需再参与比较
        for (size_t j = 0; j + 1 < n - i; ++j) {
            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
                swapped = true;
            }
        }
        // 本轮没有任何交换，说明数组已经有序，提前结束
        if (!swapped) break;
    }
}

int main() {
    // 场景一：一般乱序数组，演示逐轮冒泡过程
    {
        std::vector<int> a = {5, 3, 8, 4, 2};
        std::cout << "=== 场景一：冒泡排序逐轮演示 ===\n";
        std::cout << "初始  : ";
        printArray(a, static_cast<int>(a.size()));

        const size_t n = a.size();
        for (size_t i = 0; i < n; ++i) {
            bool swapped = false;
            for (size_t j = 0; j + 1 < n - i; ++j) {
                if (a[j] > a[j + 1]) {
                    std::swap(a[j], a[j + 1]);
                    swapped = true;
                }
            }
            std::cout << "第 " << (i + 1) << " 轮: ";
            printArray(a, static_cast<int>(n - i - 1));
            if (!swapped) {
                std::cout << "  (本轮无交换，数组已有序，提前结束)\n";
                break;
            }
        }
        std::cout << "结果  : ";
        printArray(a, 0);
        std::cout << "\n";
    }

    // 场景二：已经有序的数组，验证“提前退出”优化
    {
        std::vector<int> a = {1, 2, 3, 4, 5};
        std::cout << "=== 场景二：已有序数组（验证提前退出，期望 O(n)）===\n";
        std::cout << "初始  : ";
        printArray(a, static_cast<int>(a.size()));
        bubbleSort(a);
        std::cout << "结果  : ";
        printArray(a, 0);
        std::cout << "\n";
    }

    // 场景三：逆序数组，触发最坏情况
    {
        std::vector<int> a = {9, 7, 5, 3, 1};
        std::cout << "=== 场景三：逆序数组（最坏情况，O(n^2)）===\n";
        std::cout << "初始  : ";
        printArray(a, static_cast<int>(a.size()));
        bubbleSort(a);
        std::cout << "结果  : ";
        printArray(a, 0);
    }

    return 0;
}
