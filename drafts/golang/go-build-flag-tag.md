# Go build flags 实战：7 个场景掌握构建期控制

## 为什么 build flag 值得认真学

日常开发中，你大概率写过这样的命令：

```bash
go build -o app .
```

这个够用，但只用了 `go build` 最基础的能力。

如果你有以下需求时又当如何实现呢？

- **在 macOS 上编译出 Linux 二进制**（CI/CD 场景）
- **把 git commit 和构建时间打进二进制**（版本追踪）
- **生产环境二进制从 18MB 瘦到 9MB**（容器镜像优化）
- **调试日志只在 debug 构建中出现，生产构建零开销**（避免运行时判断）
- **同一套接口，Linux 用 epoll、Windows 用 IOCP**（平台适配）

下面将结合几个高频应用场景进行实践学习。

---

## 场景一：跨平台编译

### 问题

试想这样一个工作场景：我们的服务要部署到 Linux amd64 服务器，但开发机是 macOS arm64。

我们总不能为了编译一个二进制就配置一台 Linux 机器吧，虽然这样做也不是不行，但是大多数时候也没有必要。

### 方案

Go在语言层面就已经给出了解决方案（这是C/C++这类编译型语言所不具备的能力），实现起来也非常简单只需设置 `GOOS` 和 `GOARCH` 两个环境变量就可以了，如下所示：

```bash
# macOS 上编译 Linux amd64 纯静态二进制
CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -o app-linux-amd64 .

# Windows 32 位可执行文件
GOOS=windows GOARCH=386 go build -o app.exe .

# ARM64（树莓派、Apple Silicon 服务器）
GOOS=linux GOARCH=arm64 go build -o app-linux-arm64 .

# WebAssembly
GOOS=js GOARCH=wasm go build -o app.wasm .
```

`CGO_ENABLED=0` 表示禁用 cgo，产出不依赖 libc 的纯静态二进制。

但如果代码用了 cgo（如 `database/sql` 配 SQLite），则需对应平台的 C 工具链，此时实现跨平台编译就会复杂很多（大多数情况下是不需要这么做的）。

### 通过以下命令可以查询当前使用的Go版本支持的目标平台

```bash
# 列出所有支持的 GOOS/GOARCH 组合
go tool dist list

# 过滤 Linux 相关
go tool dist list | grep linux
```

常用组合速查（这里只做示例展示，实际上远比这展示的多）：

| GOOS | GOARCH | 备注 |
|------|--------|------|
| linux | amd64 / arm64 / 386 | 服务端主力 |
| windows | amd64 / arm64 | 带 `.exe` 后缀 |
| darwin | amd64 / arm64 | macOS，arm64 = Apple Silicon |

---

## 场景二：注入版本信息

### 问题

有这么个场景，可能你也遇到过：线上跑了 5 个二进制，出 bug 时不仅想知道二进制的版本，更想知道是哪个 commit 编出来的。

如果仅靠手动改源码里的版本号，未必能维护准确——毕竟可能某个马大哈没改版本就发布上线了。就算版本号对了，从一堆历史里翻出对应代码也要花些力气。但如果知道二进制对应的 commit id 就不一样了，可以迅速定位到对应代码。

### 方案：`-ldflags -X`

`-X` 能在**链接期**把一个字符串变量设为指定值，源码里只留占位符，构建时动态注入即可。

实现方式如下所示，先在代码里声明变量作为占位符：

```go
// main.go
package main

import "fmt"

// 以下变量在源码中只声明占位，构建时由 -X 注入实际值
var (
    Version   = "dev"
    CommitSHA = "none"
    BuildTime = "unknown"
)

func main() {
    fmt.Printf("version=%s commit=%s built=%s\n", Version, CommitSHA, BuildTime)
}
```

构建时注入：

```bash
go build -ldflags "\
  -X main.Version=1.2.3 \
  -X main.CommitSHA=$(git rev-parse --short HEAD) \
  -X 'main.BuildTime=$(date -u +%Y-%m-%dT%H:%M:%SZ)'" \
  -o app .
```

运行结果（效果展示）：

```
version=1.2.3 commit=a1b2c3d built=2026-07-08T12:00:00Z
```

### 注意事项

- `-X` 只对**字符串变量**有效，且该变量必须未初始化或初始化为常量字符串表达式，不能调用函数或引用其他变量。
- `importpath` 是变量的**完整导入路径 + 包名**。`main` 包就是 `main.Version`；子包则是 `example.com/proj/pkg.Version`。
- 值含空格或特殊字符时用单引号包裹（如 `BuildTime` 里的时间戳）。

