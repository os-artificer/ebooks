// 示例 1：UTF-8 的"字节数 != 字符数"，以及按字节索引的陷阱
// 编译：g++ -std=c++17 -O2 -Wall -Wextra utf8_basics.cpp -o bin/utf8_basics
#include <cstddef>
#include <iostream>
#include <string>

// 计算 UTF-8 字符串中的"码点数"（即人眼看到的字符数）
// 规则：非 0x80~0xBF 开头的字节，都是某个码点的首字节
std::size_t utf8_codepoint_count(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++n;  // 跳过续字节 10xxxxxx
    }
    return n;
}

int main() {
    std::string s = "中文AB";  // 2 个汉字(各3字节) + 2 个 ASCII

    std::cout << "字符串内容: " << s << "\n";
    std::cout << "std::string::size() 字节数: " << s.size() << "\n";
    std::cout << "UTF-8 码点数: " << utf8_codepoint_count(s) << "\n";

    // 陷阱：按字节取第 1 个字符（下标 0..2 才是一个完整汉字）
    std::cout << "s[0] 单独打印(截断的字节): ";
    std::cout << "\\x" << std::hex
              << static_cast<int>(static_cast<unsigned char>(s[0])) << std::dec << "\n";

    // 危险操作：从字节中间切分
    std::string half = s.substr(1);  // 从"中"的第 2 个字节开始
    std::cout << "s.substr(1) 结果(出现乱码/替换符): " << half << "\n";

    return 0;
}
