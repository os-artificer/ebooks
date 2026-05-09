# C++ 类型擦除：原理、备选方案与从 C++11 到 C++98 的两份参考实现

> **作者：**岭南过客  
> **更新时间：**2026-05-08

`std::function` 能装下函数、lambda、绑定器；`std::any` 能装下任意类型；`std::shared_ptr` 在不暴露 `T` 的析构方式的前提下，依然能在控制块里"记住"该用谁来 `delete`——这些"看起来不可能"的事，背后用的都是同一类技术：**类型擦除（Type Erasure）**。

---

## 一、什么是类型擦除技术

**把"具体类型"藏在统一接口背后，对外只暴露"能干啥"，不暴露"我是谁"。**

我们平时拿来"统一不同类型行为"的招数有三条，类型擦除是其中之一：

- **函数模板/类模板**：编译期多态。要求源码可见（模板要在头文件里），不同 `T` 各生成一份代码，容易代码膨胀。
- **公共基类 + 虚函数**：运行期多态。要求被装的类**主动**继承基类，是**侵入式**的；第三方类型（你改不了源码的那种）就装不进来了。
- **类型擦除**：运行期多态 + **不要求**继承公共基类。任何"长得像"目标接口的类型都能被装，是**非侵入式**的，也叫 duck typing 的 C++ 版。

标准库里你见过的"老朋友"都是它的产物：

- `std::function<R(Args...)>`：装任意可调用对象，无需继承一个 `Callable` 基类。
- `std::any`：装任意可拷贝的对象，运行时再按 `type_info` 取出。
- `std::shared_ptr<T>`：不要求 `T` 暴露析构方式，控制块里照样能 `delete`，靠的就是把 deleter 类型擦除。

C++11 之前，社区里就已经有 `boost::any` 与 `boost::function`，它们正是用同一套 idiom 在 C++98/03 下落地的——这也是后文第 5 节单独讲 C++98 写法的原因。

---

## 二、类型擦除技术有什么用

放在工程里，它解决的是一类"侵入式继承不好用"的问题：

- **异构容器**：`std::vector<AnyDrawable>` 同时装 `Circle`、`Square` 和 lambda，不用让它们都继承 `Shape`。
- **解耦接口与实现**：库作者只规定"需要哪几个成员调用"，使用方的类爱长啥样就长啥样；第三方类型也能塞进来。
- **跨 ABI / 动态库边界**：模板没法跨翻译单元，但已经擦除好的"包装类型"可以跨。`std::function` 这类类型常被作为插件 API 的参数。
- **抑制代码膨胀**：纯模板按 `T` 复制一份代码；类型擦除把分发集中到一处虚调用，二进制更紧凑。
- **典型业务场景**：回调表、信号槽、消息派发、插件系统、`Strategy` 模式的现代写法。

代价也很明确：多了一次堆分配 + 一次虚调用；高频小对象不叠 SBO 时缓存不友好。因此**别把它当万能钥匙**：当模板就能解决、且代码膨胀可接受时，模板更直接。

---

## 三、类型擦除技术实现原理

### 1. 五条主流备选方案

业界确实在用、且有代表性的实现路线，按"实现复杂度低 → 高""依赖语言特性少 → 多"排列，大致可分以下五条。

#### 方案 A：Concept-Model idiom（继承 + 虚函数）— 主流方案

- **思想**：内部定义一个抽象基类 `Concept`，再用模板 `Model<T>` 派生它，把任意 `T` 的成员调用桥接到 `Concept` 的虚函数；外层包装类只持有 `Concept*` 或 `unique_ptr<Concept>`。
- **优点**：写法直观，值语义自然（拷贝靠 `Clone()`、移动靠转移指针），调试栈干净，IDE 跳转友好。
- **缺点**：每个被装对象至少一次堆分配 + 一次虚调用；不叠 SBO 时缓存不太友好。
- **代表**：`boost::any`、`boost::function`、`std::function` 多数实现的核心层、libstdc++ 的 `std::any`。

#### 方案 B：手写 vtable（函数指针表 + `void*`）

- **思想**：把虚函数表手动展开成一组 `void(*)(void*, ...)` 函数指针，包装类持有 `void* data` 与 `const VTable* vtable`。
- **优点**：可关闭 RTTI / 异常；vtable 通常做成 `static constexpr`，零额外分配；C ABI 友好，做插件、跨语言很合适。
- **缺点**：手写模板生成 vtable 的元代码偏繁琐；少了编译期类型校验，容易写错。
- **代表**：`folly::Function` 的部分实现思路、嵌入式领域常见的手卷写法、Rust trait object 的 C++ 对照实现。

