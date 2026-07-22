// 示例 3：用 C++ 标准库 <codecvt> 在 UTF-8 与宽字符之间转换
// 并演示 wchar_t 的平台差异。注意：<codecvt> 在 C++17 被标为 deprecated，
// 但主流编译器仍支持，适合理解标准库思路；新项目建议用第三方库(ICU)或自写。
// 编译：g++ -std=c++17 -O2 -Wall -Wextra locale_codecvt.cpp -o bin/locale_codecvt
#include <codecvt>
#include <iostream>
#include <locale>
#include <string>

int main() {
    // 关键事实：wchar_t 的宽度因平台而异
    std::cout << "sizeof(wchar_t) = " << sizeof(wchar_t) << " 字节\n";
#ifdef _WIN32
    std::cout << "Windows 上 wchar_t 是 UTF-16(2 字节)\n";
#else
    std::cout << "Linux/macOS 上 wchar_t 是 UTF-32(4 字节)\n";
#endif

    // UTF-8 源码字面量：6 + 1 = 7 字节
    std::string u8 = "中文A";

    // C++11 起的标准转换工具（现已 deprecated）
    std::wstring_convert<std::codecvt_utf8<wchar_t>> cvt;
    std::wstring w = cvt.from_bytes(u8);

    std::cout << "UTF-8 字节数   : " << u8.size() << "\n";
    std::cout << "宽字符串码点数 : " << w.size() << "  (每个码点 = 1 个 wchar_t)\n";

    // 转回去
    std::string back = cvt.to_bytes(w);
    std::cout << "往返结果: " << back << "  一致: " << (back == u8 ? "是" : "否") << "\n";

    // locale 的影响：宽字符分类/大小写等函数依赖全局 locale
    // 设为用户环境 locale(通常是 UTF-8)
    std::locale::global(std::locale(""));
    std::wcout << L"宽字符输出示例: " << w << L"\n";

    return 0;
}
