// 快速排序 (Quick Sort) —— 分治+分区，选基准把数组分成大小两半
// 编译：cd src/cpp && make bin/quick_sort
// 运行：./bin/quick_sort

#include <iostream>
#include <vector>
#include <random>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

long long quickCmpCnt = 0, quickSwapCnt = 0;

// Lomuto 分区方案：选最后一个元素做基准
// 返回基准元素的最终位置
int partition(std::vector<int>& arr, int low, int high, int depth) {
    std::string indent(depth * 2, ' ');
    int pivot = arr[high];  // 选最后一个做 pivot
    int i = low - 1;        // i 是"小于等于 pivot 的区域"的边界

    std::cout << indent << "分区 [" << low << ".." << high
              << "], pivot=" << pivot << "\n";

    for (int j = low; j < high; ++j) {
        ++quickCmpCnt;
        if (arr[j] <= pivot) {
            ++i;
            if (i != j) {
                std::swap(arr[i], arr[j]);
                ++quickSwapCnt;
            }
        }
    }
    std::swap(arr[i + 1], arr[high]);
    ++quickSwapCnt;

    std::cout << indent << "  -> pivot " << pivot << " 落位 " << (i + 1)
              << ": [";
    for (int p = low; p <= i; ++p) std::cout << arr[p] << (p < i ? "," : "");
    std::cout << "] [" << arr[i + 1] << "] [";
    for (int p = i + 2; p <= high; ++p) std::cout << arr[p] << (p < high ? "," : "");
    std::cout << "]\n";

    return i + 1;
}

void quickSort(std::vector<int>& arr, int low, int high, int depth) {
    if (low < high) {
        int pi = partition(arr, low, high, depth);

        std::string indent(depth * 2, ' ');
        std::cout << indent << "左半区 [" << low << ".." << (pi - 1)
                  << "] | 右半区 [" << (pi + 1) << ".." << high << "]\n\n";

        quickSort(arr, low, pi - 1, depth + 1);
        quickSort(arr, pi + 1, high, depth + 1);
    }
}

void quickSortDemo(std::vector<int> arr) {
    quickCmpCnt = 0;
    quickSwapCnt = 0;
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 快速排序分区演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    quickSort(arr, 0, n - 1, 0);

    std::cout << "最终结果: ";
    printArray(arr, "");
    std::cout << "总比较: " << quickCmpCnt << " | 总交换: " << quickSwapCnt << "\n";
}

int main() {
    // 场景 1：一般乱序
    std::vector<int> data1 = {10, 7, 8, 9, 1, 5};
    quickSortDemo(data1);

    // 场景 2：已有序（Lomuto 最坏情况）
    std::cout << "\n--- 场景 2: 已有序 (Lomuto 最坏情况) ---\n";
    std::vector<int> data2 = {1, 2, 3, 4, 5};
    quickCmpCnt = 0; quickSwapCnt = 0;
    quickSort(data2, 0, static_cast<int>(data2.size()) - 1, 0);
    std::cout << "最终结果: ";
    printArray(data2, "");
    std::cout << "总比较: " << quickCmpCnt << " | 总交换: " << quickSwapCnt << "\n";

    // 场景 3：逆序
    std::cout << "\n--- 场景 3: 完全逆序 ---\n";
    std::vector<int> data3 = {5, 4, 3, 2, 1};
    quickCmpCnt = 0; quickSwapCnt = 0;
    quickSort(data3, 0, static_cast<int>(data3.size()) - 1, 0);
    std::cout << "最终结果: ";
    printArray(data3, "");
    std::cout << "总比较: " << quickCmpCnt << " | 总交换: " << quickSwapCnt << "\n";

    return 0;
}
