// 插入排序 (Insertion Sort) —— 像整理扑克牌，逐张插入已排序区
// 编译：cd src/cpp && make bin/insertion_sort
// 运行：./bin/insertion_sort

#include <iostream>
#include <vector>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

// 带逐趟演示的插入排序
void insertionSortDemo(std::vector<int> arr) {
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 插入排序逐趟演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    // 从第 2 个元素开始（下标 1），把它插入前面已排序区
    for (int i = 1; i < n; ++i) {
        int key = arr[i];
        int j = i - 1;

        // 把比 key 大的元素都右移一位
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            --j;
        }
        arr[j + 1] = key;  // key 落位

        std::cout << "第 " << i << " 趟 (插入 " << key << "): 已排序 [";
        for (int k = 0; k <= i; ++k) std::cout << arr[k] << (k < i ? "," : "");
        std::cout << "] | 未排序 [";
        for (int k = i + 1; k < n; ++k) std::cout << arr[k] << (k < n - 1 ? "," : "");
        std::cout << "]\n";
    }

    std::cout << "最终结果: ";
    printArray(arr, "");
}

// 统计版：返回比较和移动次数
void insertionSortCount(std::vector<int> arr, const std::string& label) {
    int n = static_cast<int>(arr.size());
    long long cmpCnt = 0, moveCnt = 0;

    for (int i = 1; i < n; ++i) {
        int key = arr[i];
        ++moveCnt; // 取出 key
        int j = i - 1;
        while (j >= 0) {
            ++cmpCnt;
            if (arr[j] > key) {
                arr[j + 1] = arr[j];
                ++moveCnt;
                --j;
            } else {
                break;
            }
        }
        arr[j + 1] = key;
        ++moveCnt;
    }

    std::cout << "[" << label << "] 比较: " << cmpCnt
              << " | 移动: " << moveCnt << "\n  结果: ";
    printArray(arr, "");
}

int main() {
    // 场景 1：一般乱序
    std::vector<int> data1 = {5, 3, 8, 4, 2};
    insertionSortDemo(data1);

    // 场景 2：已有序
    std::vector<int> data2 = {1, 2, 3, 4, 5};
    std::cout << "\n--- 场景 2: 已有序 ---\n";
    insertionSortCount(data2, "已有序");

    // 场景 3：完全逆序
    std::vector<int> data3 = {5, 4, 3, 2, 1};
    std::cout << "\n--- 场景 3: 完全逆序 ---\n";
    insertionSortCount(data3, "逆序");

    // 稳定性演示：相等元素保持原始相对顺序
    std::cout << "\n--- 稳定性演示 ---\n";
    std::vector<std::pair<int, char>> stableData = {{3, 'a'}, {2, 'x'}, {2, 'y'}, {1, 'b'}};
    std::cout << "原始: ";
    for (auto& p : stableData) std::cout << "(" << p.first << "," << p.second << ") ";
    std::cout << "\n";

    int sn = static_cast<int>(stableData.size());
    for (int i = 1; i < sn; ++i) {
        auto key = stableData[i];
        int j = i - 1;
        while (j >= 0 && stableData[j].first > key.first) {
            stableData[j + 1] = stableData[j];
            --j;
        }
        stableData[j + 1] = key;
    }

    std::cout << "排序后: ";
    for (auto& p : stableData) std::cout << "(" << p.first << "," << p.second << ") ";
    std::cout << "\n注意: (2,x) 和 (2,y) 的相对顺序不变 -> 稳定排序\n";

    return 0;
}
