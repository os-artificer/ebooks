/*
 * Copyright 2026 ebooks and courses
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * bad_case.cpp —— C/C++ 初学者常见误区汇总（教学用）
 *
 * 术语（下文「讲解」沿用）：
 * - UB（undefined behavior）：标准完全不约束程序行为；勿依赖「碰巧能跑」。
 * - 实现定义（implementation-defined）：实现必须给出某种规则，但可随平台变化。
 * - 未指定（unspecified）：行为合法但多种结果之一，不必文档唯一。
 *
 * 约定：
 * - 标了「误区」的片段请勿照搬到生产代码。
 * - 可能触发 UB 的示例放在 #if 0 ... #endif 中，默认不参与编译。
 * - 编译建议：g++ -std=c++17 -Wall -Wextra -Wconversion bad_case.cpp
 * - 部分「可编译的误区演示」会故意触发 -Wall 告警，便于对照改正。
 * - 每条「---------- N.」下若有「讲解 / 后果 / 正确」，便于自查。
 */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

/* ---------- 1. 使用未初始化变量（读取垃圾值 / UB） ----------
 * 讲解：自动存储期的普通变量若不初始化就读取，值是无定义的。
 * 后果：UB；编译器可能当作「永不读取」做激进优化。
 * 正确：声明时赋值或写 `int x{};` / `int x = 0;`。
 */
#if 0
void mistake_uninitialized_read() {
    int x;
    std::cout << x << "\n";  // UB
}
#endif

/* ---------- 2. = 与 == 混淆 ----------
 * 讲解：`=` 赋值，`==` 相等比较；条件里误写会先赋值再把结果当布尔值。
 * 后果：逻辑静默错误；-Wall 常有 -Wparentheses / 赋值告警。
 * 正确：比较写 `==`；或将条件写成显式布尔表达式。
 */
void demo_assignment_vs_equal(int flag) {
    if (flag = 1) {  // bug：赋值；多数编译器可告警
        std::cout << "branch taken\n";
    }
    (void)flag;
}

/* ---------- 3. 整数除法截断 ----------
 * 讲解：整数相除向零截断余数丢弃，不向最近偶数舍入。
 * 后果：非 UB；易与数学期望不符。
 * 正确：需要小数用 double/float；或显式换算后再取整。
 */
void demo_integer_division() {
    int a = 5;
    int b = 2;
    std::cout << "5/2 as int = " << (a / b) << "\n";  // 2，不是 2.5
}

/* ---------- 4. 有符号 / 无符号混比较（隐式转换惊喜） ----------
 * 讲解：寻常算术转换会把较小秩转为 unsigned；负数变极大无符号。
 * 后果：比较结果反直觉；非 UB 但有隐藏逻辑 bug。
 * 正确：同类比较（全 signed 或显式 cast）；或用宽类型包容。
 */
void demo_signed_unsigned_compare() {
    int i = -1;
    unsigned u = 10;
    // -1 转成 unsigned 后与 10 比大小：一般为「巨大数」，故 i<u 常为假。
    std::cout << "compare int(-1) < unsigned(10): "
              << (i < u ? "true\n" : "false (usual)\n");
}

/* ---------- 5. sizeof(指针) vs sizeof(数组) ----------
 * 讲解：形参写成 `int arr[]`/`int arr[10]` 实为指针；sizeof 得到指针大小。
 * 后果：误判缓冲区字节数→分配或拷贝过小（易 OOB）。
 * 正确：长度另传参；或用引用形参保留数组维度 template。
 */
void mistake_sizeof_pointer_vs_array(int *p, int arr[10]) {
    (void)p;
    (void)arr;
    int local[10] = {};
    std::cout << "sizeof(local array): " << sizeof(local)
              << ", sizeof(arr param): " << sizeof(arr) << "\n";
}

/* ---------- 6. 字符串字面量是 const char[]，勿写入 ----------
 * 讲解：字面量一般在只读存储；通过非 const 指针写入违反常量性。
 * 后果：UB；可能 SIGSEGV。
 * 正确：`const char*`/`std::string`/`std::array<char,N>` 可变缓冲。
 */
#if 0
void mistake_modify_string_literal() {
    char *s = "hello";  // C++ 字面量赋 char* 非标准写法
    s[0] = 'H';         // UB
}
#endif

/* ---------- 7. C 风格数组越界 ----------
 * 讲解：下标须在 0..N-1；越界读写不检查。
 * 后果：UB；可用 ASan 在调试期捕获。
 * 正确：`std::vector::at`、范围 for、边界检查封装。
 */
#if 0
void mistake_array_oob() {
    int a[3] = {1, 2, 3};
    a[3] = 0;  // UB
}
#endif

/* ---------- 8. switch 忘记 break（穿透） ----------
 * 讲解：case 结尾缺 break 会落入下一 case（fall-through）。
 * 后果：非 UB；多为逻辑错误。
 * 正确：补上 break；有意穿透用 [[fallthrough]]（C++17）标明。
 */
void demo_switch_fallthrough(int x) {
    switch (x) {
        case 1:
            std::cout << "one\n";
        case 2:
            std::cout << "two (maybe reached via fallthrough)\n";
            break;
        default:
            break;
    }
}

/* ---------- 9. 宏运算优先级坑 ----------
 * 讲解：宏只做文本替换，无表达式边界；参数中的运算符先于展开结合。
 * 后果：算术优先级错误；也难调试。
 * 正确：参数整体加括号 `#define SQ(x) ((x)*(x))`；优先用 constexpr。
 */
#define BAD_SQUARE(x) x * x
void demo_macro_precedence() {
    int y = BAD_SQUARE(1 + 2);  // 展开为 1 + 2 * 1 + 2 == 5，不是 9
    std::cout << "BAD_SQUARE(1+2) = " << y << "\n";
}

/* ---------- 10. 同一表达式内多次无序修改同一标量（UB） ----------
 * 讲解：若无序列点规则保证顺序，同一标量多次副作用顺序未定。
 * 后果：UB（C++17 前后措辞略异，结论仍是勿写）。
 * 正确：拆成多条语句；勿在同一表达式混写多个 ++/--。
 */
#if 0
void mistake_unsequenced_side_effects() {
    int i = 0;
    i = ++i + i++;  // UB
}
#endif

/* ---------- 11. new/delete、new[]/delete[] 不配對 ----------
 * 讲解：`new[]` 可能在首部记录元素个数；必须用 `delete[]`。
 * 后果：UB；破坏堆元数据。
 * 正确：`new`↔`delete`，`new[]`↔`delete[]`。
 */
