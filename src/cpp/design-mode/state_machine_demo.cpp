/**
 * 状态机模式（State Pattern）完整示例：自动售货机
 *
 * 设计要点：
 * 1. State 抽象接口定义所有可能的事件
 * 2. 每个具体状态封装"在我这个状态下，事件该怎么处理、是否切换状态"
 * 3. VendingMachine（Context）持有当前状态，把所有事件委托给状态对象
 * 4. 状态实例共享（shared_ptr），因为具体状态本身无数据成员
 *
 * 编译（在 src/cpp 目录下执行）：
 *   make
 * 或单独编译：
 *   g++ -std=c++17 -O2 -Wall -Wextra -o bin/state_machine_demo design-mode/state_machine_demo.cpp
 *
 * 运行：./bin/state_machine_demo
 */
#include <iostream>
#include <memory>
#include <string>

// 前向声明：State 方法需要引用 VendingMachine
class VendingMachine;

// ============================================================
// State — 抽象状态接口
// 定义所有事件，具体状态类各自实现"当前状态下如何响应"
// ============================================================
class State {
public:
    virtual ~State() = default;

    virtual void insertCoin(VendingMachine& m) = 0;
    virtual void ejectCoin(VendingMachine& m) = 0;
    virtual void selectProduct(VendingMachine& m) = 0;
    virtual void dispense(VendingMachine& m) = 0;  // 内部：出货

    virtual std::string name() const = 0;
};

// ============================================================
// VendingMachine — 上下文（Context）
// 持有当前状态，把所有操作委托给状态对象
// ============================================================
class VendingMachine {
public:
    explicit VendingMachine(int stock);

    // 委托给当前状态
    void insertCoin()    { _state->insertCoin(*this); }
    void ejectCoin()     { _state->ejectCoin(*this); }
    void selectProduct() { _state->selectProduct(*this); }
    void dispense()      { _state->dispense(*this); }

    // 状态切换（由具体状态调用）
    void setState(std::shared_ptr<State> newState) {
        std::cout << "  [状态切换] " << _state->name()
                  << " -> " << newState->name() << "\n";
        _state = std::move(newState);
    }

    // 库存管理
    void dispenseProduct() {
        --_stock;
        std::cout << "  商品已出货，剩余库存: " << _stock << "\n";
    }
    int stock() const { return _stock; }

    // 补货（维护操作，直接在上下文处理）
    void refill(int count) {
        _stock += count;
        std::cout << "  补货 " << count << " 件，当前库存: " << _stock << "\n";
        if (_stock > 0 && _state == _soldOutState) {
            setState(_idleState);
        }
    }

    // 状态实例获取（共享，避免反复创建）
    std::shared_ptr<State> idleState()    const { return _idleState; }
    std::shared_ptr<State> hasCoinState() const { return _hasCoinState; }
    std::shared_ptr<State> soldState()    const { return _soldState; }
    std::shared_ptr<State> soldOutState() const { return _soldOutState; }

private:
    std::shared_ptr<State> _state;
    int _stock;

    // 共享状态实例（状态对象无数据成员，可复用）
    std::shared_ptr<State> _idleState;
    std::shared_ptr<State> _hasCoinState;
    std::shared_ptr<State> _soldState;
    std::shared_ptr<State> _soldOutState;
};

// ============================================================
// IdleState — 待机状态：等待投币
// ============================================================
class IdleState : public State {
public:
    void insertCoin(VendingMachine& m) override {
        std::cout << "  投入硬币，等待选择商品\n";
        m.setState(m.hasCoinState());
    }
    void ejectCoin(VendingMachine&) override {
        std::cout << "  当前没有投币，无法退币\n";
    }
    void selectProduct(VendingMachine&) override {
        std::cout << "  请先投币再选择商品\n";
    }
    void dispense(VendingMachine&) override {
        std::cout << "  待机状态不支持出货\n";
    }
    std::string name() const override { return "待机(Idle)"; }
};