### 进阶：封装成 Makefile

```makefile
VERSION  := $(shell git describe --tags --always --dirty)
COMMIT   := $(shell git rev-parse --short HEAD)
BUILDTIME:= $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
LDFLAGS  := -X main.Version=$(VERSION) -X main.CommitSHA=$(COMMIT) -X main.BuildTime=$(BUILDTIME)

build:
	go build -ldflags "$(LDFLAGS)" -o bin/app .
```

这样每次 `make build` 就会自动提取git 信息并注入到二进制中，CI/CD 实践中尤其好用。

---

## 场景三：裁剪二进制体积

### 问题

一个简单的 HTTP 服务编译出来 18MB，容器镜像也因此变大，那么能不能砍掉用不到的部分呢？

### 方案：`-s -w`

```bash
# 默认构建
go build -o app-default .

# 瘦身构建
go build -ldflags "-s -w" -o app-stripped .
```

对比效果：

```bash
$ ls -lh app-default app-stripped
-rwxr-xr-x  18M app-default
-rwxr-xr-x 9.1M app-stripped
```

体积直接砍半。

- `-s`：省略符号表与调试信息。
- `-w`：省略 DWARF 符号表。

两者常组合使用。

当然做任何事都是有代价的：此时的**代价**则是无法用 gdb/delve 等工具进行调试，`go version -m` 能读取的信息也会减少。所以：调试构建别加，生产构建则可以加上。

### 组合注入版本 + 瘦身

实际项目中两者可以一起用：

```bash
go build -ldflags "-s -w \
  -X main.Version=1.2.3 \
  -X main.CommitSHA=$(git rev-parse --short HEAD)" \
  -o app .
```

---

## 场景四：调试代码开关

### 问题

有这么个需求：开发时想看详细日志，生产环境又不想有日志开销和噪音，怎么实现呢？

熟悉C/C++的朋友都知道，在C/C++中可以使用条件编译来控制哪些代码可以被编译到二进制里。

其实Go里也有提供类似的能力。

### 方案：build tag

用 build tag 提供同一函数的**两份实现**，编译期决定用哪份，生产构建里调试代码完全不存在。

**debug_log.go**（debug 构建时编译）：

```go
//go:build debug

package main

import "log"

func debugLog(msg string) {
    log.Println("[DEBUG]", msg)
}
```

**debug_log_off.go**（非 debug 构建时编译）：

```go
//go:build !debug

package main

// 空实现，生产构建中 debugLog 调用会被编译器内联消除
func debugLog(msg string) {}
```

**main.go**：

```go
package main

func main() {
    debugLog("service starting")
    // ...业务逻辑
}
```

构建时按需开启：

```bash
# 开发构建：包含调试日志
go build -tags debug -o app-debug .

# 生产构建：debugLog 为空实现，零开销
go build -o app .
```

### build tag 语法

约束注释以 `//go:build` 开头，使用 `||`（或）、`&&`（与）、`!`（非）和括号组合：

```go
//go:build (linux && 386) || (darwin && !cgo)
```

一个文件**只能有一条** `//go:build` 行。
约束必须位于文件顶部、`package` 子句之前，且后跟一个空行。

### 常用约定 tag

```go
//go:build ignore        // 文件永远不参与构建（常用于代码生成脚本）
//go:build cgo           // 仅启用 cgo 时编译
//go:build purego        // 纯 Go 实现版本
//go:build generate      // go generate 执行时设置
```

---

## 场景五：同一接口，不同实现

### 问题

业务场景：获取临时目录。临时目录在不同的平台上有不同的实现，比如：Linux 是 `/tmp`，而 Windows 是环境变量 `TEMP`。

此时如果写一堆 `if runtime.GOOS == "windows"` 可以吗？当然可以，但更优雅的做法是让编译器只编译对应平台的实现。

### 方案：文件名隐式约束

Go 会根据文件名后缀自动加隐式 build tag，无需手写 `//go:build`。

**file_linux.go**：

```go
package storage

func tempDir() string { return "/tmp" }
```

**file_windows.go**：

```go
package storage

import "os"

func tempDir() string { return os.Getenv("TEMP") }
```

构建时，`GOOS=linux` 只编译 `file_linux.go`，`GOOS=windows` 只编译 `file_windows.go`。

此实践方案无运行时判断，无冗余代码，代码会非常干净。

### 文件命名规则

