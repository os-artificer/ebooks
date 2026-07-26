// 适配器模式（Adapter Pattern）完整可编译示例
// 场景：项目统一接口 ITemperatureSensor 返回摄氏度，
//       手上有个遗留 LegacySensor 只给华氏度，用适配器接进来。
// 演示：原生实现 / 对象适配器（组合）/ 类适配器（多继承）三种方式，
//       客户端统一面向 ITemperatureSensor，无感知切换。

#include <iostream>
#include <string>

// ---- Target：客户端期望的统一接口 ----
class ITemperatureSensor {
public:
    virtual ~ITemperatureSensor() = default;
    // 统一返回摄氏度
    virtual double readCelsius() const = 0;
    virtual std::string name() const = 0;
};

// ---- 原生实现：现代传感器，直接输出摄氏度 ----
class ModernSensor : public ITemperatureSensor {
    double _c = 0;
public:
    explicit ModernSensor(double c) : _c(c) {}
    double readCelsius() const override { return _c; }
    std::string name() const override { return "ModernSensor"; }
};

// ---- Adaptee：被适配者（遗留传感器，只给华氏度，不可修改）----
class LegacySensor {
    double _f = 0;
public:
    explicit LegacySensor(double f) : _f(f) {}
    // 老式 API：只返回华氏度
    double getFahrenheit() const { return _f; }
};

// ---- Adapter（对象适配器）：组合持有 Adaptee，做 F->C 换算 ----
class FahrenheitAdapter : public ITemperatureSensor {
    const LegacySensor* _legacy;   // 组合持有被适配者
public:
    explicit FahrenheitAdapter(const LegacySensor* l) : _legacy(l) {}
    double readCelsius() const override {
        // 核心：接口翻译 + 单位换算  F -> C
        return (_legacy->getFahrenheit() - 32.0) * 5.0 / 9.0;
    }
    std::string name() const override { return "FahrenheitAdapter(Legacy)"; }
};

// ---- Adapter（类适配器）：多继承方式（进阶演示）----
class FahrenheitClassAdapter
    : public ITemperatureSensor
    , private LegacySensor {
public:
    explicit FahrenheitClassAdapter(double f) : LegacySensor(f) {}
    double readCelsius() const override {
        // 直接调用继承来的方法，无需持有引用
        return (getFahrenheit() - 32.0) * 5.0 / 9.0;
    }
    std::string name() const override { return "FahrenheitClassAdapter(Legacy)"; }
};

// ---- 客户端：只面向 Target，不关心背后是原生/对象适配/类适配 ----
void report(ITemperatureSensor& s) {
    std::cout << "  " << s.name() << " 读数: "
              << s.readCelsius() << " °C\n";
}

int main() {
    ModernSensor modern(37.0);
    LegacySensor legacy(98.6);          // 华氏 98.6 ≈ 摄氏 37.0
    FahrenheitAdapter adapter(&legacy);
    FahrenheitClassAdapter classAdapter(98.6);

    std::cout << "--- 客户端统一用 readCelsius()，三种实现无感知切换 ---\n";
    report(modern);
    report(adapter);
    report(classAdapter);
    return 0;
}