#if 0
void mistake_wrong_delete_form() {
    int *p = new int[10];
    delete p;  // 应为 delete[] p;
}
#endif

/* ---------- 12. 悬挂指针：释放后继续使用 ----------
 * 讲解：释放后指针所指存储失效；写入即非法。
 * 后果：UB；常见崩溃或静默损坏。
 * 正确：释放后置 nullptr；所有权交给 smart_ptr。
 */
#if 0
void mistake_use_after_free() {
    int *p = new int(42);
    delete p;
    *p = 0;  // UB
}
#endif

/* ---------- 13. 返回局部变量的指针/引用 ----------
 * 讲解：函数返回后栈帧销毁；悬空指针/引用。
 * 后果：UB。
 * 正确：返回按值；返回堆对象并用 smart_ptr；或输出参数。
 */
#if 0
int *mistake_return_pointer_to_local() {
    int x = 42;
    return &x;  // UB：悬空指针
}
#endif

/* ---------- 14. 裸指针「谁分配谁释放」混乱 → 泄漏 / 二次释放 ----------
 * 讲解：同一指针只能释放一次；泄漏则是配对缺失。
 * 后果：二次释放 UB；泄漏占用内存。
 * 正确：单一所有权 smart_ptr；文档规定所有权传递。
 */
#if 0
void mistake_double_delete() {
    int *p = new int(1);
    delete p;
    delete p;  // UB
}
#endif

/* ---------- 15. 经由 void* 释放 new 出来的对象（delete void* / free） ----------
 * 讲解：`delete` 需静态类型以调用析构；`free` 仅配 malloc 族。
 * 后果：`delete void*` 在标准 C++ 一般为 ill-formed；`free(new …)` 为 UB。
 * 正确：`static_cast<T*>` 再 delete；malloc/C++ new 两套勿混。
 */
#if 0
void mistake_delete_via_void_pointer() {
    int *p = new int(42);
    void *pv = p;
    delete pv;  // ill-formed（标准 C++）；扩展放行仍为 UB
}

void mistake_free_after_new_via_void() {
    void *pv = new std::string("dtor never runs");
    std::free(pv);  // UB：且不调用析构
}
#endif

/* ---------- 16. scanf("%s") / gets 无限界 → 缓冲区溢出 ----------
 * 讲解：`%s` 读到空白为止不写上限则越过 buf。
 * 后果：栈破坏 / UB / 安全漏洞。
 * 正确：`"%Ns"`（N=容量-1）；或 `fgets`；禁用 gets。
 */
#if 0
#include <cstdio>
void mistake_scanf_unbounded() {
    char buf[8];
    std::scanf("%s", buf);  // 应用 "%7s"
}
#endif

/* ---------- 17. memset/memcpy 含非平凡类型的对象 ----------
 * 讲解：非平凡类型有构造/析构语义；字节清零破坏不变式。
 * 后果：UB。
 * 正确：默认初始化或赋值成员；勿对含 string 等对象 memset。
 */
#if 0
#include <cstring>
#include <string>
struct S {
    std::string name;
};
void mistake_memset_non_trivial() {
    S s{};
    std::memset(&s, 0, sizeof(s));  // UB
}
#endif

/* ---------- 18. 多态基类析构非 virtual ----------
 * 讲解：`delete Base*` 只会调用 `~Base`，派生子对象未析构。
 * 后果：UB（若对象实为 Derived）；资源泄漏。
 * 正确：基类 virtual ~Base；或勿经基类指针 delete。
 */
#if 0
struct Base {
    ~Base() {}  // 应为 virtual ~Base() = default;
};
struct Derived : Base {
    ~Derived() {}
};
void mistake_non_virtual_dtor() {
    Base *b = new Derived{};
    delete b;  // UB：未完整析构 Derived
}
#endif

/* ---------- 19. 值切片（object slicing） ----------
 * 讲解：派生对象赋给基类按值存储只保留基类子对象。
 * 后果：数据丢失；虚调用也不再指向派生实现。
 * 正确：传引用/指针或 smart_ptr；容器存基类指针。
 */
#if 0
struct Animal {
    virtual void speak() {}
};
struct Dog : Animal {};
void take_by_value(Animal a) { (void)a; }
void mistake_slicing() {
    Dog d;
    take_by_value(d);  // 切片
}
#endif

/* ---------- 20. std::endl 滥用 ----------
 * 讲解：`endl` 等价 `'\n'` + `flush`，频繁刷新缓冲开销大。
 * 后果：非 UB；性能问题。
 * 正确：日志多用 `'\n'`；需要立刻可见时再 flush。
 */
void demo_endl_vs_newline() {
    std::cout << "use \\n when flush not needed\n";
}

/* ---------- 21. auto 推导丢失引用/常量 ----------
 * 讲解：`auto x = expr` 去掉顶层引用；得到拷贝。
 * 后果：无意拷贝大对象或切断别名语义。
 * 正确：`auto&`、`const auto&`、`decltype(auto)`。
 */
void demo_auto_strips_cv_ref(std::string &s) {
    auto x = s;  // 拷贝；若要别名写 auto &x = s
    (void)x;
}

/* ---------- 22. Lambda 捕获生命周期（含隐式 this） ----------
 * 讲解：`[=]` 按值捕获局部副本；异步调用时外部对象可能已析构。
 * 后果：悬空指针 / UB。
 * 正确：weak_ptr；延长生命周期；捕获需要的值拷贝而非裸指针。
 */
#if 0
#include <functional>
struct Widget {
    std::function<void()> later;
    void arm() {
        later = [=]() {
            /* *this 可能已销毁 */
        };
    }
};
#endif

/* ---------- 23. unsigned 算术环绕 ----------
 * 讲解：unsigned 减法模 2^N；不会出现负数。
 * 后果：非 UB；易把「期望的有符号差」写成环绕。
 * 正确：先转更大有符号类型再减；或用 `<algorithm>`、断言前置。
 */
void demo_unsigned_wraparound(unsigned a, unsigned b) {
    unsigned diff = a - b;
    std::cout << "unsigned diff when a<b wraps: " << diff
              << " (as uint32_t: " << static_cast<std::uint32_t>(diff) << ")\n";
}

/* 辅助演示：指向条目 #25（signed overflow）。 */
void demo_signed_range_hint() {
    std::cout << "INT_MAX=" << std::numeric_limits<int>::max()
              << " (signed overflow ++INT_MAX is UB, see #25)\n";
}