// ============================================================
// HasCoinState — 已投币状态：等待选择商品
// ============================================================
class HasCoinState : public State {
public:
    void insertCoin(VendingMachine&) override {
        std::cout << "  已经投过币了，不要重复投币\n";
    }
    void ejectCoin(VendingMachine& m) override {
        std::cout << "  退回硬币\n";
        m.setState(m.idleState());
    }
    void selectProduct(VendingMachine& m) override {
        if (m.stock() > 0) {
            std::cout << "  选择商品，准备出货\n";
            m.setState(m.soldState());
            m.dispense();  // 委托给 SoldState.dispense()
        } else {
            std::cout << "  商品已售罄\n";
            m.setState(m.soldOutState());
        }
    }
    void dispense(VendingMachine&) override {
        std::cout << "  已投币状态不支持直接出货\n";
    }
    std::string name() const override { return "已投币(HasCoin)"; }
};

// ============================================================
// SoldState — 出货状态：正在出货（瞬时状态）
// ============================================================
class SoldState : public State {
public:
    void insertCoin(VendingMachine&) override {
        std::cout << "  正在出货，请稍候\n";
    }
    void ejectCoin(VendingMachine&) override {
        std::cout << "  已选择商品，无法退币\n";
    }
    void selectProduct(VendingMachine&) override {
        std::cout << "  正在出货，请稍候\n";
    }
    void dispense(VendingMachine& m) override {
        m.dispenseProduct();
        if (m.stock() > 0) {
            m.setState(m.idleState());
        } else {
            m.setState(m.soldOutState());
        }
    }
    std::string name() const override { return "出货(Sold)"; }
};

// ============================================================
// SoldOutState — 售罄状态：无库存
// ============================================================
class SoldOutState : public State {
public:
    void insertCoin(VendingMachine&) override {
        std::cout << "  商品已售罄，退回硬币\n";
    }
    void ejectCoin(VendingMachine&) override {
        std::cout << "  当前没有投币，无法退币\n";
    }
    void selectProduct(VendingMachine&) override {
        std::cout << "  商品已售罄\n";
    }
    void dispense(VendingMachine&) override {
        std::cout << "  售罄状态不支持出货\n";
    }
    std::string name() const override { return "售罄(SoldOut)"; }
};

// ============================================================
// VendingMachine 构造函数（在具体状态类定义之后）
// ============================================================
VendingMachine::VendingMachine(int stock)
    : _stock(stock)
    , _idleState(std::make_shared<IdleState>())
    , _hasCoinState(std::make_shared<HasCoinState>())
    , _soldState(std::make_shared<SoldState>())
    , _soldOutState(std::make_shared<SoldOutState>())
{
    _state = (_stock > 0) ? _idleState : _soldOutState;
    std::cout << "  [初始化] 售货机启动，库存: " << _stock
              << "，初始状态: " << _state->name() << "\n";
}

// ============================================================
// 演示
// ============================================================
int main() {
    std::cout << "==================================================\n";
    std::cout << "  状态机模式（State Pattern）演示：自动售货机\n";
    std::cout << "==================================================\n\n";

    VendingMachine machine(2);

    std::cout << "\n--- 场景 1：正常购买 ---\n";
    machine.insertCoin();
    machine.selectProduct();

    std::cout << "\n--- 场景 2：投币后退币 ---\n";
    machine.insertCoin();
    machine.ejectCoin();

    std::cout << "\n--- 场景 3：未投币直接选商品 ---\n";
    machine.selectProduct();

    std::cout << "\n--- 场景 4：买走最后一件 ---\n";
    machine.insertCoin();
    machine.selectProduct();

    std::cout << "\n--- 场景 5：售罄状态尝试操作 ---\n";
    machine.insertCoin();
    machine.selectProduct();

    std::cout << "\n--- 场景 6：补货后恢复正常 ---\n";
    machine.refill(3);
    machine.insertCoin();
    machine.selectProduct();

    std::cout << "\n==================================================\n";
    std::cout << "  演示结束\n";
    std::cout << "==================================================\n";
    return 0;
}
