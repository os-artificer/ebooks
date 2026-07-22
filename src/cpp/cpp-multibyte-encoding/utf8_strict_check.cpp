// 示例 6：严格 UTF-8 校验（呼应 RFC 3629 红线：拒绝 overlong / 代理区 / 超范围）
// 用于演示 C1（Overlong 编码绕过校验）的反面——严格解码器如何挡住 C0 AF 等走私载荷。
// 编译：g++ -std=c++17 -O2 -Wall -Wextra utf8_strict_check.cpp -o bin/utf8_strict_check
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>

// 校验 n 个续字节（必须都是 10xxxxxx 格式），并还原码点
// 返回 true 表示续字节合法，cp 已更新
static bool check_continuation_bytes(std::string_view s, size_t start, int n, unsigned int& cp) {
    for (int k = 1; k <= n; ++k) {
        unsigned char d = s[start + k];
        if ((d & 0xC0) != 0x80) return false;   // 续字节格式错
        cp = (cp << 6) | (d & 0x3F);
    }
    return true;
}

bool utf8_is_valid_strict(std::string_view s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        int n;           // 需要的续字节数
        unsigned int cp; // 已还原的码点
        if      (c <= 0x7F)              { n = 0; cp = c; }
        // C0/C1 被排除 → 天然拒绝 1 字节字符的 overlong
        else if (c >= 0xC2 && c <= 0xDF) { n = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; cp = c & 0x0F; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; cp = c & 0x07; }
        // 非法首字节 / overlong 起点（如 C0 AF）
        else return false;
        // 续字节不足
        if (i + n >= s.size()) return false;
        if (!check_continuation_bytes(s, i, n, cp)) return false;
        // overlong 检测：码点可用更少字节表示
        if (n == 1 && cp < 0x80)    return false;
        if (n == 2 && cp < 0x800)   return false;
        if (n == 3 && cp < 0x10000) return false;
        // 代理区不可编码
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        // 超出 Unicode 上限
        if (cp > 0x10FFFF)          return false;
        i += n + 1;
    }
    return true;
}

int main() {
    // 测试用例：{ 名称, 字节序列, 期望结果 }
    struct Case { std::string_view name; std::string_view bytes; bool expect; };
    Case cases[] = {
        {"中文(UTF-8)",     "\xe4\xb8\xad\xe6\x96\x87", true},
        {"ASCII",           "AB",                       true},
        {"emoji(笑哭 U+1F600)", "\xf0\x9f\x98\x80",     true},
        {"NFD é(65 CC 81)", "\x65\xcc\x81",             true},
        {"overlong C0 AF",  "\xc0\xaf",                 false},
        {"代理区 U+D800",   "\xed\xa0\x80",             false},
        {"孤立续字节 80",   "\x80",                     false},
        {"截断的 中文A",    "\xe4\xb8\xad\xe6\x96",     false},
    };
    int fail = 0;
    for (auto& c : cases) {
        bool got = utf8_is_valid_strict(c.bytes);
        bool ok = (got == c.expect);
        if (!ok) ++fail;
        std::cout << (ok ? "[PASS] " : "[FAIL] ")
                  << c.name << " -> " << (got ? "合法" : "拒绝") << "\n";
    }
    std::cout << (fail == 0 ? "全部通过" : "存在失败用例") << "\n";
    return fail == 0 ? 0 : 1;
}