#### 方案 C：基于 `std::function` 组合

- **思想**：把对象需要支持的每个操作各存一个 `std::function`，包装类内部就是一组 `std::function`。
- **优点**：实现极短，几十行能搞定；接口要扩，加成员就行。
- **缺点**：每个操作都叠了一层 `std::function`（自身就是 type erasure），开销与堆分配翻倍；表达 `Clone` 这种"克隆自己"语义很别扭。
- **代表**：内部脚手架代码、教程式实现；够用，但不算工业级方案。

#### 方案 D：`typeid` + 类型化存储（`std::any` 风格）

- **思想**：包装类记录 `std::type_info` 与 `void*`，访问时按记录的类型做 `any_cast` 取回原始类型，再用模板对具体类型做静态调用。
- **优点**：能对外暴露"这里到底装了什么类型"（类型安全的取出），适合"装进去再原样取出"的场景。
- **缺点**：调用方仍要知道目标类型才能做事；想做"统一接口调用"时还得再叠一层 vtable / 函数指针，复杂度并未变低。
- **代表**：`std::any`、`boost::any`，侧重的是"装容器"而不是"装行为"。

#### 方案 E：小缓冲优化（SBO）— 正交叠加

- **思想**：包装类内嵌一块对齐过的字节缓冲（典型 16–32 字节），对象够小就 placement new 进去，免堆分配；够大再退回堆。
- **优点**：消除高频小对象的堆分配抖动，缓存友好。`std::function` 在大多数实现里都做了 SBO。
- **缺点**：实现复杂度明显上升，要处理对齐、移动、缓冲扩展、是否可平凡复制等细节；测试面也宽。
- **代表**：libstdc++ / libc++ 的 `std::function`、`folly::Function`、`std::any` 的小对象优化分支。

注意 SBO 是**正交**优化：它叠在方案 A 或 B 上都行，自身不构成独立的 idiom。

### 2. 选型口径与本文取舍

- **应用最广 = 方案 A（Concept-Model idiom）**：可读性、值语义、社区共识、教学示例都最齐全；`boost::any`、`std::function` 的核心层就是它。
- **本文两份完整范例（C++11+ 与 C++98/03）只用方案 A**，不掺其它，避免变成"四五份玩具实现拼盘"。
- 何时考虑别的：
  - 跨 C ABI / 关 RTTI / 嵌入式 → 方案 B；
  - 1 小时写一个能跑的玩具 → 方案 C；
  - 强调"装进去再取出来" → 方案 D；
  - 高频小对象、压尾延迟 → 在 A 或 B 上叠方案 E。

### 3. Concept-Model idiom 拆三层

为后两节铺垫，这里先把方案 A 的三层结构说清楚：

- **抽象层 `Concept`**：声明被装对象需要支持的纯虚操作（本文范例只有 `Draw`），外加 `Clone` 与虚析构。
- **模型层 `Model<T>`**：模板派生 `Concept`，把每个虚调用桥接到 `T` 的同名成员；`Clone()` 通过 `new Model<T>(*this)` 复制自己。
- **包装层 `AnyDrawable`**：对外暴露非模板 API，内部持有指向 `Concept` 的智能指针/裸指针；拷贝时通过 `Concept::Clone()` 实现深拷贝，移动时直接转移指针。

一次 `any.Draw()` 的分发路径如下，关键是"一次虚调用 + 一次直接调用"：

```
  Caller
    │   调 AnyDrawable::Draw()              非模板 API
    ▼
  AnyDrawable::Draw()
    │   _self->Draw()                       指针解引用
    ▼
  Concept::Draw()                           ← 抽象基类，纯虚函数
    │   vptr 分发                           ★ 唯一一次虚调用
    ▼
  Model<T>::Draw()                          ← 模板派生层（桥接器）
    │   _data.Draw()                        直接调用，编译期解析
    ▼
  T::Draw()                                 ← 用户类型（Circle / Square / Lambda...）
```

精简骨架长这样（不是完整可跑代码，完整版在第 4 节）：