/* ---------- 24. 空指针解引用（先 * 再判空的优化陷阱） ----------
 * 讲解：`*nullptr` 为 UB；之后的代码可能被优化假设指针有效。
 * 后果：UB；防御性判空可能失效。
 * 正确：先判空再使用；用 references 契约表达非空。
 */
#if 0
void mistake_null_dereference_reorder_trap(int *p) {
    std::cout << *p << "\n";  // UB；编译器可假定 p 非空
    if (p != nullptr) {
        std::cout << "ok\n";
    }
}
#endif

/* ---------- 25. 有符号整数溢出 ----------
 * 讲解：signed 溢出在 C/C++ 为 UB；不是「必定环绕」。
 * 后果：UB；编译器可作不可能分支优化。
 * 正确：改用更大宽度；`-fwrapv` 等非标准选项慎用。
 */
#if 0
void mistake_signed_int_overflow() {
    int x = std::numeric_limits<int>::max();
    ++x;  // UB
}
#endif

/* ---------- 26. 整数除零 ----------
 * 讲解：`a/b` 且 `b==0` 对整数类型为 UB。
 * 后果：UB；浮点除以零则为 ±inf/nan（IEEE），语义不同。
 * 正确：除前检查除数。
 */
#if 0
void mistake_integer_division_by_zero(int a, int b) {
    (void)(a / b);  // b==0 → UB
}
#endif

/* ---------- 27. 非法移位 ----------
 * 讲解：移位量须 \< 宽度且非负；有符号左移结果不可表示亦为 UB。
 * 后果：UB。
 * 正确：先检查移位量；无符号左操作数更安全但仍须 \< 宽度。
 */
#if 0
void mistake_illegal_shift() {
    std::uint32_t u = 1;
    (void)(u << 32U);            // UB：移位须 < 32
    int s = 1;
    (void)(s << -1);              // UB：负移位量
    (void)(s << 31);              // 有符号左移若结果不可表示 → UB
}
#endif

/* ---------- 28. scanf 格式与实参类型不匹配 ----------
 * 讲解：`scanf` 按格式写字节；类型不符导致栈局部越界写。
 * 后果：UB / 崩溃（Linux long 与 int 宽度不等时常见）。
 * 正确：`scanf` 与 `GCC -Wformat`；或用 iostream。
 */
#if 0
#include <cstdio>
void mistake_scanf_format_type_mismatch() {
    int n = 0;
    std::scanf("%ld", &n);  // 应为 long n 或改用 %d
}
#endif

/* ---------- 29. 越界指针算术 ----------
 * 讲解：指针只能在同一数组对象合法范围内算术（含末尾 past-the-end）。
 * 后果：形成无效指针算术 UB（未必等到解引用）。
 * 正确：控制索引；用 span/size。
 */
#if 0
void mistake_pointer_arithmetic_past_array() {
    short a[10];
    short *p = &a[15];  // UB：远超数组界
    (void)p;
}
#endif

/* ---------- 30. vector 迭代器失效 ----------
 * 讲解：`push_back` 可能重分配使旧迭代器全部失效。
 * 后果：UB。
 * 正确：失效规则查文档；必要时先保存索引再用 `[]`。
 */
#if 0
void mistake_vector_iterator_invalidation() {
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto it = v.begin();
    v.push_back(6);  // 可能使所有 iterator 失效
    *it = 0;         // UB
}
#endif

/* ---------- 31. Rule of Five / 手写析构与默认拷贝 ----------
 * 讲解：管理资源的类若仅定义析构，默认拷贝会浅拷贝指针。
 * 后果：double-delete UB。
 * 正确：Rule of Zero（容器接管）；或定义拷贝/移动全套。
 */
#if 0
struct BadRuleOfFive {
    int *p;
    BadRuleOfFive() : p(new int(1)) {}
    ~BadRuleOfFive() { delete p; }
};
void mistake_implicit_copy_with_raw_dtor() {
    BadRuleOfFive a;
    BadRuleOfFive b = a;  // UB：double delete
}
#endif

/* ---------- 32. operator= 未处理自赋值 ----------
 * 讲解：先 delete 再读右侧成员；若 `this==&rhs` 则读已释放内存。
 * 后果：UB。
 * 正确：copy-and-swap 惯用法。
 */
#if 0
struct BadSelfAssign {
    char *buf;
    BadSelfAssign() : buf(new char[8]) { std::memcpy(buf, "hello", 6); }
    ~BadSelfAssign() { delete[] buf; }
    BadSelfAssign &operator=(const BadSelfAssign &o) {
        delete[] buf;
        buf = new char[8];
        std::memcpy(buf, o.buf, 8);  // 若 this==&o，o.buf 已被 delete
        return *this;
    }
};
#endif

/* ---------- 33. 构造/析构期动态类型 ----------
 * 讲解：构造基类子对象时动态类型未到 Derived；析构同理。
 * 后果：期望「调到派生重写」会落空；纯虚路径可为 UB。
 * 正确：初始化放到构造函数体外或工厂函数。
 */
#if 0
struct PolyBase {
    PolyBase() { step(); }
    virtual void step() {}
};
struct PolyDerived : PolyBase {
    void step() override {}
};
// 构造 PolyDerived 时 PolyBase() 内 step() 绑定到 PolyBase::step，
// 不会调到 PolyDerived::step。

struct PureBase {
    virtual void finish() = 0;
    virtual ~PureBase() { finish(); }  // 析构阶段类型已回退为 PureBase → 调用纯虚 → UB
};
struct PureDerived : PureBase {
    void finish() override {}
};
#endif

/* ---------- 34. Strict aliasing（别名规则） ----------
 * 讲解：不得借不同类型指针别名读写同一存储（另有 memcpy/bit_cast 路径）。
 * 后果：UB。
 * 正确：`memcpy` / `std::bit_cast`（C++20）复制表示。
 */
#if 0
void mistake_strict_aliasing_type_pun() {
    float f = 3.14f;
    std::uint32_t bits = *reinterpret_cast<std::uint32_t *>(&f);
    (void)bits;
}
#endif

/* ---------- 35. 未对齐访问 ----------
 * 讲解：标量类型的地址须满足 `alignof(T)`。
 * 后果：UB（尤其 ARM 等）；或极慢路径。
 * 正确：`alignas`、`memcpy` 进对齐缓冲。
 */