前面两个例子只是"文件名隐式约束"的感性认识。

编译器究竟怎么判定？它会对文件名做固定两步剥离，去掉**不属于约束的无关后缀**，再对剩下的"基础名"做模式匹配，执行过程如下：

1. 去掉扩展名（`.go`、`.s`、`.c` 等）；
2. 再去掉 `_test` 后缀——测试文件同样服从平台约束，例如 `dns_windows_test.go` 在 `GOOS=linux` 时也不会被编译。

做完以上两步剥离后，如果基础名匹配以下模式，那么编译器就会自动加上对应隐式构建约束：

- `*_GOOS` → 如 `dns_windows.go`（仅 Windows）
- `*_GOARCH` → 如 `math_386.s`（仅 32 位 x86）
- `*_GOOS_GOARCH` → 如 `source_windows_amd64.go`

### 显式约束更灵活

有些时候一些复杂的规则用隐式规则无法实现，那么此时就可以用显式约束 `//go:build`，如下示例：

```go
//go:build (linux && 386) || (darwin && !cgo)
```

表示"Linux 32 位"或"macOS 且未启用 cgo"时才编译。

### 特殊平台映射

有些 GOOS 会同时匹配另一个 tag：

- `GOOS=android` → 同时匹配 `linux` + `android`
- `GOOS=ios` → 同时匹配 `darwin` + `ios`
- `GOOS=illumos` → 同时匹配 `solaris` + `illumos`

---

## 场景六：纯 Go 还是 cgo

### 问题

某个库你写了两套实现：cgo 版本调 C 库性能更好，纯 Go 版本便于跨编译。
怎么让用户构建时自由选？

### 方案：`cgo` / `purego` tag

**fast_cgo.go**（cgo 版本）：

```go
//go:build cgo && (linux || darwin)

package crypto

/*
#cgo CFLAGS: -O2
#include <openssl/sha.h>
*/
import "C"

func SHA256(data []byte) []byte {
    // 调用 OpenSSL 实现，性能更优
    // ...
}
```

**purego.go**（纯 Go 版本）：

```go
//go:build !(cgo && (linux || darwin))

package crypto

import "crypto/sha256"

func SHA256(data []byte) []byte {
    // 纯 Go 实现，便于跨编译
    h := sha256.Sum256(data)
    return h[:]
}
```

用户按需选择：

```bash
# 用 cgo 版本（需 CGO_ENABLED=1 且目标平台有 C 库）
CGO_ENABLED=1 go build -o app .

# 用纯 Go 版本（跨编译友好）
CGO_ENABLED=0 go build -o app .
```

`purego` 是社区约定 tag（需通过 `-tags purego` 显式开启），表示"纯 Go 实现版本"，不限制是否使用 cgo 或 unsafe，仅用于标记实现选择。

---

## 场景七：可复现构建（移除路径与时间痕迹）

### 问题

同一份代码（同一个 commit）在 A 机器和 B 机器上各编译一次，得到的二进制文件**字节并不一致**，两者的哈希值对不上。

原因：Go 默认会把构建机器的绝对路径（如 `/home/runner/work/...`）写进二进制里，机器不同，路径就不同，哈希自然不同。

这会带来两个问题：

- **安全审计**：审计者从同一份源码重新编译，得到的哈希和官方发布的对不上，就无法证明"这个二进制确实由该源码生成、未被篡改"。
- **容器镜像缓存**：镜像构建缓存按二进制哈希判断是否复用；同源却哈希不同，缓存永远不命中，每次都白白重新构建拖慢 CI。

### 方案：`-trimpath`

```bash
go build -trimpath -o app .
```

`-trimpath` 把二进制中的文件系统绝对路径替换为 `module@version` 或导入路径。配合固定的时间戳和 `-ldflags`可实现**可复现构建**（reproducible build）——同源同参数产出 bit 级一致的二进制。

### 完整可复现构建示例

```bash
# 固定构建参数
CGO_ENABLED=0 \
go build \
  -trimpath \
  -ldflags "-s -w -buildid=" \
  -o app .
```

- `-trimpath`：移除路径。
- `-buildid=`：清空 build id（在 ldflags 中设为空）。
- `-s -w`：省略符号表（也消除了部分不一致来源）。

验证：

```bash
# 两次构建后比较
sha256sum app && rm app && go build -trimpath -ldflags "-s -w -buildid=" -o app . && sha256sum app
# 两次 hash 应一致
```

### 配合 CI 使用

