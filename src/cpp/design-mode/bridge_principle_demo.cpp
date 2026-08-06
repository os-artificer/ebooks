// bridge_principle_demo.cpp
// 桥接模式（Bridge）原理演示：Shape（抽象）× Renderer（实现）
// 编译：cd src/cpp && make bin/bridge_principle_demo
// 运行：./bin/bridge_principle_demo

#include <iostream>
#include <memory>
#include <string>

// ===== Implementor：渲染接口，"怎么画" =====
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void renderCircle(double x, double y, double r) const = 0;
    virtual void renderRectangle(double x, double y, double w, double h) const = 0;
    virtual std::string name() const = 0;
};

// ===== ConcreteImplementor：矢量渲染 =====
class VectorRenderer : public Renderer {
public:
    void renderCircle(double x, double y, double r) const override {
        std::cout << "矢量绘制 圆 (" << x << "," << y << ") 半径 " << r << "\n";
    }
    void renderRectangle(double x, double y, double w, double h) const override {
        std::cout << "矢量绘制 矩形 (" << x << "," << y << ") " << w << "x" << h << "\n";
    }
    std::string name() const override { return "矢量渲染器"; }
};

// ===== ConcreteImplementor：光栅渲染 =====
class RasterRenderer : public Renderer {
public:
    void renderCircle(double x, double y, double r) const override {
        std::cout << "光栅绘制 像素圆 (" << x << "," << y << ") 半径 " << r << "\n";
    }
    void renderRectangle(double x, double y, double w, double h) const override {
        std::cout << "光栅绘制 像素矩形 (" << x << "," << y << ") " << w << "x" << h << "\n";
    }
    std::string name() const override { return "光栅渲染器"; }
};

// ===== Abstraction：形状，持有渲染器引用（桥） =====
class Shape {
protected:
    std::shared_ptr<Renderer> _renderer;   // 桥：抽象层握着实现层
    explicit Shape(std::shared_ptr<Renderer> r) : _renderer(std::move(r)) {}
public:
    virtual ~Shape() = default;
    virtual void draw() const = 0;
    virtual void resize(double factor) = 0;

    // 运行时切换实现：桥的另一头可以换成另一套渲染器
    void setRenderer(std::shared_ptr<Renderer> r) { _renderer = std::move(r); }

    void rendererInfo() const {
        std::cout << "使用渲染器: " << _renderer->name() << "\n";
    }
};

// ===== RefinedAbstraction：圆 =====
class Circle : public Shape {
    double _x, _y, _r;
public:
    Circle(double x, double y, double r, std::shared_ptr<Renderer> renderer)
        : Shape(std::move(renderer)), _x(x), _y(y), _r(r) {}
    void draw() const override { _renderer->renderCircle(_x, _y, _r); }
    void resize(double factor) override { _r *= factor; }
};

// ===== RefinedAbstraction：矩形 =====
class Rectangle : public Shape {
    double _x, _y, _w, _h;
public:
    Rectangle(double x, double y, double w, double h, std::shared_ptr<Renderer> r)
        : Shape(std::move(r)), _x(x), _y(y), _w(w), _h(h) {}
    void draw() const override { _renderer->renderRectangle(_x, _y, _w, _h); }
    void resize(double factor) override { _w *= factor; _h *= factor; }
};

// ===== Client =====
int main() {
    auto vector = std::make_shared<VectorRenderer>();
    auto raster = std::make_shared<RasterRenderer>();

    Circle c(1.0, 2.0, 5.0, vector);
    c.draw();          // 矢量绘制 圆
    c.rendererInfo();

    Rectangle rect(0.0, 0.0, 10.0, 4.0, raster);
    rect.draw();       // 光栅绘制 像素矩形
    rect.resize(2.0);
    rect.draw();

    // 同一个圆，运行时换一套渲染器
    c.setRenderer(raster);
    c.draw();          // 光栅绘制 像素圆
}