```cpp
class AnyDrawable {
    struct Concept {
        virtual ~Concept() = default;
        virtual void Draw() const = 0;
        virtual std::unique_ptr<Concept> Clone() const = 0;
    };

    template <typename T>
    struct Model final : Concept {
        T _data;
        explicit Model(T data) : _data(std::move(data)) {}
        void Draw() const override { _data.Draw(); }
        std::unique_ptr<Concept> Clone() const override {
            return std::unique_ptr<Concept>(new Model<T>(_data));
        }
    };

    std::unique_ptr<Concept> _self;
public:
    template <typename T> AnyDrawable(T data);   // 包进 Model<T>
    AnyDrawable(const AnyDrawable& o);           // _self = o._self->Clone()
    AnyDrawable(AnyDrawable&& o) noexcept;       // 转移指针
    void Draw() const { _self->Draw(); }         // 一次虚调用
};
```

两件容易写错的事先点出来：

- **包装类拷贝**必须深拷贝内部对象，否则两个 `AnyDrawable` 共用一个 `Model<T>`，析构时 double free。深拷贝就是靠 `Concept::Clone()` 这条虚函数。
- **模板构造函数**会和拷贝构造起冲突：`AnyDrawable(T&&)` 在某些重载场景里"看起来"比 `AnyDrawable(const AnyDrawable&)` 更匹配。完整版要用 `std::enable_if` 把 `T = AnyDrawable` 的情况排除掉，把拷贝/移动让给真正的拷贝/移动构造函数。

---

## 四、一个类型擦除技术的实现范例（C++11+，仅 Concept-Model idiom）

下面这份代码可以**原样**拷出来跑，第一行注释里写了编译命令：

```cpp
// 保存为 type_erasure_demo.cpp 后：
//   g++ -std=c++17 -O2 -Wall -Wextra -o type_erasure_demo type_erasure_demo.cpp
//   ./type_erasure_demo
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

class AnyDrawable {
private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void Draw() const = 0;
        virtual std::unique_ptr<Concept> Clone() const = 0;
    };

    template <typename T>
    struct Model final : Concept {
        T _data;
        explicit Model(T data) : _data(std::move(data)) {}
        void Draw() const override { _data.Draw(); }
        std::unique_ptr<Concept> Clone() const override {
            return std::unique_ptr<Concept>(new Model<T>(_data));
        }
    };

    std::unique_ptr<Concept> _self;

public:
    // 用 enable_if 排除 T = AnyDrawable，避免劫持拷贝/移动构造
    template <typename T,
              typename = typename std::enable_if<
                  !std::is_same<typename std::decay<T>::type,
                                AnyDrawable>::value>::type>
    AnyDrawable(T&& data)
        : _self(new Model<typename std::decay<T>::type>(
              std::forward<T>(data))) {}

    AnyDrawable(const AnyDrawable& other)
        : _self(other._self->Clone()) {
        std::cout << "  [copy-ctor: clone via Concept::Clone()]\n";
    }

    AnyDrawable(AnyDrawable&& other) noexcept
        : _self(std::move(other._self)) {
        std::cout << "  [move-ctor: pointer transferred]\n";
    }

    // copy-and-swap 写法：值传参 + swap，统一处理拷贝赋值与移动赋值
    AnyDrawable& operator=(AnyDrawable other) noexcept {
        std::swap(_self, other._self);
        return *this;
    }

    ~AnyDrawable() = default;

    void Draw() const { _self->Draw(); }
};

// 不继承任何基类：只要有 void Draw() const 就能被装
struct Circle {
    double r;
    void Draw() const { std::cout << "Circle r=" << r << "\n"; }
};

struct Square {
    double side;
    void Draw() const { std::cout << "Square side=" << side << "\n"; }
};

// 把任意 void() lambda 适配成"有 Draw() 的类型"
template <typename F>
struct LambdaShape {
    F _f;
    void Draw() const { _f(); }
};

template <typename F>
LambdaShape<typename std::decay<F>::type> MakeLambdaShape(F&& f) {
    return LambdaShape<typename std::decay<F>::type>{std::forward<F>(f)};
}

int main() {
    std::vector<AnyDrawable> shapes;
    shapes.reserve(3);  // 预留容量，避免 emplace_back 触发已有元素的搬迁
    shapes.emplace_back(Circle{1.5});
    shapes.emplace_back(Square{2.0});
    shapes.emplace_back(MakeLambdaShape([] {
        std::cout << "lambda triangle\n";
    }));

    std::cout << "--- iterate ---\n";
    for (const auto& s : shapes) {
        s.Draw();
    }

    std::cout << "--- copy ---\n";
    AnyDrawable a = Circle{3.0};
    AnyDrawable b = a;        // 走 Concept::Clone()
    b.Draw();

    std::cout << "--- move ---\n";
    AnyDrawable c = std::move(a);  // 仅转移 _self 指针
    c.Draw();

    return 0;
}
```

