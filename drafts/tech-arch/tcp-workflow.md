## 从三次握手到四次挥手：一文搞懂TCP的"一生"

<p align="center"><strong>作者：</strong>Artificer老王 &nbsp;&nbsp;|&nbsp;&nbsp; <strong>更新时间：</strong>2026-07-11 &nbsp;&nbsp;|&nbsp;&nbsp; <strong>阅读时长：</strong>约 15 分钟</p>

---

你有没有想过，当你在浏览器输入一个网址按下回车，到网页显示出来，这中间发生了什么？

很多人知道"TCP 是可靠的"、"有三次握手和四次挥手"，但真要细问：

**为什么是三次不是两次？**  
**TIME_WAIT 状态为什么要等 2MSL？**  
**拥塞控制到底是怎么避免网络拥堵的？**

这些问题，面试时经常被问到，但很少有人能讲清楚。

今天这篇文章，我们就从**最底层**出发，把 TCP 的**连接建立 → 数据传输 → 连接断开**这条完整链路彻底搞懂。

---

## TCP 三次握手：连接是如何建立的？

### 为什么要三次握手？

先看一个生活中的类比：

> 你打电话给朋友：
> - **你说**："喂，能听到我说话吗？"（第一次）
> - **朋友说**："能听到，你能听到我吗？"（第二次）
> - **你说**："能听到了！"（第三次）

**三次确认**，双方都确信：
- 我能发消息给你 ✅
- 我能收到你的消息 ✅
- 你也能收到我的消息 ✅

如果是两次握手会怎样？

```
CLIENT: "我能听到你吗？"
Server:  "我能收到你了！"
        ↑
        这时 Server 认为"双向通信已建立"
        但 Client 还不知道 Server 能不能收到自己的 ACK！
```

**两次握手的问题**：Server 会误以为连接已经建立，但 Client 可能根本没收到 Server 的回复。这时如果 Client 重发 SYN，Server 就会建立多个无效连接。

**四次握手可以吗？**

理论上可以，但没必要。第二次握手（SYN+ACK）把 Server 的"我能收到你"和"你能收到我吗"合并成一条消息发送了，**节省一次 RTT（往返时间）**。

所以，**三次是理论上的最小值**。

### 完整的三次握手过程

下面是标准的 TCP 三次握手过程（假设 Client 初始序列号为 `x`，Server 为 `y`）：

```
    CLIENT                          Server
      |                               |
      |   [状态: CLOSED]              |   [状态: LISTEN]
      |                               |
      |---------- SYN ----------------|  seq=x, syn=1
      |   [CLOSED -> SYN_SENT]--------|   Client 发送连接请求
      |                               |---> [状态: LISTEN -> SYN_RCVD]
      |<--------- SYN+ACK ------------|  seq=y, ack=x+1, syn=1, ack=1
      |       (seq=y, ack=x+1)        |
      |          |                    |
      |---------- ACK ----------------|  seq=x+1, ack=y+1 (ACK标志位置位)
      |                               |---> [状态: SYN_RCVD -> ESTABLISHED]
      |---> [SYN_SENT -> ESTABLISHED] |   Server 状态也变为 ESTABLISHED
      |                               |
      |         ✅ 连接建立成功        |
      |     双方进入 ESTABLISHED       |
      |     可以开始传数据了!          |
```

**每次握手的详细说明**：

#### 第一次握手：Client 发送 SYN

```text
报文内容:
┌─────────────────────────────────────┐
│  SYN = 1                            │  # 标记为同步报文
│  seq = x (随机数, 如 1000)           │  # 客户端初始序列号
│  MSS = 1460                         │  # 告诉对方我能接收的最大段长度
│  Window Scale = 7                   │  # 窗口缩放因子(可选)
│  SACK Permitted                     │  # 支持选择性确认
└─────────────────────────────────────┘

Client 状态变化: CLOSED --> SYN_SENT
含义: "我已发送连接请求,等待 Server 确认"
```

**ISN（初始序列号）为什么要随机？**
> 如果 ISN 固定或可预测，攻击者可以猜测当前连接的序列号，伪造 RST 报文或注入恶意数据，劫持现有连接（即 **Blind Connection Hijacking / TCP 连接劫持攻击**）。随机化 ISN 后，攻击者无法猜到正确的序列号，安全性大大提升。

#### 第二次握手：Server 回复 SYN+ACK

```text
报文内容:
┌─────────────────────────────────────┐
│  SYN = 1, ACK = 1                   │  # 同步 + 确认
│  seq = y (随机数, 如 2000)           │  # 服务端初始序列号
│  ack = x + 1 (1001)                 │  # 确认收到 Client 的 SYN
│  MSS = 1460                         │  # 服务端的 MSS
└─────────────────────────────────────┘

Server 状态变化: LISTEN --> SYN_RCVD
Client 状态保持: SYN_SENT (等待中)
含义: Server 已收到请求,并同意建立连接
```

