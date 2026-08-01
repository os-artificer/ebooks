// 希尔排序 (Shell Sort) —— 分组插入排序，增量递减至 1
// 编译：cd src/cpp && make bin/shell_sort
// 运行：./bin/shell_sort

#include <iostream>
#include <vector>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

// Shell 排序（带增量序列演示）
void shellSortDemo(std::vector<int> arr) {
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 希尔排序逐趟演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    // 使用 Shell 原始增量序列: gap = n/2, n/4, ..., 1
    for (int gap = n / 2; gap > 0; gap /= 2) {
        std::cout << "--- 当前增量 gap = " << gap << " ---\n";

        // 对每个分组做插入排序
        for (int i = gap; i < n; ++i) {
            int temp = arr[i];
            int j = i;
            // 同一组内，间隔为 gap 的插入排序
            while (j >= gap && arr[j - gap] > temp) {
                arr[j] = arr[j - gap];
                j -= gap;
            }
            arr[j] = temp;
        }

        std::cout << "gap=" << gap << " 后: ";
        printArray(arr, "");
    }

    std::cout << "最终结果: ";
    printArray(arr, "");
}

// 统计版
void shellSortCount(std::vector<int> arr, const std::string& label) {
    int n = static_cast<int>(arr.size());
    long long cmpCnt = 0, moveCnt = 0;

    for (int gap = n / 2; gap > 0; gap /= 2) {
        for (int i = gap; i < n; ++i) {
            int temp = arr[i];
            ++moveCnt;
            int j = i;
            while (j >= gap) {
                ++cmpCnt;
                if (arr[j - gap] > temp) {
                    arr[j] = arr[j - gap];
                    ++moveCnt;
                    j -= gap;
                } else {
                    break;
                }
            }
            arr[j] = temp;
            ++moveCnt;
        }
    }

    std::cout << "[" << label << "] 比较: " << cmpCnt
              << " | 移动: " << moveCnt << "\n  结果: ";
    printArray(arr, "");
}

int main() {
    // 场景 1：一般乱序
    std::vector<int> data1 = {9, 5, 8, 3, 7, 4, 2, 6, 1};
    shellSortDemo(data1);

    // 场景 2：已有序
    std::vector<int> data2 = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::cout << "\n--- 场景 2: 已有序 ---\n";
    shellSortCount(data2, "已有序");

    // 场景 3：逆序
    std::vector<int> data3 = {9, 8, 7, 6, 5, 4, 3, 2, 1};
    std::cout << "\n--- 场景 3: 完全逆序 ---\n";
    shellSortCount(data3, "逆序");

    return 0;
}
