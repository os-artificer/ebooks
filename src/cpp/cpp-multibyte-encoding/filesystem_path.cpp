// 示例 4：std::filesystem::path 的多种"编码视图"
// 同一个路径对象，可以按不同编码取出字符串，理解这一点能避免很多乱码/找不到文件的问题。
// 编译：g++ -std=c++17 -O2 -Wall -Wextra filesystem_path.cpp -o bin/filesystem_path
#include <filesystem>
#include <iostream>

int main() {
    namespace fs = std::filesystem;

    // 源码 UTF-8，路径名含中文
    fs::path p = "文档/报告.txt";

    std::cout << "原生格式  string() : " << p.string() << "\n";
    std::cout << "UTF-8     u8string(): " << p.u8string() << "\n";
    std::wcout << L"宽字符    wstring() : " << p.wstring() << L"\n";

#ifdef _WIN32
    // Windows 原生是 UTF-16：string() 走 ANSI 代码页，中文可能丢信息；
    // 要无损应优先用 wstring() 或 u8string()。
    std::cout << "Windows: string() 经 ANSI 代码页，可能丢失无法映射的字符\n";
#else
    // POSIX 原生就是 UTF-8：string() 与 u8string() 此刻等价
    std::cout << "POSIX: 原生即 UTF-8，string() 与 u8string() 一致\n";
#endif

    // 一个小提醒：用 path 拼路径比手工拼字符串安全，且自动处理分隔符
    fs::path full = p.parent_path() / "子目录" / "结果.txt";
    std::cout << "拼接结果: " << full.string() << "\n";
    return 0;
}