#### 第三次握手：Client 发送 ACK

```text
报文内容:
┌─────────────────────────────────────┐
│  ACK = 1                            │  # 确认报文
│  seq = x + 1 (1001)                │  # 下一个要发的序列号
│  ack = y + 1 (2001)                │  # 确认收到 Server 的 SYN
└─────────────────────────────────────┘

Client 状态变化: SYN_SENT --> ESTABLISHED
Server 状态变化: SYN_RCVD --> ESTABLISHED (收到 ACK 后)
含义: "双向通信通道正式打通!"
```

### 状态变化速查表

| 阶段 | Client 状态 | Server 状态 | 说明 |
|------|------------|------------|------|
| 初始 | **CLOSED** | **LISTEN** | Server 先调用 `listen()` 进入监听 |
| 第一次握手后 | **SYN_SENT** | **LISTEN** | Client 发出 SYN，等待回复 |
| 第二次握手后 | **SYN_SENT** | **SYN_RCVD** | Server 收到 SYN，回复 SYN+ACK |
| 第三次握手后 | **ESTABLISHED** | **ESTABLISHED** | ✅ 连接建立完成 |

> 💡 **小知识**：`LISTEN` 状态只能通过 `listen()` 系统调用进入，不能通过其他状态转换而来。这就是为什么服务器必须先启动监听，客户端才能连接。

### 用 tcpdump 抓包观察三次握手

在 Linux 上，你可以用 `tcpdump` 实际抓取三次握手的报文：

```bash
# 在终端 A 启动一个简单的 HTTP 服务
python3 -m http.server 8080

# 在终端 B 用 tcpdump 抓包
sudo tcpdump -i lo port 8080 -nn -S

# 在终端 C 发起连接
curl http://localhost:8080/
```

抓包结果示例（简化版）：

```text
# 第一次握手
IP 127.0.0.1.54321 > 127.0.0.1.8080: Flags [S], seq 123456789
# 第二次握手
IP 127.0.0.1.8080 > 127.0.0.1.54321: Flags [S.], seq 987654321, ack 123456790
# 第三次握手
IP 127.0.0.1.54321 > 127.0.0.1.8080: Flags [.], ack 987654322
```

**关键字解释**：
- `[S]` = SYN（同步标志）
- `[S.]` = SYN+ACK（同步+确认）
- `[.]` = ACK（纯确认，无数据）

### 握手时的选项协商

三次握手不只是"打招呼"，还会交换很多重要参数：

| 选项 | 作用 | 示例值 |
|------|------|--------|
| **MSS**（最大报文段长度） | 避免分段，提高效率 | 1460 字节（MTU 1500 - 头部 40） |
| **Window Scale** | 窗口缩放因子（突破 65535 限制） | 7（窗口 × 128） |
| **SACK Permitted** | 是否支持选择性确认 | 开启 |
| **Timestamps** | 用于 RTT 测量和 PAWS 保护 | 开启（握手协商后每个数据包都携带） |

> ⚠️ **注意**：MSS、Window Scale、SACK Permitted 等选项**只能在三次握手时协商**，连接建立后就固定了。如果你想调整这些参数，只能重新建连。但 **Timestamps** 比较特殊——它不仅在握手时协商启用，之后**每个数据包都会携带时间戳字段**用于实时 RTT 计算和 PAWS（防止旧数据干扰新连接）。

---

## 数据传输：TCP 如何保证可靠？

连接建立后，TCP 开始传输数据。
但网络是不可靠的——数据包可能丢失、乱序、重复。

**TCP 如何保证"可靠"？**

核心机制有三个：
1. **序列号与确认应答**（保证有序、不丢）
2. **滑动窗口流量控制**（防止发送方发太快）
3. **拥塞控制**（防止把网络搞堵了）

我们逐个讲解。

### 序列号与确认应答：给每个字节编号

TCP 给**每个字节的数据**都编上号（序列号），接收方收到后会回复一个**确认号（ACK）**告诉发送方："我已经收到了序号 X 之前的所有数据，下一个请发 X"。

**通俗类比**：快递单号

```
发送方: 发送 [1000-1999] 共 1000 字节
        ↓
接收方: 收到完整数据
        ↓
回复: ACK=2000 ("请继续发送从 2000 开始的数据")
```

**实际抓包示例**：

```text
# 发送方发出 1448 字节数据
IP client > server: Flags [P.], seq 1000:2448, ack 1, length 1448
                                        ^^^^^^^^^^^^
                                        数据范围: 1000~2447

# 接收方确认收到
IP server > client: Flags [.], ack 2448
                              ^^^^^
                              "下一个请发 2448"
```