#if 0
void mistake_misaligned_int_load() {
    alignas(unsigned char) unsigned char storage[sizeof(int) + 1];
    int *pi = reinterpret_cast<int *>(storage + 1);
    *pi = 42;  // storage+1 未必满足 alignof(int) → UB
}
#endif

/* ---------- 36. 析构函数抛出异常 ----------
 * 讲解：析构 noexcept(false) 若在栈展开路径抛会导致 terminate。
 * 后果：程序非正常终止。
 * 正确：析构 noexcept(true)；错误用 optional/error_code。
 */
#if 0
struct ThrowingDtor {
    ~ThrowingDtor() noexcept(false) { throw 1; }
};
#endif

/* ---------- 37. 数据竞争 ----------
 * 讲解：两线程无 happens-before 地读写同一非原子对象是数据竞争。
 * 后果：UB。
 * 正确：`mutex`、`atomic`、明确内存序。
 */
#if 0
// 示例需 thread + join；或用 -fsanitize=thread。
#endif

/* ---------- 38. fflush(stdin) 与 errno ----------
 * 讲解：`fflush` 只对输出流定义良好；stdin 依赖实现。
 * errno 须在失败后立即读取；中间任意库调用可能改写。
 * 正确：可移植丢弃输入用循环读；errno 紧邻检测。
 */
#if 0
#include <cerrno>
#include <cstdio>
void mistake_fflush_stdin() {
    std::fflush(stdin);  // stdin：行为未定义/不可移植
}

void mistake_errno_not_read_immediately() {
    std::fopen("probably_missing_file_xyz", "r");
    /* ... 大量其它调用 ... */
    if (errno != 0) { /* 不可靠 */ }
}
#endif

/* ---------- 39. 浮点数 == ----------
 * 讲解：二进制浮点多数十进制小数不可精确表示；累加舍入误差。
 * 后果：非 UB；逻辑误判。
 * 正确：epsilon 比较；或用有理数/decimal 库。
 */
void demo_float_equality_trap() {
    double a = 0.1 + 0.2;
    double b = 0.3;
    std::cout << "float eq 0.1+0.2==0.3: " << (a == b) << " (often 0)\n";
    std::cout << "prefer epsilon compare or rational math\n";
}

/* ---------- 40. {} 列表初始化收窄 ----------
 * 讲解：`T{x}` 禁止有损窄化转换。
 * 后果：编译失败（诊断）；阻止静默精度丢失。
 * 正确：显式 static_cast；或换合适类型。
 */
#if 0
void mistake_brace_init_narrowing() {
    int x{2.9};    // ill-formed：double → int 窄化
    char c{999};  // ill-formed：超 char 表示范围
    (void)x;
    (void)c;
}
#endif

/* ---------- 41. vector<bool> 代理 ----------
 * 讲解：`reference` 是位代理而非真实元素引用。
 * 后果：失效语义与普通容器不同；易悬空。
 * 正确：`vector<char>`、`bitset`、或其他压缩方案。
 */
void demo_vector_bool_proxy() {
    std::vector<bool> vb(3, true);
    auto bitref = vb[0];
    vb.push_back(false);  // 可能 invalidate
    (void)bitref;         // 勿在修改 vector 后依赖 bitref 的「引用」语义
    std::cout << "vector<bool> uses proxy references; "
                 "prefer std::vector<char>/bitset when needed\n";
}

/* ---------- 42. memcpy 重叠 ----------
 * 讲解：`memcpy` 要求源目的区间不重叠。
 * 后果：UB。
 * 正确：`memmove`。
 */
#if 0
void mistake_memcpy_overlapping_regions() {
    char buf[32] = "0123456789";
    std::memcpy(buf + 2, buf, 8);  // UB
}
#endif

/* ---------- 43. 值返回函数漏 return ----------
 * 讲解：控制流出函数末尾且无初始化返回值即为 UB。
 * 后果：UB；编译器常 `-Wreturn-type`。
 * 正确：覆盖所有路径；或改为 void。
 */
#if 0
int mistake_flow_off_end_without_return(int x) {
    if (x < 0) {
        return -1;
    }
    // 控制流到达此处且无 return → UB（读取垃圾返回值寄存器等）。
}
#endif

/* ---------- 44. const_cast 写原为 const 的对象 ----------
 * 讲解：去掉 const 写最初即为 const 的对象违反常量语义。
 * 后果：UB。
 * 正确：不要剥离正当 const；可变成员用 mutable。
 */
#if 0
void mistake_const_cast_modify_original_const() {
    const int origin = 42;
    int *pw = const_cast<int *>(&origin);
    *pw = 0;  // UB：origin 最初具有 const 限定
}
#endif

/* ---------- 45. 临时内部指针 ----------
 * 讲解：`std::string` 临时对象结束后其缓冲区销毁。
 * 后果：`c_str()` 指针悬空；读写 UB。
 * 正确：先把 string 存命名变量再取 `c_str()`。
 */
#if 0
void mistake_pointer_into_destroyed_string_temp() {
    const char *pz = (std::string("hello") + "!").c_str();
    std::cout << (pz ? pz : "") << "\n";  // UB：读取已销毁临时缓冲
}
#endif

/* ---------- 46. printf 可控格式串 ----------
 * 讲解：格式串中含 `%n`/`%s` 等可把内存写崩。
 * 后果：安全漏洞；也可能 UB。
 * 正确：`printf("%s", user)`；或 fputs。
 */
#if 0
#include <cstdio>
void mistake_user_supplied_printf_format(const char *user_controlled) {
    std::printf(user_controlled);
    // 若含 %s%n 可被利用；改用 fputs 或 printf("%s", user_controlled)
}
#endif

/* ---------- 47. 无关指针关系比较 ----------
 * 讲解：只有指向同一数组/对象的指针才有 `<` `>` 的全序意义；
 *       无关分配的指针比较在 C 常为 UB；C++ 亦有严格限制。
 * 后果：UB 或未指定；勿假设 malloc 先后顺序。
 * 正确：不要排序无关指针；用句柄 ID。
 */
#if 0
void mistake_compare_unrelated_allocations() {
    long *p = static_cast<long *>(std::malloc(sizeof(long)));
    long *q = static_cast<long *>(std::malloc(sizeof(long)));
    if (p != nullptr && q != nullptr && p < q) {
        (void)0;
    }
    std::free(p);
    std::free(q);
}
#endif

