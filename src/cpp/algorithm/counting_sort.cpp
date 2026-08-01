// 计数排序 (Counting Sort) —— 非比较排序，统计每个值出现次数
// 编译：cd src/cpp && make bin/counting_sort
// 运行：./bin/counting_sort

#include <iostream>
#include <vector>

void printArray(const std::vector<int>& arr, const std::string& label) {
    std::cout << label << " [";
    for (size_t i = 0; i < arr.size(); ++i) {
        std::cout << arr[i] << (i + 1 < arr.size() ? ", " : "");
    }
    std::cout << "]\n";
}

void countingSortDemo(std::vector<int> arr) {
    int n = static_cast<int>(arr.size());
    std::cout << "\n========== 计数排序演示 ==========\n";
    std::cout << "初始数组: ";
    printArray(arr, "");

    // 找最大最小值确定计数范围
    int maxVal = arr[0], minVal = arr[0];
    for (int v : arr) {
        if (v > maxVal) maxVal = v;
        if (v < minVal) minVal = v;
    }
    int range = maxVal - minVal + 1;

    std::cout << "数据范围: [" << minVal << ", " << maxVal
              << "] -> 计数数组大小 = " << range << "\n\n";

    // Step 1: 计数
    std::vector<int> count(range, 0);
    for (int v : arr) {
        count[v - minVal]++;
    }

    std::cout << "Step 1 - 计数数组 (下标+" << minVal << " 为实际值):\n  ";
    for (int i = 0; i < range; ++i) {
        if (count[i] > 0)
            std::cout << "[" << (i + minVal) << "]:" << count[i] << "  ";
    }
    std::cout << "\n";

    // Step 2: 前缀和（计算每个元素的最终位置）
    std::vector<int> prefix(range);
    prefix[0] = count[0];
    for (int i = 1; i < range; ++i) {
        prefix[i] = prefix[i - 1] + count[i];
    }

    std::cout << "\nStep 2 - 前缀和数组 (表示 <= 该值的元素个数):\n  ";
    for (int i = 0; i < range; ++i) {
        if (count[i] > 0)
            std::cout << "[" << (i + minVal) << "]:" << prefix[i] << "  ";
    }
    std::cout << "\n";

    // Step 3: 反向遍历原数组，放到正确位置（稳定）
    std::vector<int> output(n);
    for (int i = n - 1; i >= 0; --i) {
        int idx = arr[i] - minVal;
        output[prefix[idx] - 1] = arr[i];
        prefix[idx]--;
    }

    std::cout << "\nStep 3 - 输出数组: ";
    printArray(output, "");

    // 验证稳定性
    std::cout << "\n--- 稳定性演示 ---\n";
    struct Pair {
        int val;
        char tag;
    };
    std::vector<Pair> stableData = {{2, 'a'}, {3, 'x'}, {2, 'b'}, {1, 'y'}, {3, 'z'}};
    std::cout << "原始: ";
    for (auto& p : stableData) std::cout << "(" << p.val << "," << p.tag << ") ";
    std::cout << "\n";

    int sMax = 3, sMin = 1, sRange = sMax - sMin + 1;
    std::vector<int> sCount(sRange, 0), sPrefix(sRange);
    for (auto& p : stableData) sCount[p.val - sMin]++;
    sPrefix[0] = sCount[0];
    for (int i = 1; i < sRange; ++i) sPrefix[i] = sPrefix[i-1] + sCount[i];

    std::vector<Pair> sOut(stableData.size());
    for (int i = static_cast<int>(stableData.size()) - 1; i >= 0; --i) {
        int idx = stableData[i].val - sMin;
        sOut[sPrefix[idx] - 1] = stableData[i];
        sPrefix[idx]--;
    }

    std::cout << "排序后: ";
    for (auto& p : sOut) std::cout << "(" << p.val << "," << p.tag << ") ";
    std::cout << "\n注意: (2,a) 在 (2,b) 前、(3,x) 在 (3,z) 前 -> 稳定排序\n";

    std::cout << "\n最终结果: ";
    printArray(output, "");
}

int main() {
    std::vector<int> data1 = {4, 2, 2, 8, 3, 3, 1};
    countingSortDemo(data1);

    return 0;
}