**关键点**：
- `seq` 表示**本段数据的起始序列号**
- `ack` 表示**期望收到的下一个序列号**（即已收到 `ack-1` 及之前的所有数据）
- 如果中间丢了包，`ack` 会**重复**（重复 ACK），触发重传机制

### 滑动窗口：让发送更高效

如果每发送一个字节就等一个 ACK，那效率太低了！

**滑动窗口**允许发送方**连续发送多个包**而不必等待每个 ACK，只要这些包的总大小不超过窗口大小即可。

```
发送方视角:

已发送且已确认    已发送未确认    可发送但不能超    超出窗口
[████████████]    [███████████]    [░░░░░░░░░░]    [..........]
  0 ~ 999        1000 ~ 2499     2500 ~ 4499      4500+
                  (在飞行中)      (可用窗口)

接收方窗口 = 3000 字节
当前可用窗口 = 3000 - 1500 = 1500 字节
```

**窗口如何动态调整？**

接收方会在每个 ACK 中通告自己的**接收窗口（rwnd）**：

```text
IP server > client: Flags [.], ack 2448, win 29200
                                              ^^^^^^
                                              "我的接收缓冲区还能装 29200 字节"
```

如果接收方处理不过来，它会**缩小窗口**甚至设为 0，迫使发送方暂停（零窗口探询）。

> 💡 **实战命令**：查看当前连接的窗口大小
> ```bash
> ss -ti state established sport = :80
> ```
> 输出中的 `wnd_scale:` 和 `rcv_space:` 就是窗口相关信息。

### 拥塞控制：不要把网络堵死了

即使接收方能跟上，但如果网络中间的路由器、交换机被海量数据淹没，还是会丢包。

**拥塞控制**就是发送方的"自觉限速"，根据网络的拥堵程度动态调整发送速率。

拥塞窗口（**cwnd**）是核心参数，它表示**在不引起拥堵的前提下，最多能有多少字节在网络中传输**。

#### 慢启动（Slow Start）：指数增长

刚建立连接时，cwnd 从小开始（通常 2-10 个 MSS），然后**每收到一个 ACK，cwnd 就增加 1 个 MSS**，导致每个 RTT 大约**翻倍**：

```
cwnd 变化过程 (初始 cwnd=10):

RTT 0:  cwnd = 10  [*]
RTT 1:  cwnd = 20  [**]
RTT 2:  cwnd = 40  [****]
RTT 3:  cwnd = 80  [********]
RTT 4:  cwnd = 160 [****************]
       |
       v
到达 ssthresh (慢启动阈值)
转入拥塞避免阶段
```

**为什么叫"慢启动"？其实一点都不慢！**

这个名字是相对于直接发送大窗口而言的。实际上慢启动阶段的增长速度非常快（指数级），目的是快速探测网络的承载能力。

#### 拥塞避免（Congestion Avoidance）：线性增长

当 cwnd 达到 **ssthresh（慢启动阈值）** 后，转为线性增长（每个 RTT 只增加 1 个 MSS），避免快速增长导致突然拥塞：

```
cwnd 变化过程 (ssthresh=160):

RTT 4:  cwnd = 160  [****************]
RTT 5:  cwnd = 161  [ ****************]
RTT 6:  cwnd = 162  [  *************** ]
...
RTT 20: cwnd = 175  [   **************  ]  ← 缓慢增长
       |
       v
如果发生丢包 (超时或重复ACK):
cwnd 直接降到 1 或 ssthresh/2!
```

#### 快重传（Fast Retransmit）：不等超时

传统的超时重传要等很久（几百毫秒到几秒）。**快重传**利用重复 ACK 快速发现丢包：

```
正常情况:
发送: [pkt1] [pkt2] [pkt3] [pkt4] [pkt5]
确认:  ACK1   ACK2   ACK3   ACK4   ACK5  ← 依次确认

pkt2 丢失的情况:
发送: [pkt1] [pkt2✗] [pkt3] [pkt4] [pkt5]
确认:  ACK2   ACK2   ACK2   ACK2   ACK3
              ^^^^^^^^^^^^^^^^^^^^
              收到 3 个重复 ACK2!
              → 触发快重传，立即重传 pkt2
              （不用等到超时！）
              ↑ 最后一个 ACK3: 接收方收到重传的 pkt2 后,
                此时 [1]~[5] 全部到齐, 回复最终确认
```

**规则**：收到 **3 个重复 ACK** 就认为该包丢失，立即重传。

#### 快恢复（Fast Recovery）：不完全从头来