/* ---------- 48. moved-from 误用 ----------
 * 讲解：`unique_ptr` 移走后为空；大多数 moved-from 对象「有效但未指定」。
 * 后果：空指针解引用 UB。
 * 正确：只用库文档允许的操作；勿假定 string 一定空。
 */
#if 0
#include <memory>
void mistake_dereference_moved_unique_ptr() {
    std::unique_ptr<int> p = std::make_unique<int>(42);
    std::unique_ptr<int> q = std::move(p);
    *p = 1;  // UB：p 已为空指针状，解引用非法
}
#endif

/* 辅助演示：对应条目 #48；string moved-from 合法但未指定状态。 */
void demo_move_unspecified_state_hint() {
    std::string s = "payload";
    std::string sink = std::move(s);
    (void)sink;
    std::cout << "after std::move(string): moved-from valid but unspecified "
                 "state; never assume emptiness except for documented types "
                 "(e.g. unique_ptr is null)\n";
}

/* ---------- 49. thread 局部引用 ----------
 * 讲解：`detach` 线程可能比栈帧寿命长。
 * 后果：捕获局部引用 → UB。
 * 正确：按值捕获；或 join。
 */
#if 0
#include <thread>
void mistake_thread_capture_reference_to_local() {
    std::thread handle;
    {
        int stack_local = 42;
        handle = std::thread([&]() {
            (void)stack_local;  // 若线程晚于块结束仍在跑 → 悬垂引用 / UB
        });
    }
    handle.detach();  // 典型：块结束后线程仍访问 stack_local
}
#endif

/* ---------- 50. string_view 绑临时 ----------
 * 讲解：`string_view` 不拥有缓冲；右侧临时析构后指针悬空。
 * 后果：UB。
 * 正确：先保存 owning string；或用 string_view 字面量寿命规则。
 */
#if 0
#include <string_view>
void mistake_string_view_on_dead_temporary() {
    std::string_view sv = std::string("backing dies here");
    std::cout << sv << "\n";  // UB：存储随临时 std::string 已销毁，视图指向无效内存
}
#endif

/* ---------- 51. plain char 符号 ----------
 * 讲解：`char` 是否有符号由实现定义。
 * 后果：扩展 signed char 数值时可移植性差。
 * 正确：字节语义用 `unsigned char`/`uint8_t`/`std::byte`。
 */
void demo_plain_char_signedness() {
    char c = static_cast<char>(-1);
    int promoted = c;  // 实现定义：char 可能为 signed 或 unsigned
    std::cout << "char(-1) promoted to int = " << promoted
              << " (255 if unsigned char, -1 if signed char)\n";
    std::cout << "for byte semantics use std::uint8_t / "
                 "unsigned char / std::byte\n";
}

/* ---------- 52. 整数当中指针 ----------
 * 讲解：指针须指向合法对象或合法 past-the-end。
 * 后果：随意整数地址写入 UB。
 * 正确：只用 provenance 合法的指针。
 */
#if 0
void mistake_integer_as_pointer_deref() {
    std::uintptr_t addr = 0x1000;
    int *p = reinterpret_cast<int *>(addr);
    *p = 0;  // 几乎必然 UB：addr 并非指向有效 int 对象的指针
}
#endif

/* ---------- 53. 未初始化聚合体 ----------
 * 讲解：自动存储期聚合未 `{}` 初始化则成员不定。
 * 后果：读取 UB。
 * 正确：`SomePod x{}`；成员逐个赋值后再读。
 */
#if 0
struct SomePod {
    int a;
    char b;
};
void mistake_read_uninitialized_struct() {
    SomePod x;         // 未默认初始化（自动存储期）
    int u = x.a + x.b; // UB：读取未初始化的 indeterminate 值
}
#endif

/* ---------- 54. optional 空解引用 ----------
 * 讲解：`optional` 无值时 `*`/`->` 违反前置条件。
 * 后果：UB。
 * 正确：`value()`、`has_value()`；或 `value_or`。
 */
#if 0
#include <optional>
void mistake_optional_star_when_disengaged() {
    std::optional<int> o;
    *o = 1;             // UB：无值时 operator* 不可用；应 value()/has_value()
}
#endif

/* ---------- 55. assert 副作用 ----------
 * 讲解：`NDEBUG` 下整条 assert 删除。
 * 后果：调试与发布语义分叉。
 * 正确：副作用放在 assert 外。
 */
#if 0
#include <cassert>
void mistake_assert_side_effects(int &runs) {
    assert(++runs > 0);  // Debug 递增；Release(-DNDEBUG) 整条删除 → runs 不变，语义分叉
}
#endif

/* ---------- 56. feof 驱动读循环 ----------
 * 讲解：`feof` 在「读失败后」才置位；不是「预判 EOF」。
 * 后果：多读一次或死循环样板错误。
 * 正确：用 `fgetc`/`fread` 返回值检测 EOF。
 */
#if 0
#include <cstdio>
void mistake_feof_only_read_loop() {
    std::FILE *f = std::fopen("/dev/null", "r");
    if (!f) {
        return;
    }
    // feof 通常在「越过 EOF 的一次读失败之后」才为真；样板错误写法易多读一次或误判。
    while (!std::feof(f)) {
        (void)std::fgetc(f);
    }
    std::fclose(f);
}
#endif

/* ---------- 57. variant 取错类型 ----------
 * 讲解：`std::get<T>` 与当前备选不符则抛 `bad_variant_access`。
 * 后果：未捕获即 std::terminate；类型假设错误即为 bug。
 * 正确：`holds_alternative`；`get_if`；visit。
 */
#if 0
#include <variant>
void mistake_variant_wrong_alternative() {
    std::variant<int, float> v{42};
    // 抛 bad_variant_access；unchecked get 即为逻辑炸弹
    (void)std::get<float>(v);
}
#endif

/* ---------- 58. strlen / strcmp 缺 NUL ----------
 * 讲解：C 字符串 API 依赖 `'\0'` 结尾 sentinel。
 * 后果：strlen 越过缓冲区 UB。
 * 正确：保证 NUL；传长度时用 `memcmp`/`string_view`。
 */
#if 0
void mistake_strlen_without_null_terminator() {
    char raw[4] = {'q', 'w', 'e', 'r'};
    (void)std::strlen(raw);  // UB：越过数组找结尾
}
#endif

/* ---------- 59. 无终止递归 ----------
 * 讲解：递归深度超过栈容量。
 * 后果：溢出崩溃；语言层面多为 UB。
 * 正确：改迭代；尾递归依赖优化不可靠。
 */
