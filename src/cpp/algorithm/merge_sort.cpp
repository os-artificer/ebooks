// 归并排序 (Merge Sort) —— 分治法：递归分解再合并
// 编译：cd src/cpp && make bin/merge_sort
// 运行：./bin/merge_sort

#include <iostream>
#include <vector>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

long long mergeCmpCnt = 0;

// 合并两个有序子数组 arr[left..mid] 和 arr[mid+1..right]
void merge(std::vector<int>& arr, int left, int mid, int right, int depth) {
    std::string indent(depth * 2, ' ');

    // 复制到临时数组
    std::vector<int> leftArr(arr.begin() + left, arr.begin() + mid + 1);
    std::vector<int> rightArr(arr.begin() + mid + 1, arr.begin() + right + 1);

    std::cout << indent << "合并 [" << left << ".." << mid << "] + ["
              << mid + 1 << ".." << right << "]: ";
    std::cout << "[";
    for (size_t i = 0; i < leftArr.size(); ++i)
        std::cout << leftArr[i] << (i + 1 < leftArr.size() ? "," : "");
    std::cout << "] + [";
    for (size_t i = 0; i < rightArr.size(); ++i)
        std::cout << rightArr[i] << (i + 1 < rightArr.size() ? "," : "");
    std::cout << "] -> ";

    int i = 0, j = 0, k = left;
    while (i < static_cast<int>(leftArr.size()) && j < static_cast<int>(rightArr.size())) {
        ++mergeCmpCnt;
        if (leftArr[i] <= rightArr[j]) {   // <= 保证稳定性
            arr[k++] = leftArr[i++];
        } else {
            arr[k++] = rightArr[j++];
        }
    }
    while (i < static_cast<int>(leftArr.size())) arr[k++] = leftArr[i++];
    while (j < static_cast<int>(rightArr.size())) arr[k++] = rightArr[j++];

    // 打印合并结果
    std::cout << "[";
    for (int p = left; p <= right; ++p)
        std::cout << arr[p] << (p < right ? "," : "");
    std::cout << "]\n";
}

// 归并排序主函数（带分解-合并演示）
void mergeSort(std::vector<int>& arr, int left, int right, int depth) {
    if (left >= right) return;

    std::string indent(depth * 2, ' ');
    int mid = left + (right - left) / 2;

    std::cout << indent << "分解 [" << left << ".." << right
              << "] -> [" << left << ".." << mid << "] + ["
              << mid + 1 << ".." << right << "]\n";

    mergeSort(arr, left, mid, depth + 1);
    mergeSort(arr, mid + 1, right, depth + 1);
    merge(arr, left, mid, right, depth);
}

// 包装函数
void mergeSortDemo(std::vector<int> arr) {
    mergeCmpCnt = 0;
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 归并排序分治演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    mergeSort(arr, 0, n - 1, 0);

    std::cout << "最终结果: ";
    printArray(arr, "");
    std::cout << "总比较次数: " << mergeCmpCnt << "\n";
}

int main() {
    // 场景 1：一般乱序
    std::vector<int> data1 = {38, 27, 43, 3, 9, 82, 10};
    mergeSortDemo(data1);

    // 场景 2：已有序
    std::cout << "\n--- 场景 2: 已有序 ---\n";
    std::vector<int> data2 = {1, 2, 3, 4, 5, 6, 7};
    mergeCmpCnt = 0;
    mergeSort(data2, 0, static_cast<int>(data2.size()) - 1, 0);
    std::cout << "最终结果: ";
    printArray(data2, "");
    std::cout << "总比较次数: " << mergeCmpCnt << "\n";

    // 场景 3：逆序
    std::cout << "\n--- 场景 3: 完全逆序 ---\n";
    std::vector<int> data3 = {7, 6, 5, 4, 3, 2, 1};
    mergeCmpCnt = 0;
    mergeSort(data3, 0, static_cast<int>(data3.size()) - 1, 0);
    std::cout << "最终结果: ";
    printArray(data3, "");
    std::cout << "总比较次数: " << mergeCmpCnt << "\n";

    return 0;
}