传统方式：超时后 cwnd 降为 1，重新慢启动（太保守）。
快恢复方式：只适度降低 cwnd，然后跳过慢启动，直接进入拥塞避免：

```
丢包前: cwnd = 32 (单位: MSS)

传统超时重传:
cwnd = 1 → 重新慢启动... (太慢!)

快恢复:
ssthresh = cwnd / 2 = 16
cwnd = ssthresh + 3 = 19  (收到 3 个重复 ACK，适当降低)
→ 直接进入拥塞避免 (比慢启动快得多)
```

#### 拥塞控制全景图

```text
cwnd
 64 |                                    *
    |                                   * *
 32 |                                  *   *
    |                                 *     *
 16 |-------*-----------------*-------*       *---- (ssthresh)
    |      *                 * *             * *
  8 |     *                 *   *           *   *
    |    *                 *     *         *     *
  4 |   * * * * * * * *   *       *       *       *
  2 |* *               * *         *   * *         *
  1 |*               *             *   *             *
    +--------------------------------------------------> 时间
      慢启动(指数↑)   拥塞避免(线性↑)  丢包→快恢复→拥塞避免
```

### 超时与重传：最后的保障

即使有了快重传，某些场景下还是需要超时重传（比如只发了 1 个包就丢了，没有后续包触发重复 ACK）。

**RTO（Retransmission Timeout）** 的计算采用经典的 Jacobson/Karels 算法：

```bash
# 查看 RTO 相关参数
sysctl net.ipv4.tcp_rto_min            # 最小 RTO (默认 200ms)
sysctl net.ipv4.tcp_rto_max            # 最大 RTO (默认 120s)
sysctl net.ipv4.tcp_retries2           # 最大重传次数 (默认 15 次)
```

**RTO 动态调整规律**：

```text
第一次重传: RTO = 1 秒 (举例)
第二次重传: RTO = 2 秒 (×2)
第三次重传: RTO = 4 秒 (×2)
第四次重传: RTO = 8 秒 (×2)... 指数退避
```

> ⚠️ **注意**：Linux 默认最多重传 15 次 (`net.ipv4.tcp_retries2`)，之后强制关闭连接。

### SACK 选择性确认：精准定位丢包

传统 ACK 只能告诉发送方"我收到了序号 X 之前的所有数据"，但如果中间丢了两个包，发送方只能按顺序重传所有后续数据（Go-Back-N 策略）。

**SACK（Selective ACK）** 允许接收方精确告知哪些块收到了、哪些没收到：

```text
没有 SACK:
发送: [1] [2] [3✗] [4✗] [5]
ACK:  ACK3 ("我只收到了 1,2")
→ 发送方必须重传 [3][4][5] (虽然 4,5 可能已收到!)

使用 SACK:
ACK:  ACK3, SACK Blocks=[4-5] ("1,2 收到了, 3 丢了, 4,5 已收到")
→ 发送方只需重传 [3] (高效!)
```

**检查是否启用 SACK**：

```bash
cat /proc/sys/net/ipv4/tcp_sack
# 输出 1 表示开启 (现代 Linux 默认开启)
```

### Nagle 算法与小包问题

如果应用频繁发送小数据（比如每次只发 1 字节），会产生大量的小包，严重浪费带宽。

**Nagle 算法**的规则：
1. 如果有待确认的数据（之前的包还没收到 ACK），先把新数据缓存起来
2. 只有收到 ACK 或数据积累到 MSS 大小时才发送

```text
没有 Nagle:
App write("H") → [1字节] → 网络
App write("e") → [1字节] → 网络
App write("l") → [1字节] → 网络
App write("l") → [1字节] → 网络
App write("o") → [1字节] → 网络
结果: 5个小包!

使用 Nagle:
App write("H") → [缓存]
App write("e") → [缓存]
App write("l") → [缓存]
App write("l") → [缓存]
App write("o") → [达到阈值或超时] → ["Hello"] 一次性发出
结果: 1个包!
```

> 💡 **什么时候应该禁用 Nagle？**
> 
> 对于实时性要求高的场景（如 SSH 终端、在线游戏），小延迟比带宽效率更重要。
> 可以通过 `TCP_NODELAY` 选项禁用：
> ```c
> setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
> ```

---

## 四次挥手：连接如何优雅地断开？

TCP 是**全双工协议**，意味着数据可以同时双向传输。
断开连接时，**每一方都需要单独关闭自己的发送方向**。

所以需要**四次挥手**（实际上是两对 FIN/ACK）。

### 为什么是四次而不是三次？

```
类比: 两人通话结束

A: "我说完了,你要说完吗?" (FIN)
B: "知道了" (ACK) -- B 可能还有话要说
B: "我也说完了" (FIN)
A: "好的,拜拜" (ACK)
```