#if 0
void mistake_unbounded_recursion(int n) {
    mistake_unbounded_recursion(n + 1);  // 栈溢出；编译器未必尾递归优化
}
#endif

/* ---------- 60. return std::move(局部) ----------
 * 讲解：对局部按值返回值写 `move` 常妨碍 NRVO/拷贝省略。
 * 后果：非 UB；多余移动或悲观优化。
 * 正确：`return local;`。
 */
std::string demo_return_local_without_std_move() {
    std::string s = "prefer `return s;` not `return std::move(s);` "
                      "for locals (NRVO/elision)";
    return s;
}

void demo_nrvo_vs_return_move_hint() {
    (void)demo_return_local_without_std_move();
    std::cout << "avoid `return std::move(local);` on local by-value "
                 "returns (blocks NRVO, rarely helps)\n";
}

/* ---------- 61. strcmp 非 C 字符串 ----------
 * 讲解：`strcmp` 读参直至 NUL。
 * 后果：无 NUL 缓冲区 UB。
 * 正确：`memcmp(a,b,n)`；或填 NUL。
 */
#if 0
void mistake_strcmp_on_non_cstring_buffer() {
    char block[3] = {'x', 'y', 'z'};
    (void)std::strcmp(block, "xyz");  // UB：block 无 '\\0'
}
#endif

/* ---------- 62. offsetof 非标准布局 ----------
 * 讲解：`offsetof` 仅对标准布局类型合法。
 * 后果：多态类等情形 UB。
 * 正确：改为 POD；或手动布局文档。
 */
#if 0
#include <cstddef>
struct NonStandardLayout {
    virtual ~NonStandardLayout() = default;
    int x;
};
void mistake_offsetof_on_polymorphic_type() {
    (void)offsetof(NonStandardLayout, x);  // 多态类型通常非标准布局 → offsetof UB
}
#endif

/* ---------- 63. async future 丢弃 ----------
 * 讲解：`std::async` 返回的 `future` 析构可能对异步策略阻塞。
 * 后果：看似后台的任务实则卡住退出作用域。
 * 正确：保存 `future`；或明确 launch policy + join。
 */
#if 0
#include <future>
void mistake_fire_and_forget_async() {
    std::async(std::launch::async, [] { /* 耗时工作 */ });
    // 返回的 std::future 立即析构；launch::async 时析构通常会阻塞到任务结束——并非「后台不管」。
}
#endif

/* ---------- 64. delete this ----------
 * 讲解：仅当对象由分配器分配且协议允许时可 `delete this`。
 * 后果：栈/静态对象 delete UB。
 * 正确：极少使用；优先外部 owner。
 */
#if 0
struct DeleteThisTrap {
    void destroy() {
        delete this;  // 仅当 *this 由 placement new / ::new 分配且生命周期约定允许时才合法
    }
};
void mistake_delete_this_on_stack() {
    DeleteThisTrap stack_obj;
    stack_obj.destroy();  // UB：栈对象绝不能 delete this
}
#endif

/* ---------- 65. 分配长度乘法溢出 ----------
 * 讲解：`count * sizeof(T)` 可先环绕再传入 malloc。
 * 后果：过小分配 + 大量写入 UB。
 * 正确：溢出检测；`<numeric>`/`checked_*`。
 */
#if 0
void mistake_allocation_size_multiplication_overflow() {
    std::size_t n = (std::numeric_limits<std::size_t>::max() / sizeof(int)) + 2;
    std::size_t bytes = n * sizeof(int);  // 环绕→极小值；malloc 成功后仍按 n 写入 → OOB / UB
    void *p = std::malloc(bytes);
    std::free(p);
}
#endif

/* ---------- 66. mutex 二次 lock ----------
 * 讲解：`std::mutex` 不可重入。
 * 后果：UB（标准用语 undefined）。
 * 正确：`recursive_mutex`；或分层锁。
 */
#if 0
#include <mutex>
void mistake_double_lock_same_mutex() {
    std::mutex m;
    m.lock();
    m.lock();  // 同一 mutex 不可重入 → 未定义行为（死锁或未规定）
}
#endif

/* ---------- 67. ctype 负 char ----------
 * 讲解：`unsigned char` 值域映射到 EOF..UCHAR_MAX；负 char → UB。
 * 后果：UB。
 * 正确：`unsigned char(c)` 再交给 `toupper` 等。
 */
#if 0
#include <cctype>
void mistake_ctype_on_negative_char() {
    char ch = static_cast<char>(-1);
    (void)std::isspace(static_cast<unsigned char>(ch));  // 正确：先转成 unsigned char
    (void)std::isspace(ch);  // 若 plain char 有符号且 ch<0 → 传入负 int → UB
}
#endif

/* ---------- 68. volatile 非同步 ----------
 * 讲解：`volatile` 不关内存可见性与排序。
 * 后果：数据竞争仍为 UB。
 * 正确：`atomic`、`mutex`。
 */
void demo_volatile_not_synchronization_hint() {
    std::cout << "volatile does not provide thread synchronization "
                 "or memory ordering; use std::atomic or mutex\n";
}

/* ---------- 69. 函数指针签名不匹配 ----------
 * 讲解：通过不相符的类型调用函数违反别名规则。
 * 后果：UB；ABI 崩溃。
 * 正确：只用匹配签名类型指针调用。
 */
#if 0
using VoidFn = void (*)();
int real_impl(int x) {
    return x + 1;
}
void mistake_call_through_wrong_signature_fn_ptr() {
    VoidFn fp = reinterpret_cast<VoidFn>(&real_impl);
    fp();  // UB：通过错误签名调用函数（ABI/栈不匹配）
}
#endif

/* ---------- 70. 重复 destroy_at ----------
 * 讲解：对象生命周期结束只可销毁一次。
 * 后果：UB。
 * 正确：`destroy_at` 配对 placement new；或用 RAII。
 */
#if 0
#include <memory>
void mistake_explicit_destructor_twice() {
    alignas(std::string) unsigned char buf[sizeof(std::string)];
    std::string *ps = new (buf) std::string("hi");
    std::destroy_at(ps);
    std::destroy_at(ps);  // UB：placement / 手动生命周期管理下重复销毁
}
#endif

/* ---------- 71. union 非活跃成员 ----------
 * 讲解：C++ 活跃成员规则严格；读写错误成员多为 UB。
 * 后果：UB。
 * 正确：`std::variant`；或 memcpy/bit_cast。
 */
