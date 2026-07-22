// 示例 7：用 ICU 的 Normalizer2 把 NFD 规范化为 NFC（消除 C2 的等价字符绕过）
// 需安装 ICU 开发库；编译：g++ -std=c++17 -O2 -licuuc -licudata c2_normalize.cpp -o bin/c2_normalize
#include <iostream>
#include <string>
#include <unicode/normalizer2.h>
#include <unicode/unistr.h>

int main() {
    // 分解态: 'e'(U+0065) + 组合重音 '◌́'(U+0301)，用码点拼接，不依赖源文件编码
    icu::UnicodeString nfd;
    nfd.append(static_cast<UChar32>(0x65));    // 'e'
    nfd.append(static_cast<UChar32>(0x301));   // 组合重音 ◌́

    UErrorCode err = U_ZERO_ERROR;
    const icu::Normalizer2* nfc = icu::Normalizer2::getNFCInstance(err);
    if (U_FAILURE(err)) {
        std::cerr << "获取 NFC 实例失败: " << u_errorName(err) << "\n";
        return 1;
    }

    icu::UnicodeString out;
    nfc->normalize(nfd, out, err);
    if (U_FAILURE(err)) {
        std::cerr << "规范化失败: " << u_errorName(err) << "\n";
        return 1;
    }

    // 合成态 é (U+00E9, UTF-8: C3 A9)
    std::string nfd_utf8, nfc_utf8;
    nfd.toUTF8String(nfd_utf8);
    out.toUTF8String(nfc_utf8);

    std::cout << "NFD 字节 : ";
    for (unsigned char b : nfd_utf8) std::cout << std::hex << (int)b << " ";
    std::cout << "\nNFC 字节 : ";
    for (unsigned char b : nfc_utf8) std::cout << std::hex << (int)b << " ";
    std::cout << "\nNFC UTF-8 : " << nfc_utf8 << std::dec << "\n";
    return 0;
}