**关键点**：第二次挥手（ACK）和第三次挥手（FIN）之间可能有**时间差**，因为 Server 可能还有数据要发给 Client。

### CLIENT 主动断开的完整过程

```
    CLIENT                          Server
      |                               |
      |  [状态: ESTABLISHED]          |  [状态: ESTABLISHED]
      |                               |
      |---------- FIN ----------------|  fin=1, seq=u
      |                               |---> [状态: ESTABLISHED -> FIN_WAIT1]
      |<--------- ACK ----------------|  ack=u+1 (确认 Client 的 FIN)
      |---> [FIN_WAIT1 -> FIN_WAIT2]--|   [状态: ESTABLISHED -> CLOSE_WAIT]
      |                               |
      |     (此时半关闭状态)          |
      |     Server 还能发数据给 Client |
      |     Client 不能再发数据        |
      |                               |
      |<--------- FIN ----------------|  fin=1, seq=w
      |                               |   [状态: CLOSE_WAIT -> LAST_ACK]
      |---------- ACK ----------------|  ack=w+1 (确认 Server 的 FIN)
      |---> [FIN_WAIT2 -> TIME_WAIT]-|   [状态: LAST_ACK -> CLOSED]
      |                               |
      |   等待 2MSL 时间...           |
      |   [TIME_WAIT -> CLOSED]       |
      |                               |
      |         ✅ 连接完全关闭        |
```

### 详细状态变化解析

#### 第一步：CLIENT 发送 FIN

```text
报文: FIN=1, seq=u (u 是 Client 当前序列号)

Client 状态: ESTABLISHED --> FIN_WAIT1
含义: "我没有数据要发了,请求关闭发送方向"
```

**此时 Client 不再发送数据**，但仍能**接收** Server 发来的数据。

#### 第二步：Server 回复 ACK

```text
报文: ACK=1, ack=u+1 (确认 Client 的 FIN)

Server 状态: ESTABLISHED --> CLOSE_WAIT
Client 状态: FIN_WAIT1 --> FIN_WAIT2
```

> 💡 **CLOSE_WAIT 的含义**：Server 收到了 Client 的关闭请求，知道自己不再接收数据了，但**可能还有数据要发给 Client**。

#### 第三步：Server 发送 FIN

```text
报文: FIN=1, seq=w (w 是 Server 当前序列号)

Server 状态: CLOSE_WAIT --> LAST_ACK
含义: "我也没有数据要发了"
```

**注意**：这一步可能在第二步之后很久才发生（取决于 Server 应用层何时调用 `close()`）。

#### 第四步：Client 回复 ACK

```text
报文: ACK=1, ack=w+1

Server 状态: LAST_ACK --> CLOSED  ✅ (立即关闭)
Client 状态: FIN_WAIT2 --> TIME_WAIT (等待 2MSL)
```

### TIME_WAIT 状态详解

**TIME_WAIT 是什么？**

主动关闭的一方（这里是 Client）在发送最后一个 ACK 后，不会立即关闭，而是进入 **TIME_WAIT** 状态，持续 **2MSL** 时间。

> **名词解释 — 2MSL：**
> MSL = Maximum Segment Lifetime（最大报文段生存时间），即一个 TCP 报文段在网络中可以存活的最长时间（通常为 30~60 秒）。2MSL 就是两倍的 MSL，即报文段的最大往返时间，确保迟到的数据包彻底消失在网络中。

**为什么要等 2MSL？**

原因有两个：

**原因 1：确保最后一个 ACK 到达 Server**

```
场景: Client 发送的最后一个 ACK 丢失

Client                          Server
  |--- ACK (丢失!) ---------->|   Server 在 LAST_ACK
  |                           |   等待超时...
  |<--- FIN 重传 -------------|   Server 重传 FIN
  |--- ACK (重发) ------------>|
  |                           |   Server 收到 ACK → CLOSED
  |   (还在 TIME_WAIT)         |
  |   (能正确响应重传的 FIN)    |
```

如果没有 TIME_WAIT，Client 已经关闭了，就无法响应 Server 重传的 FIN，Server 将永远停留在 LAST_ACK 状态。

**原因 2：让旧连接的报文在网络中消失**

```
时间轴:

t=0:   连接关闭, Client 进入 TIME_WAIT
t=0.5: 网络中还残留旧连接的延迟报文 (如重传的数据包)
t=1:   (1 MSL 后) 所有旧的报文都已过期或被路由器丢弃
t=2:   (2 MSL 后) Client 可以安全关闭,不会有旧报文干扰新连接
```

