// 选择排序 (Selection Sort) —— 每轮选最小放到未排序区头部
// 编译：cd src/cpp && make bin/selection_sort
// 运行：./bin/selection_sort

#include <algorithm>
#include <iostream>
#include <vector>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

// 基础版选择排序（带逐趟演示）
void selectionSortDemo(std::vector<int> arr) {
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 选择排序逐趟演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        // 在 [i, n-1] 中找最小元素下标
        for (int j = i + 1; j < n; ++j) {
            if (arr[j] < arr[minIdx]) {
                minIdx = j;
            }
        }
        // 把最小元素交换到位置 i
        if (minIdx != i) {
            std::swap(arr[i], arr[minIdx]);
        }

        // 打印当前趟结果：[0..i] 已排序，[i+1..n-1] 未排序
        std::cout << "第 " << i + 1 << " 趟 (选最小放位置 " << i << "): ";
        std::cout << "[";
        for (int k = 0; k <= i; ++k) std::cout << arr[k] << (k < i ? "," : "");
        std::cout << "] ";
        std::cout << "[";
        for (int k = i + 1; k < n; ++k) std::cout << arr[k] << (k < n - 1 ? "," : "");
        std::cout << "]\n";
    }

    std::cout << "最终结果: ";
    printArray(arr, "");
}

// 标准版选择排序（返回比较次数和交换次数）
void selectionSortCount(std::vector<int> arr, const std::string& label) {
    int n = static_cast<int>(arr.size());
    long long cmpCnt = 0, swapCnt = 0;

    for (int i = 0; i < n - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < n; ++j) {
            ++cmpCnt;
            if (arr[j] < arr[minIdx]) {
                minIdx = j;
            }
        }
        if (minIdx != i) {
            std::swap(arr[i], arr[minIdx]);
            ++swapCnt;
        }
    }

    std::cout << "[" << label << "] 比较: " << cmpCnt
              << " | 交换: " << swapCnt << "\n  结果: ";
    printArray(arr, "");
}

int main() {
    // 场景 1：一般乱序
    std::vector<int> data1 = {5, 3, 8, 4, 2};
    selectionSortDemo(data1);

    // 场景 2：已有序
    std::vector<int> data2 = {1, 2, 3, 4, 5};
    std::cout << "\n--- 场景 2: 已有序 ---\n";
    selectionSortCount(data2, "已有序");

    // 场景 3：完全逆序
    std::vector<int> data3 = {5, 4, 3, 2, 1};
    std::cout << "\n--- 场景 3: 完全逆序 ---\n";
    selectionSortCount(data3, "逆序");

    // 稳定性演示：相等元素的相对顺序会被改变
    std::cout << "\n--- 稳定性演示 ---\n";
    std::vector<std::pair<int, char>> stableData = {{3, 'a'}, {2, 'x'}, {2, 'y'}, {1, 'b'}};
    std::cout << "原始: ";
    for (auto& p : stableData) std::cout << "(" << p.first << "," << p.second << ") ";
    std::cout << "\n";

    int sn = static_cast<int>(stableData.size());
    for (int i = 0; i < sn - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < sn; ++j) {
            if (stableData[j].first < stableData[minIdx].first)
                minIdx = j;
        }
        if (minIdx != i)
            std::swap(stableData[i], stableData[minIdx]);
    }

    std::cout << "排序后: ";
    for (auto& p : stableData) std::cout << "(" << p.first << "," << p.second << ") ";
    std::cout << "\n注意: (2,x) 和 (2,y) 的相对顺序可能变化 -> 不稳定排序\n";

    return 0;
}
