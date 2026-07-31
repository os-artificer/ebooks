# 英语介词实用指南：用法、搭配与开发场景例句

> 📚 中英双语 · 开发者专属 ｜ 阅读时长约 10 分钟

你有没有过这种时刻：写技术文档时想说"部署到生产环境"，结果卡在到底用 *to* 还是 *in*？评审别人代码时，看到 "discuss about the bug" 觉得别扭，却又说不清哪儿不对？

介词（preposition）就是英语里那些短得不起眼、却最容易露怯的小词：*in, on, at, by, for, with, from, to, of*。

本文用**中英对照**的方式，把开发者最高频的介词用法讲清楚，文末附 **10 个软件开发真实场景例句**，并加入**记忆锚点**与**开发者专属补充介词**。

---

## 🧩 一、介词到底有什么用 ／ What a preposition does

介词放在它的**宾语**（名词、代词或动名词）前面，构成一个"介词短语"，用来表示**时间、地点、方向、方式、原因、目的或所属**这类关系——就是英语里搭关系的"小连接词"。**通常**带着一个宾语。

> A **preposition** sits before its **object** (a noun, pronoun, or gerund) to form a "prepositional phrase." It expresses relationships like **time, place, direction, manner, reason, purpose, or possession**.  
> 💡 **例外：介词悬垂（preposition stranding）** —— *"This is the file I was looking **for**."* 介词 for 出现在句末、后面没有宾语，这在口语和 technical writing 里很常见。初学阶段不用纠结这个，先记住「通常有宾语」就够了。

几个例子：

- The logs are stored **in** the `/var/log` directory.（日志存在 `/var/log` 目录里）
- We are waiting **for** the build to finish.（我们在等构建完成）
- He is good **at** debugging.（他擅长调试）

⚠️ **一个硬规则**：介词后面的宾语必须用**宾格**（him / me / us），绝不能用主格（he / I / we）。

✅ "This tool was built by **him** and **me**."  
❌ "This tool was built by he and I."

> 💡 **一句话记住**：介词 by 后面要用宾格"他/我"（him / me），不能写成主格 he / I。

> 🧠 **记忆锚点①｜介词后接动词，一律用 -ing（动名词）**  
> good **at debugging** / **before merging** / **by setting** the flag  
> ❌ "before merge" / "by use the flag" —— 错在把动词用成了原形，介词后必须用 -ing 形式。  
> 💡 注：`without error` / `without question` 这类写法是正确的——error/question 在这里是名词，不是动词。

---

## 🕐 二、时间三件套：at / on / in

🔹 **at** — 确切钟点、时刻  
例：Deploy **at** 02:00 UTC.（在 02:00 UTC 部署）

🔹 **on** — 星期、日期、特定某天  
例：The release ships **on** Friday.（版本在周五发布）

🔹 **in** — 较长时段（月、年、季度）  
例：We launched it **in** Q3.（我们在第三季度上线）

✅ Merge the PR **at** noon, **on** Monday, **in** 2025.  
❌ "Merge it on noon" / "in Friday".

> 🧠 **记忆锚点②｜时间三件套一张图**  
> at = 一个**点**（钟点）· on = 一张**日历页**（某天）· in = 一段**区间**（月/年/季）  
> 口诀：**点 at、天 on、段 in**。

> 💡 **高频例外**：
>
> - **at night**（不用 in）／ **at the weekend**（英式；美式用 **on the weekend**）
> - **in the morning / afternoon / evening**（不用 on）
> - 这些例外没有规律，只能当固定搭配记。

---

## 📍 三、地点三件套：at / on / in

🔹 **at** — 一个点 / 具体位置  
例：The exception was thrown **at** line 42.（异常在第 42 行抛出）

🔹 **on** — 表面、设备或主机、端口（技术习惯用法）  
例：The app runs **on** a container and listens **on** port 8080.（应用跑在容器上，监听 8080 端口）

🔹 **in** — 封闭空间 / 容器  
例：Secrets are stored **in** a vault.（密钥存在保险库里）

> 💡 技术提示：网络语境里说 *listen **on** a port*、*run **on** a host*，这是固定习惯用法，会覆盖一般"at 表示点"的规则。

> 🧠 **记忆锚点③｜地点三件套一张图**  
> at = 图钉📍（**点**）· on = 桌面/线（**表面·主机·端口**）· in = 盒子📦（**容器**）  
> 和时间共用同一张图更好记：**点 at、面 on、里 in**。