跑出来应该是这样：

```
--- iterate ---
Circle r=1.5
Square side=2
lambda triangle
--- copy ---
  [copy-ctor: clone via Concept::Clone()]
Circle r=3
--- move ---
  [move-ctor: pointer transferred]
Circle r=3
```

几个关键点对着代码读：

- **`Circle` / `Square` / `LambdaShape` 都没继承任何基类**，证明这是非侵入式的。
- `std::vector<AnyDrawable>` 装的是**值**，不是 `Shape*`；所以容器复制、跨函数传值都按值语义走，不用关心谁负责 `delete`。
- 拷贝构造里那行 `[copy-ctor: clone via Concept::Clone()]` 证明走的是深拷贝路径——两个 `AnyDrawable` 各持有一个独立的 `Model<Circle>`。
- 移动构造里那行 `[move-ctor: pointer transferred]` 证明只动了 `_self` 指针，没有再 `new` 一份。

想自己造一个 `std::function` 该怎么走？把 `Draw()` 换成 `Invoke(Args...)`、再叠 SBO（方案 E），就接近 `std::function` 的最小骨架了。变参模板和 SBO 不是本文重点，留给读者动手。

---

## 五、C++98/03 兼容的实现方案与完整范例

很多遗留项目卡在 C++98/03，但仍然要做"装不同类型的可绘制对象"这种事；`boost::any` / `boost::function` 当年就是在这套约束下落地的。

下面把 C++11+ 写法搬到 C++98 时，需要**改**的几个点先列清楚，再贴一份完整可跑的源码。

### 1. 与 C++11+ 的核心差异

- **没有 `std::unique_ptr`**：内部用裸指针 `Concept*`，由包装类的析构 `delete`。`std::auto_ptr` 不要碰，所有权转移语义反人类。
- **没有移动语义**：只能实现拷贝构造 + `operator=`；赋值用 **copy-and-swap idiom**——先用 `other` 构造一个临时对象，再 `std::swap` 内部指针，旧对象在临时对象析构里被 `delete` 掉。这套写法**强异常安全**：构造临时对象抛异常时，原对象状态不动。
- **没有变参模板 / lambda / `nullptr` / `override` / `= default`**：用 `0` 当空指针、显式实现析构与拷贝、用 functor 替代 lambda；本范例只装一个无参操作 `Draw()`，不需要变参模板。
- **没有 `std::move`**：`vector::push_back` 必然走拷贝路径，所以 push_back 时会看到额外的"克隆"日志，这是 C++98 的预期行为，不是 bug。

### 2. 完整可编译可运行的范例