GitHub Actions、GitLab CI 中固定 runner 镜像 + 上述 flag，即可实现稳定的可复现构建，便于安全团队校验二进制与源码的对应关系。

---

## 小结：场景速查表

| 场景 | 关键 flag / 变量 | 一句话 |
|------|------------------|--------|
| 跨平台编译 | `GOOS` / `GOARCH` / `CGO_ENABLED` | 设两个变量，产出任意平台二进制 |
| 注入版本信息 | `-ldflags -X importpath.name=value` | 链接期把字符串变量注入二进制 |
| 裁剪体积 | `-ldflags "-s -w"` | 省略符号表，体积砍半 |
| 调试开关 | `-tags debug` + `//go:build debug` | 同一函数两份实现，编译期选 |
| 平台特定代码 | 文件名后缀 / `//go:build linux` | 编译器只编译对应平台文件 |
| 纯 Go / cgo 切换 | `CGO_ENABLED` + `//go:build cgo` | 一套接口两套实现，构建时选 |
| 可复现构建 | `-trimpath` + `-ldflags "-buildid="` | 移除路径与 build id，同源同果 |

> 生产构建的"黄金组合"：`CGO_ENABLED=0 go build -trimpath -ldflags "-s -w -X main.Version=..." -o app .` 兼顾跨平台、瘦身、版本注入与可复现。

---

## 附录 A：go build 全部 build flag 速查

下列 flag 被 `build`、`clean`、`get`、`install`、`list`、`run`、`test` 共享。

| Flag | 参数 | 说明 |
|------|------|------|
| `-C dir` | 目录 | 先切换目录再执行；必须是命令行第一个 flag。 |
| `-a` | — | 强制重新编译已是最新的包。 |
| `-n` | — | 打印命令但不执行。 |
| `-p n` | 整数 | 并行度，默认 `GOMAXPROCS`。 |
| `-race` | — | 数据竞争检测（darwin/amd64、darwin/arm64、freebsd/amd64、linux/amd64、linux/arm64[48-bit VMA]、linux/ppc64le、linux/riscv64、windows/amd64）。 |
| `-msan` | — | 与 Memory Sanitizer 协同（linux/amd64、linux/arm64、linux/loong64、freebsd/amd64，需 Clang/LLVM）。 |
| `-asan` | — | 与 Address Sanitizer 协同（linux/arm64、linux/amd64、linux/loong64；需 GCC≥7 或 Clang/LLVM≥9；loong64 需 Clang/LLVM≥16）。 |
| `-cover` | — | 开启覆盖率插桩。 |
| `-covermode` | `set,count,atomic` | 覆盖率模式：set=是否执行；count=执行次数；atomic=线程安全计数。默认 `set`，`-race` 时 `atomic`。 |
| `-coverpkg` | `p1,p2,...` | 对匹配模式的包做覆盖率分析。隐含 `-cover`。 |
| `-v` | — | 编译时打印包名。 |
| `-work` | — | 打印临时工作目录且不删除。 |
| `-x` | — | 打印执行的命令。 |
| `-asmflags` | `[pattern=]arg list` | 透传给 `go tool asm`。 |
| `-buildmode` | `mode` | 构建模式（见附录 C）。 |
| `-buildvcs` | `true/false/auto` | 是否把 VCS 信息写入二进制，默认 `auto`。 |
| `-compiler` | `name` | 编译器：`gc` 或 `gccgo`。 |
| `-gccgoflags` | `[pattern=]arg list` | 透传给 gccgo 编译器/链接器。 |
| `-gcflags` | `[pattern=]arg list` | 透传给 `go tool compile`。 |
| `-installsuffix` | `suffix` | 安装目录后缀；`-race` 自动加 `race`。 |
| `-json` | — | 以 JSON 输出构建事件。 |
| `-ldflags` | `[pattern=]arg list` | 透传给 `go tool link`（见附录 B）。 |
| `-linkshared` | — | 链接到 `-buildmode=shared` 生成的共享库。 |
| `-mod` | `readonly/vendor/mod` | 模块下载模式。 |
| `-modcacherw` | — | 模块缓存目录保持可写。 |
| `-modfile` | `file` | 使用备选 `go.mod`。 |
| `-overlay` | `file` | 构建覆盖 JSON 配置。 |
| `-pgo` | `file/auto/off` | PGO 性能优化配置，默认 `auto`。 |
| `-pkgdir` | `dir` | 从指定目录安装/加载包。 |
| `-tags` | `tag,list` | 额外视为满足的 build tag（逗号分隔）。 |
| `-trimpath` | — | 移除二进制中的文件系统绝对路径。 |
| `-toolexec` | `'cmd args'` | 包装工具链调用（如 `vet`、`asm`）。 |
| `-o file` | 文件/目录 | 指定输出文件或目录（`go build` 专属）。 |

