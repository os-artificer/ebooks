// 堆排序 (Heap Sort) —— 建大顶堆，逐次弹出堆顶到末尾
// 编译：cd src/cpp && make bin/heap_sort
// 运行：./bin/heap_sort

#include <iostream>
#include <vector>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

long long heapCmpCnt = 0, heapSwapCnt = 0;

// 堆化（下沉）：以 root 为根的子树调整为大顶堆
// n 是堆的大小（不是数组总大小，因为堆在缩小）
void heapify(std::vector<int>& arr, int n, int root) {
    int largest = root;
    int left = 2 * root + 1;   // 左孩子
    int right = 2 * root + 2;  // 右孩子

    if (left < n) {
        ++heapCmpCnt;
        if (arr[left] > arr[largest])
            largest = left;
    }
    if (right < n) {
        ++heapCmpCnt;
        if (arr[right] > arr[largest])
            largest = right;
    }

    if (largest != root) {
        std::swap(arr[root], arr[largest]);
        ++heapSwapCnt;
        // 递归堆化被影响的子树
        heapify(arr, n, largest);
    }
}

void heapSortDemo(std::vector<int> arr) {
    heapCmpCnt = 0;
    heapSwapCnt = 0;
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 堆排序演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    // 第一步：建堆（从最后一个非叶子节点开始，自底向上）
    std::cout << "\n--- 第一步: 建堆 ---\n";
    for (int i = n / 2 - 1; i >= 0; --i) {
        heapify(arr, n, i);
    }
    std::cout << "建堆完成(大顶堆): ";
    printArray(arr, "");

    // 第二步：逐次弹出堆顶到末尾
    std::cout << "\n--- 第二步: 逐次弹出 ---\n";
    for (int i = n - 1; i > 0; --i) {
        // 把当前最大值（堆顶）交换到已排序区末尾
        std::swap(arr[0], arr[i]);
        ++heapSwapCnt;

        std::cout << "弹出 " << arr[i] << " 到位置 " << i
                  << ", 剩余堆 [0.." << (i - 1) << "]: ";

        // 缩小堆范围，重新堆化根节点
        heapify(arr, i, 0);

        // 打印当前状态
        std::cout << "堆=[";
        for (int j = 0; j < i; ++j) std::cout << arr[j] << (j < i - 1 ? "," : "");
        std::cout << "] 已排=[";
        for (int j = i; j < n; ++j) std::cout << arr[j] << (j < n - 1 ? "," : "");
        std::cout << "]\n";
    }

    std::cout << "\n最终结果: ";
    printArray(arr, "");
    std::cout << "总比较: " << heapCmpCnt << " | 总交换: " << heapSwapCnt << "\n";
}

int main() {
    // 场景 1：一般乱序
    std::vector<int> data1 = {9, 4, 7, 1, 3, 6, 8, 2, 5};
    heapSortDemo(data1);

    // 场景 2：已有序
    std::cout << "\n--- 场景 2: 已有序 ---\n";
    std::vector<int> data2 = {1, 2, 3, 4, 5};
    heapCmpCnt = 0; heapSwapCnt = 0;
    int n2 = static_cast<int>(data2.size());
    for (int i = n2 / 2 - 1; i >= 0; --i) heapify(data2, n2, i);
    for (int i = n2 - 1; i > 0; --i) {
        std::swap(data2[0], data2[i]);
        ++heapSwapCnt;
        heapify(data2, i, 0);
    }
    std::cout << "最终结果: ";
    printArray(data2, "");
    std::cout << "总比较: " << heapCmpCnt << " | 总交换: " << heapSwapCnt << "\n";

    return 0;
}
