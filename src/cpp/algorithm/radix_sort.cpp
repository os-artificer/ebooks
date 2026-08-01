// 基数排序 (Radix Sort) —— 按位数从低到高逐位分配收集
// 编译：cd src/cpp && make bin/radix_sort
// 运行：./bin/radix_sort

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

// 获取数字的第 d 位（个位 d=1, 十位 d=2, ...）
int getDigit(int num, int d) {
    for (int i = 1; i < d; ++i) num /= 10;
    return num % 10;
}

// 获取数字的最大位数
int getMaxDigits(const std::vector<int>& arr) {
    int maxVal = *std::max_element(arr.begin(), arr.end());
    int digits = 0;
    while (maxVal > 0) { maxVal /= 10; ++digits; }
    return digits;
}

void radixSortDemo(std::vector<int> arr) {
    int n = static_cast<int>(arr.size());
    int maxDigits = getMaxDigits(arr);

    std::cout << "\n========== 基数排序逐位演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");
    std::cout << "最大位数: " << maxDigits << "\n\n";

    // 从最低位（个位）开始，逐位用计数排序
    for (int d = 1; d <= maxDigits; ++d) {
        std::cout << "--- 第 " << d << " 轮（"
                  << (d == 1 ? "个位" : d == 2 ? "十位" : d == 3 ? "百位" :
                     std::to_string(d) + "-th digit")
                  << "）---\n";

        // 用计数排序对当前位稳定排序
        std::vector<std::vector<int>> buckets(10);  // 0~9 共 10 个桶

        // 分配：按第 d 位放入对应桶
        for (int i = 0; i < n; ++i) {
            int digit = getDigit(arr[i], d);
            buckets[digit].push_back(arr[i]);
        }

        // 打印分配结果
        std::cout << "  分配: ";
        bool first = true;
        for (int b = 0; b < 10; ++b) {
            if (!buckets[b].empty()) {
                if (!first) std::cout << " | ";
                std::cout << "桶" << b << ":[";
                for (size_t k = 0; k < buckets[b].size(); ++k)
                    std::cout << buckets[b][k] << (k + 1 < buckets[b].size() ? "," : "");
                std::cout << "]";
                first = false;
            }
        }
        std::cout << "\n";

        // 收集：按桶 0->9 顺序合并回原数组
        int idx = 0;
        for (int b = 0; b < 10; ++b) {
            for (int val : buckets[b]) {
                arr[idx++] = val;
            }
        }

        std::cout << "  收集: ";
        printArray(arr, "");
        std::cout << "\n";
    }

    std::cout << "最终结果: ";
    printArray(arr, "");

    // 稳定性验证
    std::cout << "\n--- 稳定性演示 ---\n";
    struct Pair { int val; char tag; };
    std::vector<Pair> stableData = {{170, 'a'}, {45, 'x'}, {75, 'y'},
                                     {90, 'b'}, {802, 'c'}, {24, 'z'},
                                     {2, 'd'}, {66, 'w'}};
    std::cout << "原始: ";
    for (auto& p : stableData) std::cout << "(" << p.val << "," << p.tag << ") ";
    std::cout << "\n";

    int sn = static_cast<int>(stableData.size());
    int sd = 3; // 最大 3 位数
    for (int d = 1; d <= sd; ++d) {
        std::vector<std::vector<Pair>> sbuckets(10);
        for (int i = 0; i < sn; ++i) {
            int digit = getDigit(stableData[i].val, d);
            sbuckets[digit].push_back(stableData[i]);
        }
        int idx = 0;
        for (int b = 0; b < 10; ++b)
            for (auto& p : sbuckets[b])
                stableData[idx++] = p;
    }

    std::cout << "排序后: ";
    for (auto& p : stableData) std::cout << "(" << p.val << "," << p.tag << ") ";
    std::cout << "\n注意: 相同值的元素保持原始相对顺序 -> 稳定排序\n";
}

int main() {
    std::vector<int> data1 = {170, 45, 75, 90, 802, 24, 2, 66};
    radixSortDemo(data1);

    // 大量数据测试
    std::cout << "\n--- 较大数据集 ---\n";
    std::vector<int> data2 = {329, 457, 657, 839, 436, 720, 355};
    radixSortDemo(data2);

    return 0;
}
