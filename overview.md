# 排序算法系列文章 —— 交付总览

> 2026-08-01 产出 | 分类: algorithm | 语言: C++

## 交付清单（10 篇 + 10 个代码）

### 文章（notes/algorithm/）

| # | 文件 | 算法 | 复杂度 | 稳定 |
|---|------|------|--------|------|
| 1 | bubble-sort.md | 冒泡排序 | O(n²) 最好 O(n) | 稳定 |
| 2 | selection-sort.md | 选择排序 | O(n²) 固定 | 不稳定 |
| 3 | insertion-sort.md | 插入排序 | O(n²) 最好 O(n) | 稳定 |
| 4 | shell-sort.md | 希尔排序 | 取决于增量序列 | 不稳定 |
| 5 | merge-sort.md | 归并排序 | O(n log n) | 稳定 |
| 6 | quick-sort.md | 快速排序 | O(n log n) 最坏 O(n²) | 不稳定 |
| 7 | heap-sort.md | 堆排序 | O(n log n) 固定 | 不稳定 |
| 8 | counting-sort.md | 计数排序 | O(n+k) | 稳定 |
| 9 | bucket-sort.md | 桶排序 | O(n) 平均 / O(n log n) 最坏 | 取决于桶内算法 |
| 10 | radix-sort.md | 基数排序 | O(d×n) | 稳定 |

### 代码（src/cpp/algorithm/）

每个算法对应一个 `.cpp` 文件，均已编译通过：
- `bubble_sort.cpp` / `selection_sort.cpp` / `insertion_sort.cpp`
- `shell_sort.cpp` / `merge_sort.cpp` / `quick_sort.cpp`
- `heap_sort.cpp` / `counting_sort.cpp` / `bucket_sort.cpp` / `radix_sort.cpp`

## 每篇结构（符合 MEMORY.md §4.1 规范）

1. 标题 + 元信息行
2. 开篇引入（生活化场景）
3. 工作原理（mermaid 分层流程图 + 真实程序输出演示）
4. 适用场景与局限性（对比表）
5. C++ 伪代码 + 时间/空间复杂度及**计算方法推导**
6. 全文小结（6 要点）
7. 尾部标准两段式（与 strategy-pattern.md 一致）

## 质量检查

- [x] 全文无「」符号（grep 零匹配）
- [x] mermaid 流程图均为分层小图（每张 ≤8 节点）
- [x] 演示数据均来自真实程序运行输出
- [x] 复杂度分析含推导过程（等差数列求和 / 递归树分析 / 分步计数）
- [x] 稳定性均有证明或程序验证
- [x] 尾部格式与 strategy-pattern.md 完全对齐

## 发布状态

⚠️ **所有文章仅为草稿，未经用户显式确认不执行发布脚本。**
- publish_article.py 已登记 algorithm 分类
- nav.json 已添加 algorithm 分类块（items 空）
- 未生成任何 web/page 产物

## 下一步

等待用户确认：
1. 文章格式/内容是否满意？
2. 是否执行发布到 web？
3. 是否需要调整某篇文章的内容或深度？
