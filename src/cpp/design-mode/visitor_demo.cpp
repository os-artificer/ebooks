// Visitor 模式示例：文档导出（标题/段落/图片 × HTML导出/Markdown导出/字数统计）
// 编译：在 src/cpp 目录执行 make bin/visitor_demo
#include <cstddef>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// ---- 前向声明具体元素（Visitor 接口要按类型备齐重载）----
class Heading;
class Paragraph;
class Image;

// ---- Visitor 接口：为每一种具体元素声明一个 visit 重载 ----
class DocVisitor {
public:
    virtual ~DocVisitor() = default;
    virtual void visit(const Heading& e) = 0;
    virtual void visit(const Paragraph& e) = 0;
    virtual void visit(const Image& e) = 0;
    virtual std::string name() const = 0;
};

// ---- Element 接口：只留一个 accept 入口 ----
class DocElement {
public:
    virtual ~DocElement() = default;
    virtual void accept(DocVisitor& v) const = 0;
};

// ---- ConcreteElement：标题 ----
class Heading : public DocElement {
    std::string _text;
    int _level;
public:
    Heading(std::string text, int level) : _text(std::move(text)), _level(level) {}
    const std::string& text() const { return _text; }
    int level() const { return _level; }
    void accept(DocVisitor& v) const override { v.visit(*this); }   // 双重分派第二跳
};

// ---- ConcreteElement：段落 ----
class Paragraph : public DocElement {
    std::string _text;
public:
    explicit Paragraph(std::string text) : _text(std::move(text)) {}
    const std::string& text() const { return _text; }
    void accept(DocVisitor& v) const override { v.visit(*this); }
};

// ---- ConcreteElement：图片 ----
class Image : public DocElement {
    std::string _path;
    std::string _alt;
public:
    Image(std::string path, std::string alt)
        : _path(std::move(path)), _alt(std::move(alt)) {}
    const std::string& path() const { return _path; }
    const std::string& alt() const { return _alt; }
    void accept(DocVisitor& v) const override { v.visit(*this); }
};

// ---- ConcreteVisitor：HTML 导出（结果累积在内部）----
class HtmlExportVisitor : public DocVisitor {
    std::string _out;
public:
    void visit(const Heading& e) override {
        _out += "<h" + std::to_string(e.level()) + ">" + e.text()
              + "</h" + std::to_string(e.level()) + ">\n";
    }
    void visit(const Paragraph& e) override {
        _out += "<p>" + e.text() + "</p>\n";
    }
    void visit(const Image& e) override {
        _out += "<img src=\"" + e.path() + "\" alt=\"" + e.alt() + "\">\n";
    }
    std::string name() const override { return "HTML导出"; }
    std::string result() const { return _out; }
};

// ---- ConcreteVisitor：Markdown 导出 ----
class MarkdownExportVisitor : public DocVisitor {
    std::string _out;
public:
    void visit(const Heading& e) override {
        _out += std::string(static_cast<size_t>(e.level()), '#') + " " + e.text() + "\n\n";
    }
    void visit(const Paragraph& e) override {
        _out += e.text() + "\n\n";
    }
    void visit(const Image& e) override {
        _out += "![" + e.alt() + "](" + e.path() + ")\n\n";
    }
    std::string name() const override { return "Markdown导出"; }
    std::string result() const { return _out; }
};

// 按 UTF-8 码点计数（跳过续字节），中文按 1 字计
static size_t utf8Len(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

// ---- ConcreteVisitor：字数统计 ----
class WordCountVisitor : public DocVisitor {
    size_t _chars = 0;
public:
    void visit(const Heading& e) override { _chars += utf8Len(e.text()); }
    void visit(const Paragraph& e) override { _chars += utf8Len(e.text()); }
    void visit(const Image& e) override { _chars += utf8Len(e.alt()); }
    std::string name() const override { return "字数统计"; }
    size_t result() const { return _chars; }
};

// ---- ObjectStructure：文档，持有元素集合并挨个派发 visitor ----
class Document {
    std::vector<std::unique_ptr<DocElement>> _elements;
public:
    void add(std::unique_ptr<DocElement> e) { _elements.push_back(std::move(e)); }
    void accept(DocVisitor& v) const {
        std::cout << "--- " << v.name() << " ---\n";
        for (const auto& e : _elements)
            e->accept(v);   // 双重分派第一跳：按元素类型进 accept
    }
};

int main() {
    Document doc;
    doc.add(std::make_unique<Heading>("C++ 设计模式", 1));
    doc.add(std::make_unique<Paragraph>("访问者模式把操作从元素里搬出去。"));
    doc.add(std::make_unique<Image>("uml.png", "类图"));

    HtmlExportVisitor html;
    doc.accept(html);
    std::cout << html.result() << "\n";

    MarkdownExportVisitor md;
    doc.accept(md);
    std::cout << md.result() << "\n";

    WordCountVisitor wc;
    doc.accept(wc);
    std::cout << "总字符数: " << wc.result() << "\n";

    return 0;
}