> ⚠️ **生产环境注意**：短时间内大量连接关闭会导致大量 TIME_WAIT，占用系统资源。
> 
> 可以通过以下参数优化：
> ```bash
> sysctl net.ipv4.tcp_tw_reuse=1    # 允许复用 TIME_WAIT (仅用于客户端)
> sysctl net.ipv4.tcp_max_tw_buckets=262144  # 增加 TIME_WAIT 上限
> ```

**查看当前 TIME_WAIT 数量**：

```bash
ss -tan | grep TIME-WAIT | wc -l

# 或者查看各状态的分布统计
ss -tan | awk '{print $1}' | sort | uniq -c | sort -rn
```

---

## 四次挥手的另一种场景：SERVER 主动断开

在实际应用中，更多时候是 **Server 主动断开连接**（例如 HTTP 服务器 keepalive 超时、负载均衡器踢掉不活跃连接等）。

### SERVER 主动断开的完整过程

```
    CLIENT                          Server
      |                               |
      |  [状态: ESTABLISHED]          |  [状态: ESTABLISHED]
      |                               |
      |<--------- FIN ----------------|  fin=1, seq=p
      |   [ESTABLISHED->FIN_WAIT1] <--|   [ESTABLISHED->FIN_WAIT1]
      |---------- ACK ----------------|  ack=p+1 (确认 Server 的 FIN)
      |   [FIN_WAIT1->CLOSE_WAIT] --->|   [FIN_WAIT1->FIN_WAIT2]
      |                               |
      |     (半关闭状态)              |
      |     Client 还能发数据给 Server |
      |     Server 不能再发数据        |
      |                               |
      |---------- FIN ----------------|  fin=1, seq=r
      |   [CLOSE_WAIT->LAST_ACK] ---->|   [FIN_WAIT2->TIME_WAIT]
      |<--------- ACK ----------------|  ack=r+1 (确认 Client 的 FIN)
      |   [LAST_ACK->CLOSED] <-------|   (等待 2MSL)
      |                               |
      |   ✅ Client 立即关闭           |
      |   Server 进入 TIME_WAIT       |
      |   等待 2MSL 后关闭            |
      |                               |
      |         ✅ 连接完全关闭        |
```

**等等！这里有个容易混淆的点！**

仔细对比两种场景：

| 场景 | 谁进入 TIME_WAIT？ | 谁立即关闭？ |
|------|-------------------|-------------|
| **Client 主动断开** | **Client** (主动方) | Server (被动方) |
| **Server 主动断开** | **Server** (主动方) | Client (被动方) |

**结论：谁主动发起关闭（第一个发送 FIN），谁就要经历 TIME_WAIT！**

这是非常重要的原则！

### 与 Client 主动断开的对比

```text
【场景 A】Client 主动关闭:
Client: ESTAB -> FIN_WAIT1 -> FIN_WAIT2 -> TIME_WAIT(2MSL) -> CLOSED
Server: ESTAB -> CLOSE_WAIT -> LAST_ACK -> CLOSED(立即)

【场景 B】Server 主动关闭:
Client: ESTAB -> CLOSE_WAIT -> LAST_ACK -> CLOSED(立即)
Server: ESTAB -> FIN_WAIT1 -> FIN_WAIT2 -> TIME_WAIT(2MSL) -> CLOSED
```

### 生产环境案例：Nginx 的 keepalive_timeout

HTTP/1.1 支持长连接（keep-alive），但服务器不可能无限等待。
以 Nginx 为例：

```nginx
# nginx.conf 配置
http {
    keepalive_timeout 65;  # 65 秒无数据就踢掉客户端
}
```

**工作原理**：

```
1. Client 访问 Nginx, 建立 TCP 连接
2. 完成 HTTP 请求/响应
3. 连接空闲 65 秒
4. Nginx 主动发送 FIN (Server 主动断开)
5. Nginx 进入 TIME_WAIT (作为主动方)
6. Client 收到 FIN, 进入 CLOSE_WAIT
```

> 💡 **排查建议**：如果你的服务器上有大量 TIME_WAIT，很可能是 **服务端主动断开了大量短连接**。
>
> 可以考虑：
> - 调大 `keepalive_timeout`（减少断开频率）
> - 启用 `tcp_tw_reuse`（复用 TIME_WAIT）
> - 使用连接池（减少频繁建连/断连）

### CLOSE_WAIT 泄漏：常见陷阱

当 Server 主动关闭时，Client 会进入 **CLOSE_WAIT** 状态。

**CLOSE_WAIT 的含义**："我知道对方想关闭，但我还没调 `close()`"。

如果应用代码**忘记调用 close()**，或者**阻塞在某个操作上无法退出**，连接就会一直卡在 CLOSE_WAIT，最终耗尽文件描述符！

