// 示例 5：网络通信中的编码处理——带编码声明的 TCP 消息帧
// 演示自定义 TCP 协议中如何正确处理 UTF-8 编码的文本消息：
//   - 消息帧结构（4 字节长度前缀 + UTF-8 载荷）
//   - 构造帧、解析帧、处理不完整帧
//   - 强调 payload_len 是字节数而非字符数
// 编译：g++ -std=c++17 -O2 -Wall -Wextra network_encoding.cpp -o bin/network_encoding
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

// ── 消息帧结构（4 字节长度前缀 + UTF-8 载荷）──
#pragma pack(push, 1)
struct MessageFrame {
    uint32_t payload_len;    // 载荷字节长度（网络字节序）
    char     payload[];       // UTF-8 编码的文本内容
};
#pragma pack(pop)

// 将主机字节序转为网络字节序（大端）
uint32_t host_to_network(uint32_t x) {
    return ((x & 0xFF) << 24) |
           ((x & 0xFF00) << 8)  |
           ((x & 0xFF0000) >> 8) |
           ((x & 0xFF000000) >> 24);
}

// 将网络字节序转为主机字节序
uint32_t network_to_host(uint32_t n) {
    // 同一个操作，对称的
    return host_to_network(n);
}

// 构造一条待发送的消息帧（内部字符串 → UTF-8 帧）
std::vector<uint8_t> build_frame(const std::string& text_utf8) {
    // 假设传入的 text_utf8 已经是 UTF-8 编码
    // 如果不是，这里先做转码：text_utf8 = gbk_to_utf8(raw_input);
    uint32_t len = host_to_network(static_cast<uint32_t>(text_utf8.size()));
    std::vector<uint8_t> frame(sizeof(uint32_t) + text_utf8.size());
    std::memcpy(frame.data(), &len, sizeof(uint32_t));
    std::memcpy(frame.data() + sizeof(uint32_t), text_utf8.data(), text_utf8.size());
    return frame;
}

// 从接收缓冲区解析消息帧（帧 → UTF-8 字符串）
// 返回 {解析出的文本, 消耗的字节数}; 若帧不完整返回 {"", 0}
std::pair<std::string, size_t> parse_frame(const uint8_t* buf, size_t buf_len) {
    // 长度字段都不够
    if (buf_len < sizeof(uint32_t)) return {"", 0};

    uint32_t net_len;
    std::memcpy(&net_len, buf, sizeof(uint32_t));
    uint32_t payload_len = network_to_host(net_len);

    // 安全检查：防止恶意超大值
    if (payload_len > 10 * 1024 * 1024) {
        std::cerr << "载荷长度异常: " << payload_len << "\n";
        return {"", 0};
    }
    // 帧不完整
    if (buf_len < sizeof(uint32_t) + payload_len) return {"", 0};

    // 提取 UTF-8 载荷
    std::string text(reinterpret_cast<const char*>(buf + sizeof(uint32_t)), payload_len);
    return {text, sizeof(uint32_t) + payload_len};
}

void dump_hex(const std::string& label, const std::vector<uint8_t>& data) {
    std::cout << label << " (" << data.size() << " bytes): ";
    for (auto b : data) {
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(b) << " ";
    }
    std::cout << std::dec << "\n";
}

int main() {
    // 模拟：构造一条包含中文的 UTF-8 消息并发送
    // 内部统一 UTF-8（"你好，服务器" = 3+3+3+3+3+3 = 18 字节）
    std::string msg = "你好，服务器";

    auto frame = build_frame(msg);
    dump_hex("发送帧", frame);

    // 模拟：服务端接收并解析
    auto [text, consumed] = parse_frame(frame.data(), frame.size());
    std::cout << "解析出文本: " << text << "\n";
    std::cout << "消耗字节数: " << consumed << "\n";

    // 模拟：帧不完整的情况（只收到部分数据）
    auto [text2, consumed2] = parse_frame(frame.data(), 3);  // 只收到 3 字节
    std::cout << "不完整帧解析: \"" << text2 << "\", 消耗=" << consumed2 << "\n";

    return 0;
}