---

## 🧭 四、方向与运动 ／ Direction & movement

🔸 **to** — 目的地：Deploy **to** production.（部署到生产环境）  
🔸 **from** — 起点：Roll back **from** the bad release.（从那个坏版本回滚）  
🔸 **into** — 进入内部：Insert the row **into** the table.（把行插入表里）  
🔸 **onto** — 到表面：Copy the binary **onto** the server.（把二进制拷到服务器上）  
🔸 **toward** — 朝…方向：moving **toward** a microservices design（朝微服务架构演进）  
🔸 **through** — 穿过／经由：Requests flow **through** the gateway.（请求经网关流转）

> 🧠 **记忆锚点④｜in vs into（开发者最爱混）**  
> **in** = 静态"在…里"：The data is **in** the cache.（数据在缓存里）  
> **into** = 动态"进到…里"：Load the data **into** the cache.（把数据载入缓存）  
> 一句话：**in 是位置，into 是动作**。

---

## 🛠 五、方式、手段与施动者 ／ by / with / through / via

🔹 **by**（被动的"由谁"）：The bug was found **by** the QA team.（bug 是 QA 团队发现的）  
🔹 **by** + 动名词（手段）：disable it **by** setting the flag（通过设置标志位来禁用）  
🔹 **with**（工具 / 语言）：Open it **with** a text editor.（用文本编辑器打开）/ Hash it **with** SHA-256.（用 SHA-256 哈希）  
🔸 **through / via**（中间渠道）：Authenticated **through** OAuth.（通过 OAuth 认证）/ Connected **via** SSH.（经 SSH 连接）

⚠️ **最容易混的一组**：

✅ "Written **by** the author" — 强调"人"写的  
✅ "Written **with** a pen / **in** Python" — 强调"工具 / 语言"  
❌ "Written by Python" — Python 是工具不是作者（除非拟人）

> 🧠 **记忆锚点⑤｜by / with 二分法**  
> **by = 谁做的（人/施动者）**；**with = 拿什么做的（工具/语言）**。  
> 对比：fixed **by** Alice（Alice 修的）vs fixed **with** a patch（用补丁修的）。

---

## ⚡ 六、原因 ／ Cause & reason

🔸 **because of** + 名词：The outage happened **because of** a config error.（故障因配置错误发生）  
🔸 **due to** + 名词（常跟在 be 后）：The delay was **due to** network latency.（延迟源于网络延迟）  
🔸 **for**（理由 / 目的）：We paused the deploy **for** safety.（为了安全暂停部署）  
🔸 **from**（问题来源）：The service suffered **from** memory leaks.（服务饱受内存泄漏之苦）

---

## 🎯 七、目的 ／ Purpose

🔹 **for** + 名词：A script **for** backups.（用于备份的脚本）  
🔹 **to** + 动词原形（表目的 = in order to）：We use CI **to** catch regressions.（用 CI 来捕获回归）

> 🧠 **记忆锚点⑥｜for / to 目的二分**  
> **for + 名词**，**to + 动词**。卡住时问自己：后面是词还是动作？

---

## 📦 八、所属与构成 ／ Possession & composition

🔸 **of**：The owner **of** the repository（仓库的拥有者）/ A list **of** users（用户清单）  
🔸 常用搭配：**consist of**（由…组成）、**made of**（由…制成，材料不变）、**made from**（由…制成，材料已变质）、**part of**（…的一部分）、**instead of**（而不是）、**a lot of**（许多）

> 💡 **made of vs made from**：
>
> - A table **made of wood** — 还是木头，材料没变 → 用 **of**
> - Wine **made from grapes** — 葡萄变成酒了，材料变了 → 用 **from**

---

## 🔗 九、开发者常用"动词 + 介词"搭配

下面每组先给动词，再给固定介词和中文义，例句里介词已加粗。

🔗 **depend / rely / count + on** ｜ 依赖  
　The API depends **on** the cache. ｜ 该 API 依赖于缓存。

🔗 **consist + of** ｜ 由…组成  
　The package consists **of** three modules. ｜ 这个包由三个模块组成。

🔗 **responsible / accountable + for** ｜ 对…负责  
　You are responsible **for** the pipeline. ｜ 你对该流水线负责。

