// 示例 2：跨平台编码转换
//   - POSIX(macOS/Linux)：用 iconv 在 UTF-8 与 GBK(典型多字节中文编码) 之间互转
//   - Windows：用 Win32 API 在 UTF-8 与本机 ANSI 代码页(UTF-16 中转)之间互转
// 编译(POSIX)：g++ -std=c++17 -O2 -Wall -Wextra encoding_convert.cpp -o bin/encoding_convert -liconv
// 编译(Windows)：cl /std:c++17 /EHsc encoding_convert.cpp /link /out:encoding_convert.exe
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>

// UTF-8 -> 本机宽字符(UTF-16)
std::wstring utf8_to_wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    // 去掉结尾的 L'\0'
    w.pop_back();
    return w;
}

// 本机宽字符(UTF-16) -> 当前 ANSI 代码页的多字节串
std::string wide_to_ansi(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    s.pop_back();
    return s;
}

// 当前 ANSI 代码页多字节串 -> 本机宽字符(UTF-16)
std::wstring ansi_to_wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, w.data(), n);
    w.pop_back();
    return w;
}

// 本机宽字符(UTF-16) -> UTF-8
std::string wide_to_utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    s.pop_back();
    return w;
}

// 对外统一入口：UTF-8 <-> 当前 ANSI 代码页
std::string utf8_to_ansi(const std::string& u8) { return wide_to_ansi(utf8_to_wide(u8)); }
std::string ansi_to_utf8(const std::string& ansi) { return wide_to_utf8(ansi_to_wide(ansi)); }

#else
#include <iconv.h>

// 处理 iconv 转换中的错误字节（EILSEQ/EINVAL 时跳过 1 字节）
static void skip_one_byte(char*& inbuf, size_t& inleft) {
    if (inleft > 0) { ++inbuf; --inleft; }
}

// 通用 iconv 转换(from -> to 是 iconv 支持的编码名，如 "UTF-8"、"GBK")
std::string iconv_convert(const std::string& in, const char* to, const char* from) {
    iconv_t cd = iconv_open(to, from);
    if (cd == (iconv_t)-1) {
        std::cerr << "iconv_open 失败: " << from << " -> " << to << "\n";
        return {};
    }
    std::string out;
    out.reserve(in.size() * 2 + 8);
    char* inbuf = const_cast<char*>(in.data());
    size_t inleft = in.size();

    char buf[256];
    while (inleft > 0) {
        char* outbuf = buf;
        size_t outleft = sizeof(buf);
        size_t r = iconv(cd, &inbuf, &inleft, &outbuf, &outleft);
        out.append(buf, sizeof(buf) - outleft);
        if (r != (size_t)-1) continue;          // 正常转换完成本轮
        if (errno == E2BIG) continue;            // 输出缓冲不够，继续写
        if (errno == EINVAL || errno == EILSEQ) {
            skip_one_byte(inbuf, inleft);         // 非法/不完整字节：跳过
            continue;
        }
        break;                                   // 未知错误
    }
    iconv_close(cd);
    return out;
}

std::string utf8_to_gbk(const std::string& u8) { return iconv_convert(u8, "GBK", "UTF-8"); }
std::string gbk_to_utf8(const std::string& gbk) { return iconv_convert(gbk, "UTF-8", "GBK"); }

// 对外统一入口(演示用：UTF-8 <-> GBK)
std::string utf8_to_ansi(const std::string& u8) { return utf8_to_gbk(u8); }
std::string ansi_to_utf8(const std::string& ansi) { return gbk_to_utf8(ansi); }
#endif

// 把字节序列打印成十六进制，便于核对编码
void dump_hex(const std::string& label, const std::string& s) {
    std::cout << label << " (len=" << s.size() << "): ";
    for (unsigned char c : s) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(c) << " ";
    }
    std::cout << std::dec << "\n";
}

int main() {
    // 源码以 UTF-8 保存，此字面量是 6 个 UTF-8 字节
    std::string u8 = "中文";

#ifdef _WIN32
    dump_hex("UTF-8 原文 ", u8);
    // 转成本机 ANSI 代码页(如 GBK)
    std::string ansi = utf8_to_ansi(u8);
    dump_hex("ANSI(本机代码页)", ansi);
    // 转回 UTF-8
    std::string back = ansi_to_utf8(ansi);
    std::cout << "转回 UTF-8: " << back << "\n";
#else
    dump_hex("UTF-8 原文 ", u8);
    std::string gbk = utf8_to_gbk(u8);
    // 期望: d6 d0 ce c4
    dump_hex("GBK 编码  ", gbk);
    std::string back = gbk_to_utf8(gbk);
    std::cout << "转回 UTF-8: " << back << "\n";
    std::cout << "往返一致: " << (back == u8 ? "是" : "否") << "\n";
#endif
    return 0;
}