**典型错误代码**：

```python
# 错误示例: 忘记 close()
def handle_client(sock):
    data = sock.recv(1024)
    process(data)
    # ❌ 忘记调用 sock.close()!
    # 连接会一直处于 CLOSE_WAIT

# 正确写法:
def handle_client(sock):
    try:
        data = sock.recv(1024)
        process(data)
    finally:
        sock.close()  # ✅ 确保关闭
```

**排查命令**：

```bash
# 查看 CLOSE_WAIT 状态的连接
ss -tan | grep CLOSE-WAIT

# 找出哪个进程持有这些连接
ss -tanp | grep CLOSE-WAIT
# 输出示例:
# State   Recv-Q Send-Q Local Address:Port Peer Address:Port Process
# CLOSE-WAIT 0      0 192.168.1.1:8080   10.0.0.1:54321   users:(("python",pid=12345,fd=12))
```

---

## TCP 状态机总览

### 全流程状态转换图

```
                        +-----------+
                        |  CLOSED   |
                        +-----+-----+
                              |
                 +-------------+-------------+
                 |                           |
         被动打开(服务端)              主动打开(客户端)
                 |                           |
                 v                           v
         +-------+-------+             +-------+-------+
         |    LISTEN     |             |   SYN_SENT     |
         +-------+-------+             +-------+-------+
                 |                           |
         收到 SYN |              收到 SYN+ACK |
                 v                           |
         +-------+-------+             +-------+-------+
         |  SYN_RCVD     |<------------+   ESTABLISHED  |
         +-------+-------+  收到 ACK    +---------------+
                 |                           |
         收到 ACK |              主动关闭(发FIN)
                 v                           |
         +-------+-------+             +-------+-------+
         |  ESTABLISHED  +------------>+  FIN_WAIT1    |
         +---------------+             +-------+-------+
                                           |
                                   收到 ACK |
                                           v
                                     +-------+-------+
                                     |  FIN_WAIT2    |<------------------+
                                     +-------+-------+                   |
                                           |                             |
                                       收到 FIN                         |
                                           v                             |
                                     +-------+-------+                   |
                                     |  TIME_WAIT    |                   |
                                     +-------+-------+                   |
                                           |  等 2MSL                    |
                                           v                             |
                                     +-------+-------+                   |
                                     |    CLOSED     |<------------------+
                                     +---------------+

被动关闭路径 (收到 FIN):
ESTABLISHED --> CLOSE_WAIT (收到 FIN, 回复 ACK)
    |
    v
LAST_ACK (发送 FIN)
    |
    v
CLOSED (收到最后的 ACK)

> 注：还有一种极罕见的 **CLOSING** 状态——双方同时发送 FIN（同时关闭场景），
> 标准路径为 FIN_WAIT1 --[收到对方FIN]--> CLOSING --[收到ACK]--> TIME_WAIT。
> 极少数情况下也可能从 FIN_WAIT1 直接跳转至 TIME_WAIT（在 FIN_WAIT1 时同时收到对方的 FIN+ACK 合并报文）。
> 实际生产环境中几乎不会遇到。
```

### TCP 状态速查表

| 状态名 | 含义 | 出现场景 |
|--------|------|----------|
| **CLOSED** | 连接不存在 | 初始状态 / 最终状态 |
| **LISTEN** | 监听 incoming 连接 | 服务端调用 `listen()` 后 |
| **SYN_SENT** | 已发送 SYN，等待回应 | 客户端发起连接后 |
| **SYN_RCVD** | 收到 SYN，已发送 SYN+ACK | 服务端收到连接请求后 |
| **ESTABLISHED** | 连接已建立，可传输数据 | 三次握手完成后 |
| **FIN_WAIT1** | 已发送 FIN，等待 ACK | 主动关闭的第一步 |
| **FIN_WAIT2** | 对方确认了 FIN，等待对方也关闭 | 主动关闭的第二步 |
| **CLOSE_WAIT** | 收到了对方的 FIN，准备关闭 | 被动关闭后的等待期 |
| **LAST_ACK** | 已发送 FIN，等待最后的 ACK | 被动关闭的最后一步 |
| **TIME_WAIT** | 等待 2MSL 以确保网络清空 | 主动关闭的最后一步 |
| **CLOSING** | 双方同时关闭的罕见状态 | 双方同时发 FIN（极少见） |

> 📊 **统计你的服务器上的 TCP 状态分布**：
> ```bash
> ss -tan | awk 'NR>1 {print $1}' | sort | uniq -c | sort -rn
> ```

---

## 常见问题排查指南

### 大量 TIME_WAIT

**症状**：服务器上有成千上万 TIME_WAIT 连接，端口资源紧张。