🔗 **familiar / comfortable + with** ｜ 熟悉  
　Be familiar **with** the guidelines. ｜ 要熟悉这些规范。

🔗 **aware / conscious + of** ｜ 意识到  
　Aware **of** the limitation. ｜ 意识到这个限制。

🔗 **interested + in** ｜ 对…感兴趣  
　Interested **in** Rust? ｜ 对 Rust 感兴趣吗？

🔗 **good / bad + at** ｜ 擅长 / 不擅长  
　Good **at** debugging. ｜ 擅长调试。

🔗 **complain / worry / care + about** ｜ 抱怨 / 担心 / 在意  
　Care **about** performance. ｜ 在意性能。

🔗 **deal / work / agree + with** ｜ 处理 / 共事 / 赞同（人/观点）  
　Deal **with** edge cases. ｜ 处理边界情况。

🔗 **agree / decide / focus / comment + on** ｜ 就…达成一致 / 聚焦（计划/事物）  
　Focus **on** the hot path. ｜ 聚焦于热点路径。

🔗 **refer / belong / listen / talk / relate + to** ｜ 参考 / 属于 / 听 / 谈 / 关联  
　Refer **to** the docs. ｜ 参考文档。

🔗 **recover / differ / suffer + from** ｜ 从…恢复 / 区别于 / 受…之苦  
　Recover **from** a crash. ｜ 从一次崩溃中恢复。

🔗 **wait / look / ask / pay + for** ｜ 等待 / 寻找 / 请求 / 支付  
　Wait **for** the lock. ｜ 等待锁。

🔗 **based / insist + on** ｜ 基于 / 坚持  
　Based **on** the logs. ｜ 基于日志。

🔗 **connect / talk + to** ｜ 连接到 / 对…说  
　Connect **to** the database. ｜ 连接到数据库。

💡 **小提醒｜agree 为什么出现两次？**  
　不是笔误，是含义不同：  
　- agree **with** a person / an idea → 赞同某人 / 某观点  
　- agree **on** a plan / a decision → 就计划 / 决定达成一致  
　- 另：agree **to** a proposal / a request → 答应一个提议 / 请求

---

## ⚠️ 十、必须避开的 5 个典型错误

❌ "discuss about the bug"  
✅ "discuss the bug" ｜ *discuss* 是及物动词，后面不加 about

❌ "depend of"  
✅ "depend **on**" ｜ 依赖用 on，不是 of

❌ "reach to the server"  
✅ "reach the server" ｜ *reach* 是及物动词；想加介词就说 "get **to**"

❌ "explain me"  
✅ "explain **to** me" ｜ 向某人解释用 explain **to** sb.

❌ "listen the event"  
✅ "listen **to** the event" ｜ 听…用 listen **to**

---

## 🚀 十一、开发者高频补充介词（原文未覆盖）

下面这些介词在代码、文档、运维里出现极高频，原文没单独列，这里补齐。

🔸 **against** — 基于…编译／对照／防范  
　Compile **against** the API.（基于该 API 编译）/ Protect **against** SQL injection.（防范 SQL 注入）

🔸 **within** — 在…之内（时限、范围）  
　Complete **within** 5 seconds.（在 5 秒内完成）

🔸 **across** — 横跨／分布在…（多节点）  
　Replicated **across** three zones.（跨三个可用区复制）

🔸 **between** — 在…之间（两者）  
　Communication **between** services.（服务之间的通信）

🔸 **over** — 经由（网络 / 协议）  
　Sent **over** HTTP.（通过 HTTP 发送）

🔸 **under** — 在…之下（状态 / 管控）  
　The code is **under** version control.（代码处于版本控制之下）/ The server is **under** load.（服务器正处于负载中）

🔸 **without** — 没有（伴随否定）  
　Deploy **without** downtime.（零停机部署）

🔸 **out of** — 用尽／超出（资源 / 边界）  
　The process crashed **out of** memory.（进程因内存耗尽崩溃）/ Index **out of** bounds.（索引越界）

🔸 **per** — 每（比率）  
　1000 requests **per** second.（每秒 1000 个请求）

🔸 **behind** — 在…背后（代理 / 网关）  
　The service sits **behind** a proxy.（服务位于代理之后）