#if 0
union IntOrFloat {
    int i;
    float f;
};
void mistake_union_read_inactive_member() {
    IntOrFloat u{};
    u.i = 42;
    float x = u.f;  // C++：读取非当前活跃成员通常为 UB（可用 std::variant / memcpy(bit_cast)）
    (void)x;
}
#endif

/* ---------- 72. 负值右移 ----------
 * 讲解：有符号负数右移为实现定义。
 * 后果：算术右移逻辑右移不定。
 * 正确：先用无符号再解释位。
 */
void demo_signed_right_shift_negative_hint() {
    std::cout << "right-shift on negative signed integers is "
                 "implementation-defined (not arithmetic shift "
                 "everywhere)\n";
}

/* ---------- 73. memcmp 填充 ----------
 * 讲解：填充字节值未指定。
 * 后果：memcmp 相等性与语义无关。
 * 正确：逐成员比较。
 */
#if 0
struct WithPaddingMaybe {
    char tag;
    int value;
};
void mistake_memcmp_struct_with_padding_bytes() {
    WithPaddingMaybe a{};
    WithPaddingMaybe b{};
    // 填充未指定，memcmp 结果无意义
    (void)std::memcmp(&a, &b, sizeof(WithPaddingMaybe));
}
#endif

/* ---------- 74. 无符号下标环绕 ----------
 * 讲解：`unsigned 0 - 1` 为大正数。
 * 后果：数组越界 UB。
 * 正确：用 `ptrdiff_t` 或有符号索引。
 */
#if 0
void mistake_unsigned_wraparound_as_array_index() {
    unsigned idx = 0;
    int arr[4] = {};
    arr[idx - 1] = 1;  // idx-1 在无符号算术下环绕 → 远超边界 → UB
}
#endif

/* ---------- 75. INT_MIN / -1 ----------
 * 讲解：商不在 signed int 表示内。
 * 后果：UB。
 * 正确：避免该组合；或用更大宽度。
 */
#if 0
void mistake_int_min_divided_by_minus_one() {
    int x = std::numeric_limits<int>::min();
    (void)(x / -1);  // 典型补码平台上结果不可表示 → UB（常被误认为得到 INT_MIN）
}
#endif

/* ---------- 76. dynamic_cast 引用 ----------
 * 讲解：引用失败抛 `bad_cast`；指针失败返回 nullptr。
 * 后果：未捕获终止。
 * 正确：`dynamic_cast<D*>(...) == nullptr`。
 */
#if 0
struct CastBase {
    virtual ~CastBase() = default;
};
struct CastDerived : CastBase {};
void mistake_dynamic_cast_reference_without_try(CastBase &b) {
    // 若 b 非 Derived → std::bad_cast
    CastDerived &d = dynamic_cast<CastDerived &>(b);
    (void)d;
}
#endif

/* ---------- 77. noexcept 内 throw ----------
 * 讲解：`noexcept(true)` 函数抛异常调用 `terminate`。
 * 后果：立即终止进程。
 * 正确：放宽 noexcept；或用错误码。
 */
#if 0
void mistake_throw_escaping_noexcept() noexcept {
    throw 1;  // 违反 noexcept 契约 → terminate（某些编译器告警）
}
#endif

/* ---------- 78. relaxed 发布 ----------
 * 讲解：`memory_order_relaxed` 不提供同步。
 * 后果：若有数据竞争仍为 UB；即便无竞争也可能读到次序错乱。
 * 正确：`release/acquire` 或默认 seq_cst。
 */
void demo_atomic_ordering_publish_hint() {
    std::cout << "do not publish non-atomic shared data "
                 "using only relaxed loads/stores; "
                 "need acquire/release or default(seq_cst) "
                 "for happens-before\n";
}

/* ---------- 79. 无副作用忙等 ----------
 * 讲解：无向前进展可能被激进假设。
 * 后果：极端优化意外。
 * 正确：`atomic`/条件变量。
 */
#if 0
void mistake_busy_spin_assumed_progress() {
    while (true) {
        /* 若循环体内无任何 IO、volatile、原子等「可观测」副作用，
         * 新版本标准下可能被视作违反前进保证→极端优化下惊奇行为。
         * 真实忙等请用条件变量或 std::atomic 带适当 memory_order。 */
    }
}
#endif

/* ---------- 80. enum 强转越界 ----------
 * 讲解：数值若无对应枚举表示则为非法或 unspecified。
 * 后果：读写枚举 UB（非法枚举）。
 * 正确：校验范围；映射表。
 */
#if 0
enum class NarrowEnum : unsigned char { A = 1, B = 2 };
void mistake_enum_cast_out_of_range() {
    int raw = 1024;
    NarrowEnum e = static_cast<NarrowEnum>(raw);  // 超出枚举底层类型表示→未指定或无效枚举值，读写需谨慎
    (void)e;
}
#endif

/* ---------- 81. catch(...) 吞异常 ----------
 * 讲解：裸 catch 吃掉错误路径。
 * 后果：静默失败；日志缺失。
 * 正确：边界翻译再封装；内部至少记录。
 */
void demo_swallow_all_exceptions_hint() {
    std::cout << "avoid bare catch(...) without log/rethrow "
                 "unless boundary layer; destructors still run "
                 "but errors vanish silently\n";
}

/* ---------- 82. 信号 handler 非 async-safe ----------
 * 讲解：异步信号上下文仅能调用 POSIX 列出的 async-signal-safe。
 * 后果：UB（死锁、重入 CRT）。
 * 正确：`sig_atomic_t` 标志位；主线程处理。
 */
#if 0
#include <csignal>
#include <iostream>
void mistake_signal_handler_not_async_safe(int signum) {
    (void)signum;
    std::cout << "handler\n";
    // UB：异步 handler 仅能调用 POSIX async-signal-safe 函数族
}

void mistake_register_bad_signal_handler() {
    std::signal(SIGINT, mistake_signal_handler_not_async_safe);
}
#endif

/* ---------- 83. noreturn 却返回 ----------
 * 讲解：`[[noreturn]]` 承诺永不返回。
 * 后果：UB。
 * 正确：`std::abort`/`exit`/无限循环（审慎）。
 */
#if 0
[[noreturn]] void mistake_noreturn_function_returns() {
    return;  // UB：标注无返回却返回（或以常规路径落到末尾）
}

void mistake_calls_noreturn() {
    mistake_noreturn_function_returns();
}
#endif