> `-gcflags` / `-ldflags` 等支持 `[pattern=]` 前缀，把参数仅作用于匹配某包模式的包。如 `go build -gcflags=all=-S` 对所有包打印汇编。

## 附录 B：链接器 `-ldflags` 全部参数（go tool link）

| Flag | 说明 |
|------|------|
| `-B note` | ELF 下添加 `ELF_NT_GNU_BUILD_ID` 注记。 |
| `-E entry` | 设置入口符号名。 |
| `-H type` | 设置可执行文件格式；Windows 上 `windowsgui` 生成 GUI 程序。 |
| `-I interpreter` | 设置 ELF 动态链接器。 |
| `-L dir` | 在 `$GOROOT/pkg/$GOOS_$GOARCH` 之后搜索导入包。 |
| `-R quantum` | 设置地址对齐量子。 |
| `-T address` | 设置文本符号起始地址。 |
| `-V` | 打印链接器版本并退出。 |
| `-X importpath.name=value` | 将字符串变量 `name` 设为 `value`。 |
| `-asan` | 链接 C/C++ Address Sanitizer 支持。 |
| `-aslr` | 对 `c-shared` 启用 ASLR（Windows，默认 true）。 |
| `-bindnow` | 标记动态链接 ELF 对象立即绑定（默认 false）。 |
| `-buildid id` | 记录 Go 工具链 build id。 |
| `-buildmode mode` | 构建模式（默认 `exe`）。 |
| `-c` | 导出调用图。 |
| `-checklinkname=value` | `0` 允许所有 `go:linkname`；`1`（默认）仅允许已知常用 linkname。 |
| `-compressdwarf` | 尽可能压缩 DWARF（默认 true）。 |
| `-cpuprofile file` | 写 CPU profile。 |
| `-d` | 禁用动态可执行文件生成。 |
| `-dumpdep` | 导出符号依赖图。 |
| `-e` | 不限制报告的错误数量。 |
| `-extar ar` | 外部归档程序（默认 `ar`），仅 `c-archive` 用。 |
| `-extld linker` | 外部链接器（默认 `clang` 或 `gcc`）。 |
| `-extldflags flags` | 传给外部链接器的空格分隔参数，如 `-static`。 |
| `-f` | 忽略链接归档中的版本不匹配。 |
| `-funcalign N` | 函数对齐到 N 字节。 |
| `-g` | 禁用 Go 包数据检查。 |
| `-importcfg file` | 从文件读取导入配置。 |
| `-installsuffix suffix` | 在 `$GOROOT/pkg/$GOOS_$GOARCH_suffix` 中查找包。 |
| `-k symbol` | 设置字段追踪符号（需 `GOEXPERIMENT=fieldtrack`）。 |
| `-libgcc file` | 编译器支持库名（仅内部链接模式）。 |
| `-linkmode mode` | 链接模式：`internal`/`external`/`auto`。 |
| `-linkshared` | 链接已安装的 Go 共享库（实验性）。 |
| `-memprofile file` | 写内存 profile。 |
| `-memprofilerate rate` | 设置 `runtime.MemProfileRate`。 |
| `-msan` | 链接 C/C++ Memory Sanitizer 支持。 |
| `-o file` | 输出文件（默认 `a.out`）。 |
| `-pluginpath path` | 导出 plugin 符号的前缀路径。 |
| `-r dir1:dir2:...` | 设置 ELF 动态链接器搜索路径。 |
| `-race` | 链接竞态检测库。 |
| `-s` | 省略符号表与调试信息（隐含 `-w`）。 |
| `-tmpdir dir` | 外部链接模式下临时文件目录。 |
| `-v` | 打印链接器操作追踪。 |
| `-w` | 省略 DWARF 符号表。 |

## 附录 C：`-buildmode` 取值说明

