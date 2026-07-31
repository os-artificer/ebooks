// 策略模式（Strategy Pattern）演示：电商订单的促销折扣策略
// 一族可互换的折扣算法，由 Context（订单）在运行时选择/切换。
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

// ---- Strategy 接口：折扣策略 ----
class DiscountStrategy {
public:
    virtual ~DiscountStrategy() = default;
    virtual double calc(double original) const = 0;   // 输入原价，返回折后价
    virtual std::string name() const = 0;
};

// ---- ConcreteStrategy A：无折扣 ----
class NoDiscount : public DiscountStrategy {
public:
    double calc(double original) const override { return original; }
    std::string name() const override { return "无折扣"; }
};

// ---- ConcreteStrategy B：按比例打折（rate = 0.8 表示 8 折）----
class PercentageDiscount : public DiscountStrategy {
    double _rate;
public:
    explicit PercentageDiscount(double rate) : _rate(rate) {}
    double calc(double original) const override { return original * _rate; }
    std::string name() const override {
        int pct = static_cast<int>(std::lround(_rate * 100.0));
        return "打折(" + std::to_string(pct) + "%)";
    }
};

// ---- ConcreteStrategy C：满减（满 threshold 减 minus）----
class ThresholdDiscount : public DiscountStrategy {
    double _threshold;
    double _minus;
public:
    ThresholdDiscount(double threshold, double minus)
        : _threshold(threshold), _minus(minus) {}
    double calc(double original) const override {
        return original >= _threshold ? original - _minus : original;
    }
    std::string name() const override { return "满减"; }
};

// ---- ConcreteStrategy D：固定直减（不会减成负数）----
class FixedDiscount : public DiscountStrategy {
    double _minus;
public:
    explicit FixedDiscount(double minus) : _minus(minus) {}
    double calc(double original) const override {
        return original > _minus ? original - _minus : 0.0;
    }
    std::string name() const override { return "直减"; }
};

// ---- Context：订单，持有并委派给策略 ----
class Order {
    std::unique_ptr<DiscountStrategy> _strategy;
    double _amount;
public:
    Order(double amount, std::unique_ptr<DiscountStrategy> s)
        : _strategy(std::move(s)), _amount(amount) {}

    void setStrategy(std::unique_ptr<DiscountStrategy> s) {
        _strategy = std::move(s);   // 运行时切换策略
    }

    double checkout() const {
        double final = _strategy->calc(_amount);
        std::cout << "  [" << _strategy->name() << "] 原价 "
                  << _amount << " -> 实付 " << final << "\n";
        return final;
    }
};

int main() {
    double amount = 200.0;
    std::cout << "--- 同一笔订单(原价 " << amount << ")，切换不同促销策略 ---\n";

    Order order(amount, std::make_unique<NoDiscount>());
    order.checkout();

    order.setStrategy(std::make_unique<PercentageDiscount>(0.8));
    order.checkout();

    order.setStrategy(std::make_unique<ThresholdDiscount>(100.0, 20.0));
    order.checkout();

    order.setStrategy(std::make_unique<FixedDiscount>(30.0));
    order.checkout();

    return 0;
}