/* ---------- 84. shared_ptr 双裸指针 ----------
 * 讲解：同一裸指针不应构造多个无关 `shared_ptr`。
 * 后果：双重释放 UB。
 * 正确：`shared_ptr` 拷贝共亨；或 `alias` ctor。
 */
#if 0
#include <memory>
void mistake_two_shared_ptr_from_same_raw() {
    int *raw = new int(42);
    std::shared_ptr<int> a(raw);
    std::shared_ptr<int> b(raw);  // UB：两组无关控制块 → double-delete / 破坏计数
}
#endif

/* ---------- 85. mutex 二次 unlock ----------
 * 讲解：谁 lock 谁 unlock 一次。
 * 后果：UB。
 * 正确：`unique_lock::unlock` 配对 RAII。
 */
#if 0
#include <mutex>
void mistake_mutex_double_unlock() {
    std::mutex m;
    m.lock();
    m.unlock();
    m.unlock();  // UB：非 recursive_mutex 不得二次 unlock（与 #66 二次 lock 相对）
}
#endif

/* ---------- 86. 极大指针偏移 ----------
 * 讲解：指针算术须在数组可行地址范围内。
 * 后果：形成无效指针 UB。
 * 正确：边界检查；索引用整数。
 */
#if 0
void mistake_pointer_plus_huge_offset_ub() {
    int arr[8] = {};
    int *base = arr;
    int *p = base + (static_cast<std::ptrdiff_t>(1) << 60);  // 远超分配区形成无效指针 → UB
    (void)p;
}
#endif

void print_catalog() {
    std::cout
        << "常见初学者误区索引（详见本文件注释与 #if 0 示例）：\n"
        << " 1 未初始化就读\n"
        << " 2 if(x=y)\n"
        << " 3 整数除法截断\n"
        << " 4 有符号与无符号比较\n"
        << " 5 sizeof 数组参数退化\n"
        << " 6 修改字符串字面量\n"
        << " 7 数组越界\n"
        << " 8 switch 穿透\n"
        << " 9 宏不加括号\n"
        << "10 ++/-- 同一表达式无序副作用\n"
        << "11 new[] 却 delete\n"
        << "12 use-after-free\n"
        << "13 返回局部指针/引用\n"
        << "14 double free / 泄漏\n"
        << "15 void*/free 释放 new 对象\n"
        << "16 scanf %s 无宽度\n"
        << "17 对非平凡类型 memset\n"
        << "18 多态基类无虚析构\n"
        << "19 对象切片\n"
        << "20 endl 滥用\n"
        << "21 auto 丢掉引用\n"
        << "22 Lambda 捕获生命周期\n"
        << "23 unsigned 减法回绕\n"
        << "24 空指针解引用/判空优化\n"
        << "25 有符号整数溢出\n"
        << "26 整数除零\n"
        << "27 非法移位\n"
        << "28 scanf 格式与类型不符\n"
        << "29 越界指针算术\n"
        << "30 vector 迭代器失效\n"
        << "31 Rule of Five 浅拷贝\n"
        << "32 operator= 自赋值\n"
        << "33 构造析构期多态\n"
        << "34 strict aliasing\n"
        << "35 未对齐访问\n"
        << "36 析构抛异常\n"
        << "37 数据竞争\n"
        << "38 fflush stdin/errno\n"
        << "39 浮点==\n"
        << "40 {} 初始化收窄\n"
        << "41 vector<bool> 代理\n"
        << "42 memcpy 重叠须 memmove\n"
        << "43 值返回函数漏 return\n"
        << "44 const_cast 写 const 原对象\n"
        << "45 临时对象销毁后仍用内部指针\n"
        << "46 printf 格式串不可信输入\n"
        << "47 无关指针关系比较\n"
        << "48 移动后误用 moved-from\n"
        << "49 thread 引用/detach 悬垂\n"
        << "50 string_view 绑定临时\n"
        << "51 plain char 符号可移植\n"
        << "52 整数伪装指针解引用\n"
        << "53 读未初始化 struct\n"
        << "54 optional 空解引用\n"
        << "55 assert 副作用\n"
        << "56 feof 读循环误区\n"
        << "57 variant 取错类型\n"
        << "58 strlen/strcmp 缺 NUL 结尾\n"
        << "59 无终止递归\n"
        << "60 return std::move 局部\n"
        << "61 strcmp 非 C 字符串缓冲\n"
        << "62 offsetof 非标准布局\n"
        << "63 async future 丢弃阻塞\n"
        << "64 delete this 栈对象\n"
        << "65 分配长度乘法溢出\n"
        << "66 mutex 同线程二次 lock\n"
        << "67 isspace 负 char UB\n"
        << "68 volatile 非线程同步\n"
        << "69 函数指针签名不匹配调用\n"
        << "70 placement 重复手动析构\n"
        << "71 union 读非活跃成员\n"
        << "72 负整型右移实现定义\n"
        << "73 memcmp 含填充结构体\n"
        << "74 无符号下标环绕越界\n"
        << "75 INT_MIN/-1 除法溢出\n"
        << "76 dynamic_cast 引用失败\n"
        << "77 noexcept 内 throw\n"
        << "78 relaxed 不足以发布数据\n"
        << "79 无副作用忙等循环\n"
        << "80 enum 强转越表示范围\n"
        << "81 catch(...) 吞异常\n"
        << "82 信号处理非 async-safe\n"
        << "83 noreturn 却 return\n"
        << "84 shared_ptr 双裸指针\n"
        << "85 mutex 二次 unlock\n"
        << "86 极大指针偏移 UB\n";
}

}  // namespace

int main() {
    print_catalog();
    demo_assignment_vs_equal(0);
    demo_integer_division();
    demo_signed_unsigned_compare();
    mistake_sizeof_pointer_vs_array(nullptr, nullptr);
    demo_switch_fallthrough(1);
    demo_macro_precedence();
    demo_endl_vs_newline();
    std::string s = "ref demo";
    demo_auto_strips_cv_ref(s);
    demo_unsigned_wraparound(1U, 10U);
    demo_signed_range_hint();
    demo_float_equality_trap();
    demo_vector_bool_proxy();
    demo_move_unspecified_state_hint();
    demo_plain_char_signedness();
    demo_nrvo_vs_return_move_hint();
    demo_volatile_not_synchronization_hint();
    demo_signed_right_shift_negative_hint();
    demo_atomic_ordering_publish_hint();
    demo_swallow_all_exceptions_hint();
    return 0;
}