**原因**：高频短连接场景（如 HTTP 短轮询、每次请求新建连接）。

**解决方案**：

```bash
# 方案 1: 启用 TIME_WAIT 复用 (推荐)
sysctl -w net.ipv4.tcp_tw_reuse=1

# 方案 2: 增加本地端口范围
sysctl -w net.ipv4.ip_local_port_range="1024 65535"

# 方案 3: 减少 TIME_WAIT 时间 (谨慎使用!)
# sysctl -w net.ipv4.tcp_tw_recycle=1  # ⚠️ 可能导致 NAT 问题,慎用

# 方案 4: 根治方法 - 使用连接池/长连接
# (应用层面优化,如 HTTP Keep-Alive,数据库连接池)
```

### CLOSE_WAIT 泄漏

**症状**：大量 CLOSE_WAIT 连接不释放，最终导致"Too many open files"。

**原因**：应用层代码 bug，收到 FIN 后没有调用 `close()`。

**排查步骤**：

```bash
# 1. 查看 CLOSE_WAIT 连接数量
ss -tan state close-wait | wc -l

# 2. 找出对应进程
ss -tanp state close-wait

# 3. 结合应用日志定位问题代码
# 检查是否有异常处理遗漏 close(), 或死锁导致无法退出
```

### 连接建立失败 (SYN Flood)

**症状**：新连接超时或被拒绝，服务器有大量 SYN_RCVD。

**原因**：遭受 SYN Flood 攻击：攻击者大量发送 SYN 包但不完成三次握手，耗尽服务器资源。

**防护措施**：

```bash
# 启用 SYN Cookies (内核自动防御)
sysctl -w net.ipv4.tcp_syncookies=1

# 增加 backlog 队列长度
sysctl -w net.core.somaxconn=1024
sysctl -w net.ipv4.tcp_max_syn_backlog=1024

# 减少 SYN+ACK 重试次数
sysctl -w net.ipv4.tcp_synack_retries=3
```

### 高延迟 / 丢包导致的性能问题

**现象**：传输速度慢，频繁超时重传。

**诊断工具**：

```bash
# 查看连接的重传情况
ip -s link show eth0
# 关注 "retransmits" 字段

# 用 ping 测试基础连通性和 RTT
ping -c 10 target_host

# 用 tcptrace 分析抓包文件
sudo tcpdump -i eth0 host target -w capture.pcap
tcptrace capture.pcap
```

**可能的优化方向**：

- 检查网络质量（丢包率、延迟）
- 调整 MTU 避免分段
- 启用 SACK（默认已开启）
- 考虑使用 UDP 替代方案（如 QUIC 协议）

---

## 总结

今天我们从头到尾梳理了 TCP 的"一生"：

**🤝 三次握手**：确保双向通信能力，交换初始参数（MSS、窗口大小等）

**📦 数据传输可靠性**：
- 序列号 + ACK 保证有序不丢
- 滑动窗口实现流量控制
- 拥塞控制四部曲（慢启动 → 拥塞避免 → 快重传 → 快恢复）
- 超时重传 + SACK 作为最后防线

**👋 四次挥手**：全双工特性导致需要两对 FIN/ACK
- **主动关闭方**会进入 TIME_WAIT（等 2MSL）
- **被动关闭方**经过 CLOSE_WAIT → LAST_ACK → CLOSED
- 谁先发 FIN，谁就承担 TIME_WAIT

**⚠️ 生产环境要点**：
- 高并发短连接 → 注意 TIME_WAIT 复用
- CLOSE_WAIT 泄漏 → 检查应用层是否正确 close()
- SYN Flood → 启用 SYN Cookies

---

## 参考资源

- RFC 793 - Transmission Control Protocol（TCP 协议标准）
- RFC 2581 / RFC 5681 - TCP Congestion Control（拥塞控制四部曲）
- RFC 2018 - TCP Selective Acknowledgment Options（SACK 选择性确认）
- RFC 6298 - TCP Retransmission Timeout（RTO 计算与超时重传）
- RFC 1323 - TCP Extensions for High Performance（窗口缩放 / 时间戳 / PAWS）
- RFC 6691 - TCP Options and MSS（MSS 计算）
- RFC 896 / RFC 1122 - Congestion Control in IP/TCP Interconnections（Nagle 算法 / TCP_NODELAY）
- RFC 5925 / RFC 6528 - TCP Authentication Option & Improved ISN Generation（ISN 安全）
- RFC 6928 - Increasing TCP's Initial Window（初始拥塞窗口）
- Wireshark 官方文档 - https://wiki.wireshark.org/TCP
- Linux man pages - `man 7 tcp`

**本文首发于公众号「Artificer老王的学习笔记」，转载请注明出处。**