🔸 **about** — 关于／涉及（主题）  
　Read the docs **about** authentication.（读关于认证的文档）／ A book **about** algorithms.（一本算法书）

> 💡 注意：about 在第九节以动词搭配出现（care about / complain about），这里单独列出是因为"关于"这个含义在技术写作里极高频，值得独立记忆。

🔸 **during** — 在…期间（过程 / 事件）  
　**During** the build, all tests run in parallel.（构建期间所有测试并行运行）／ **During** deployment, the service enters maintenance mode.（部署期间服务进入维护模式）

🔸 **since / for**（时间：你之前可能混用）  
　**since** + 时间点（从何时起）：running **since** 9 AM（从早上 9 点一直运行）  
　**for** + 时段（持续多久）：running **for** 3 hours（已运行 3 小时）

---

## 💡 十二、10 个软件开发场景例句（中英对照）

1️⃣ The authentication service **depends on** a third-party API **for** token validation.  
　🇨🇳 认证服务依赖一个第三方 API 来做令牌校验。

2️⃣ We migrated the database **from** PostgreSQL **to** MySQL without any downtime.  
　🇨🇳 我们把数据库从 PostgreSQL 迁移到 MySQL，全程零停机。

3️⃣ The crash was caused **by** a race condition **in** the background worker thread.  
　🇨🇳 这次崩溃是由后台工作线程里的一个竞态条件引起的。

4️⃣ Please run the test suite **before** merging your branch **into** `main`.  
　🇨🇳 请把你的分支合并进 `main` 之前，先跑一遍测试套件。

5️⃣ The gateway is responsible **for** routing requests **to** the correct microservice.  
　🇨🇳 网关负责把请求路由到正确的微服务。

6️⃣ We replaced the slow parser **with** a newer one written **in** Rust.  
　🇨🇳 我们把那个慢解析器换成了用 Rust 写的新版。

7️⃣ The nightly build failed **because of** an expired signing certificate.  
　🇨🇳 夜间构建因一张过期签名证书而失败。

8️⃣ New contributors must be familiar **with** the style guide **before** opening a pull request.  
　🇨🇳 新贡献者在开 PR 之前，必须先熟悉风格指南。

9️⃣ The function returns a pointer **to** the allocated buffer, or `NULL` **on** failure.  
　🇨🇳 该函数返回一个指向已分配缓冲区的指针，失败时则返回 `NULL`。

🔟 Logs are written **in** the `/var/log` directory and shipped **to** our monitoring service **at** midnight.  
　🇨🇳 日志写在 `/var/log` 目录里，并在午夜发送到我们的监控服务。

> 💡 小结：每个句子都把介词和正确宾语配对。注意 **by**（施动者）vs **with**（工具）vs **from**（来源）的区别，以及第二、三节的 **at / on / in** 时间地点规则。

---

## 📌 十三、速记 cheat sheet（一图背下）

📍 **at / on / in（时间）** — 点 at、天 on、段 in；例外：at night / in the morning  
📍 **at / on / in（地点）** — 点 at、面/主机/端口 on、容器 in  
🛠 **by / with** — 人(by) / 工具语言(with)  
🎯 **for / to（目的）** — 名词前 for、动词前 to  
🔄 **in / into** — 静态在里(in) / 动态进里(into)  
⏱ **since / for（时间）** — 时点 since、时段 for  
🔤 **介词 + 动词** — 一律用 -ing  
📖 **about** — 关于主题 ｜ **during** — 在…期间

---

## ✅ 十四、快速自检清单

- [ ] 介词宾语用宾格了吗？（him / me / us）
- [ ] 用对动词的固定介词了吗？（depend **on**，不是 of）
- [ ] 时间：at（钟点）/ on（某天）/ in（时段）？
- [ ] 地点：at（点）/ on（表面·主机·端口）/ in（容器）？
- [ ] 被动施动者用 by；工具 / 语言用 with；渠道用 through / via？
- [ ] 介词后接动词用了 -ing 吗？（before merging，不是 before merge）
- [ ] in（静态）和 into（动态）分清了吗？
- [ ] 时间 since（点）/ for（段）分清了吗？
- [ ] 及物动词后没多此一举加介词吧？（discuss、reach、explain→to）

---

📌 **收藏这篇**，下次写文档卡在介词时直接翻出来对照。

> 本文首发于公众号 **Artificer老王的学习笔记**，转载请注明出处。
