// 桶排序 (Bucket Sort) —— 数据分桶，各桶内排序后合并
// 编译：cd src/cpp && make bin/bucket_sort
// 运行：./bin/bucket_sort

#include <algorithm>
#include <iostream>
#include <vector>

void printArray(const std::vector<double>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

// 桶排序（针对 [0, 1) 范围的浮点数）
void bucketSortDemo(std::vector<double> arr) {
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 桶排序演示 ==========\n";
    std::cout << "初始数组 (" << n << " 个元素, 范围 [0,1)): ";
    printArray(arr, "");

    int bucketCount = n;  // 通常桶数 = 元素个数
    std::vector<std::vector<double>> buckets(bucketCount);

    // Step 1: 分桶 —— arr[i] 放入第 floor(arr[i] * n) 个桶
    std::cout << "\nStep 1 - 分桶:\n";
    for (int i = 0; i < n; ++i) {
        int bucketIdx = static_cast<int>(arr[i] * n);
        buckets[bucketIdx].push_back(arr[i]);
        std::cout << "  " << arr[i] << " -> 桶[" << bucketIdx << "]\n";
    }

    // Step 2: 各桶内部排序（演示用 std::sort 简化；元素少时插入排序更优）
    std::cout << "\nStep 2 - 各桶内排序:\n";
    for (int i = 0; i < bucketCount; ++i) {
        if (!buckets[i].empty()) {
            std::sort(buckets[i].begin(), buckets[i].end());
            std::cout << "  桶[" << i << "]: [";
            for (size_t j = 0; j < buckets[i].size(); ++j)
                std::cout << buckets[i][j] << (j + 1 < buckets[i].size() ? "," : "");
            std::cout << "]\n";
        }
    }

    // Step 3: 合并所有桶
    std::cout << "\nStep 3 - 按桶顺序合并:\n";
    std::vector<double> output;
    output.reserve(n);
    for (int i = 0; i < bucketCount; ++i) {
        for (double val : buckets[i]) {
            output.push_back(val);
        }
    }

    std::cout << "最终结果: ";
    printArray(output, "");

    // 复杂度说明
    std::cout << "\n复杂度分析:\n";
    std::cout << "  分桶: O(n)\n";
    std::cout << "  各桶排序: 若数据均匀分布，每桶平均 O(1)，总计 O(n)；最坏全进一桶 O(n log n)\n";
    std::cout << "  合并: O(n)\n";
    std::cout << "  空间: O(n+k), k=桶数\n";
}

int main() {
    // 场景 1：均匀分布的浮点数
    std::vector<double> data1 = {0.78, 0.17, 0.39, 0.26, 0.72, 0.94,
                                 0.21, 0.12, 0.23, 0.68};
    bucketSortDemo(data1);

    // 场景 2：几乎都在一个桶里（退化情况）
    std::cout << "\n--- 退化场景: 数据集中 ---\n";
    std::vector<double> data2 = {0.01, 0.02, 0.03, 0.04, 0.05,
                                 0.91, 0.92, 0.93, 0.94, 0.95};
    bucketSortDemo(data2);

    return 0;
}