| 模式 | 说明 |
|------|------|
| `archive` | 把非 main 包构建为 `.a` 文件（忽略 main 包）。 |
| `c-archive` | 把 main 包及其依赖构建为 C 归档；仅 `//export` 导出的符号可调用。 |
| `c-shared` | 构建为 C 共享库；仅 `//export` 导出符号可调用；wasip1 上构建为 WASI reactor。 |
| `default` | 默认：main 包构建为可执行文件，非 main 包构建为 `.a`。 |
| `shared` | 把非 main 包合并为单个共享库（配合 `-linkshared` 使用）。 |
| `exe` | 把 main 包及其依赖构建为可执行文件。 |
| `pie` | 构建为位置无关可执行文件（PIE）。 |
| `plugin` | 构建为 Go plugin。 |

## 附录 D：构建相关环境变量

| 变量 | 说明 |
|------|------|
| `GOOS` | 目标操作系统（如 `linux`、`windows`、`darwin`）。 |
| `GOARCH` | 目标架构（如 `amd64`、`arm64`）。 |
| `CGO_ENABLED` | 是否启用 cgo（`1`/`0`）。 |
| `GOFLAGS` | 空格分隔的 `-flag=value` 默认参数列表。 |
| `GOCACHE` | 构建缓存目录（绝对路径）。 |
| `GOMODCACHE` | 下载的模块缓存目录。 |
| `GOPATH` | Go 工作区路径。 |
| `GOROOT` | Go 安装根目录。 |
| `GOBIN` | `go install` 安装可执行文件的位置。 |
| `GO111MODULE` | 模块模式开关：`off`/`on`/`auto`。 |
| `GOPROXY` | 模块代理地址，逗号分隔，支持 `direct`、`off`。默认 `https://proxy.golang.org,direct`。 |
| `GOPRIVATE` | 私有模块前缀列表，自动设置 `GONOPROXY` 与 `GONOSUMDB`。 |
| `GOSUMDB` | 校验和数据库地址（`sum.golang.org`），`off` 可关闭。 |
| `CC` / `CXX` | cgo 使用的 C / C++ 编译器。 |

修改持久默认值用 `go env -w NAME=VALUE`，查看用 `go env <NAME>`。

典型私有仓库配置：

```bash
go env -w GOPRIVATE="git.example.com,github.com/your-org"
go env -w GOPROXY="https://proxy.golang.org,direct"
```

## 附录 E：内置 build tag 清单

**默认满足的 tag**（随构建环境变化）：

- 操作系统：`runtime.GOOS`（`linux`、`windows`、`darwin`…）
- 架构：`runtime.GOARCH`（`amd64`、`arm64`…）
- 架构特性：`GOARCH.feature`，如 `amd64.v1/v2/v3`（由 `GOAMD64` 等环境变量控制）
- `unix`：GOOS 为类 Unix 系统时
- 编译器：`gc` / `gccgo`
- `cgo`：cgo 可用时
- Go 主版本：`go1.1`、`go1.12`…直到当前版本（无 beta/minor tag）
- `-tags` 传入的任意自定义 tag

**架构特性 tag 派生规则**：

| GOARCH | 环境变量 | 产生的 feature tag |
|--------|----------|--------------------|
| amd64 | `GOAMD64=v1,v2,v3` | `amd64.v1` / `amd64.v2` / `amd64.v3` |
| arm | `GOARM=5,6,7` | `arm.5` / `arm.6` / `arm.7` |
| arm64 | `GOARM64=v8.{0-9}` / `v9.{0-5}` | `arm64.v8.{0-9}` / `arm64.v9.{0-5}` |
| mips / mipsle | `GOMIPS=hardfloat,softfloat` | `mips.hardfloat` / `mips.softfloat` |
| ppc64 / ppc64le | `GOPPC64=power8,power9,power10` | `ppc64.power8` 等 |
| riscv64 | `GORISCV64=rva20u64,…` | `riscv64.rva20u64` 等 |
| wasm | `GOWASM=satconv,signext` | `wasm.satconv` / `wasm.signext` |

设定某个特性等级会自动带上之前所有等级的 tag（如 `GOAMD64=v2` 同时设置 `amd64.v1` 和 `amd64.v2`）。

**约定俗成的自定义 tag**：

- `ignore`：文件不参与任何构建。
- `purego`：纯 Go 实现版本。
- `generate`：`go generate` 执行时设置。

**特殊文件名后缀**（隐式约束）：`*_GOOS`、`*_GOARCH`、`*_GOOS_GOARCH`。

**GOOS 别名映射**：`android` → `linux`+`android`；`ios` → `darwin`+`ios`；`illumos` → `solaris`+`illumos`。

---

> 以上内容整理自 go.dev 官方文档（`cmd/go`、`cmd/link`），版本以 go1.26 为准。