```cpp
// 保存为 type_erasure_legacy.cpp 后：
//   g++ -std=c++98 -O2 -Wall -Wextra -Wno-deprecated -o type_erasure_legacy type_erasure_legacy.cpp
//   ./type_erasure_legacy
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

class AnyDrawable {
private:
    struct Concept {
        virtual ~Concept() {}
        virtual void Draw() const = 0;
        virtual Concept* Clone() const = 0;
    };

    template <typename T>
    struct Model : Concept {
        T _data;
        Model(const T& data) : _data(data) {}
        virtual void Draw() const { _data.Draw(); }
        virtual Concept* Clone() const { return new Model<T>(*this); }
    };

    Concept* _self;

public:
    template <typename T>
    AnyDrawable(const T& data) : _self(new Model<T>(data)) {}

    AnyDrawable(const AnyDrawable& other) : _self(other._self->Clone()) {
        std::cout << "  [copy-ctor: clone via Concept::Clone()]\n";
    }

    // copy-and-swap idiom：构造临时再交换，强异常安全
    AnyDrawable& operator=(const AnyDrawable& other) {
        AnyDrawable tmp(other);
        std::swap(_self, tmp._self);
        return *this;
    }

    ~AnyDrawable() { delete _self; }

    void Draw() const { _self->Draw(); }
};

struct Circle {
    double r;
    Circle(double radius) : r(radius) {}
    void Draw() const { std::cout << "Circle r=" << r << "\n"; }
};

struct Square {
    double side;
    Square(double s) : side(s) {}
    void Draw() const { std::cout << "Square side=" << side << "\n"; }
};

// C++98 没有 lambda，用 functor 替代：只要有 Draw()，仍然能装进 AnyDrawable
class Stamp {
public:
    Stamp(const std::string& name) : _name(name) {}
    void Draw() const { std::cout << "Stamp[" << _name << "]\n"; }
private:
    std::string _name;
};

int main() {
    std::vector<AnyDrawable> shapes;
    shapes.reserve(3);

    std::cout << "--- push_back: each insert triggers AnyDrawable copy-ctor ---\n";
    shapes.push_back(Circle(1.5));
    shapes.push_back(Square(2.0));
    shapes.push_back(Stamp("alpha"));

    std::cout << "--- iterate ---\n";
    for (std::size_t i = 0; i < shapes.size(); ++i) {
        shapes[i].Draw();
    }

    std::cout << "--- copy ---\n";
    AnyDrawable a = Circle(3.0);
    AnyDrawable b = a;
    b.Draw();

    std::cout << "--- assign (copy-and-swap) ---\n";
    AnyDrawable d = Square(4.0);
    d = b;  // 走 operator=：构造临时再 swap
    d.Draw();

    return 0;
}
```

跑出来是这样：

```
--- push_back: each insert triggers AnyDrawable copy-ctor ---
  [copy-ctor: clone via Concept::Clone()]
  [copy-ctor: clone via Concept::Clone()]
  [copy-ctor: clone via Concept::Clone()]
--- iterate ---
Circle r=1.5
Square side=2
Stamp[alpha]
--- copy ---
  [copy-ctor: clone via Concept::Clone()]
Circle r=3
--- assign (copy-and-swap) ---
  [copy-ctor: clone via Concept::Clone()]
Circle r=3
```

读这份输出时注意两点：

- 开头三条 `[copy-ctor]` 是 `vector::push_back` 把临时 `AnyDrawable` 拷进存储时触发的——C++98 没有移动，所以**这是正常代价**；C++11+ 的版本用 `emplace_back` 直接构造，不会有这三行。
- `--- assign ---` 阶段那一条 `[copy-ctor]` 来自 `operator=` 内部构造 `tmp(other)` 时；之后 `std::swap` 不再分配也不再克隆，旧对象在 `tmp` 析构里被释放——这就是 copy-and-swap 的安全保证。

### 3. 迁移提示

如果项目能升 C++11，按下面三步把这套搬过去就行：

1. `_self` 换成 `std::unique_ptr<Concept>`，析构里的 `delete` 删掉。
2. 加上**移动构造**与**移动赋值**（noexcept 版），让 `std::vector` 在扩容时走移动而非拷贝。
3. 把 functor 改成 lambda，配一个 `LambdaShape` 适配器装进来；模板成员构造加 `enable_if` 排除 `T = AnyDrawable`。

写法更短、安全性更好，不止一点。

---

## 六、小结

| 维度 | 说明 |
| --- | --- |
| 核心思想 | 把"具体类型"藏在统一接口背后，运行期分发到具体实现 |
| 主流方案 | A. Concept-Model idiom；B. 手写 vtable；C. `std::function` 组合；D. `typeid` + 类型化存储；E. SBO（叠加优化） |
| 应用最广 | 方案 A（Concept-Model idiom），`boost::any` / `std::function` 的核心层都是它 |
| 优势 | 非侵入、值语义、API 稳定、抑制模板膨胀 |
| 代价 | 间接调用、可能堆分配；调试不如直接模板直观 |
| 标准库对照 | `std::function`、`std::any`、`std::shared_ptr` 的 deleter；C++98 时代对应 `boost::any`、`boost::function` |

选型口诀：

- 默认选 → 方案 A（Concept-Model）；
- 关 RTTI / 跨 C ABI → 方案 B；
- 玩具实现 → 方案 C；
- "装进去再取出来" → 方案 D；
- 想压堆分配抖动 → 在 A/B 上叠方案 E（SBO）。

老项目卡在 C++98/03 也别慌，第 5 节那套 copy-and-swap 写法能直接搬。能升 C++11 就升 C++11，改三步换来的可读性与安全性都很值。
