# Go 生产级 Panic 防护实战：从一个 safe 包看防御性编程的艺术

> **作者：**岭南过客  
> **更新时间：**2026-04-16

在 Go 语言的并发编程中，panic 是一颗随时可能引爆的"地雷"——一个未被 recover 的 goroutine panic 会直接导致整个进程崩溃。
对于需要 7×24 小时运行的服务端程序而言，这种行为是灾难性的。

很多团队的做法是在每个 goroutine 入口处写一段 `defer recover()` 的模板代码，但这种"复制-粘贴"式的防护往往既不统一，也不完备——日志格式五花八门、调用栈信息缺失、敏感数据泄漏到日志，甚至 recover 逻辑本身也可能 panic。

本文将以 `safe` 包为蓝本（文末附录有源码），深入解析如何设计一套统一、可扩展、自身 panic-safe 的防护框架。

文章涵盖设计动机、API 架构、四层防护链原理、Functional Options 模式落地，以及工程最佳实践。

## 一、问题背景：为什么需要统一的 Panic 防护？

### 1.1 Go Panic 的致命特性

Go 的 panic/recover 机制有两个关键特性需要牢记：

- **goroutine 隔离性**：`recover()` 只能捕获当前 goroutine 的 panic，主 goroutine 无法 recover 子 goroutine 的 panic。
- **进程级杀伤力**：任何一个 goroutine 中未被 recover 的 panic 都会导致整个进程退出。

```
                      +-----------+
                      |   main    |
                      | goroutine |
                      +-----+-----+
                            |
                  +---------+---------+
                  |                   |
            +-----+-----+      +-----+-----+
            | goroutine A|      | goroutine B|
            |  (safe.Go) |      |   (bare)   |
            +-----+------+      +-----+------+
                  |                    |
             panic -> recover     panic -> ???
             (进程存活)            (进程崩溃)
```

### 1.2 "复制粘贴式" Recover 的痛点

实际项目中，团队最常见的做法如下：

```go
go func() {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("panic: %v", r)
        }
    }()
    doWork()
}()
```

这段代码虽然能工作，但在大型项目中会暴露出多重问题：

| 痛点 | 具体表现 |
|------|---------|
| 日志不统一 | 有的用 `log.Printf`，有的用 `logger.Error`，格式各异 |
| 调用栈缺失 | 很多地方只记录了 panic 值，没有 `debug.Stack()` |
| 敏感信息泄漏 | panic 值可能包含 token、密码等敏感信息直接写入日志 |
| 缺乏回调机制 | 无法统一触发告警、上报指标 |
| recover 本身不安全 | logger 或回调函数自身 panic 会导致防护链断裂 |
| 语义不清晰 | 同步场景能否 re-panic？异步场景是否允许？缺乏约束 |

## 二、API 全景：五个入口函数覆盖全场景

`safe` 包将日常开发中的 panic 防护需求抽象为同步执行和异步执行两大类，共五个核心函数：

```
+-- 同步 --+                     +---- 异步 ----+
|          |                     |              |
|   Run    |                     |   Go         |  fire-and-forget
|          |                     |   GoWait     |  + 等待完成
+----------+                     |   GoCtx      |  + 上下文传递
                                 |   GoCtxWait  |  + 上下文 + 等待
                                 +--------------+
```

### 2.1 同步执行：Run

```go
safe.Run(func() {
    riskyWork()
}, safe.WithLabel("data-import"))
```

`Run` 在当前 goroutine 中执行目标函数。如果发生 panic，会进行 recover、记录日志、触发回调，然后正常返回——除非显式开启了 `WithRepanic`。

适用场景：HTTP Handler、RPC Handler 中需要"兜底"但不想开新 goroutine 的情况。

### 2.2 异步执行家族：Go / GoWait / GoCtx / GoCtxWait

```go
// fire-and-forget
safe.Go(func() { backgroundSync() })

// 需要等待完成
wait := safe.GoWait(func() { batchProcess() })
wait() // 阻塞直到完成

// 携带 context，用于回调中判断取消状态
safe.GoCtx(ctx, func() { streamProcess() },
    safe.WithOnPanic(func(pi safe.PanicInfo) {
        if pi.Ctx.Err() != nil {
            return // context 已取消，无需告警
        }
        alerting.Send(pi)
    }),
)
```

四个异步函数的关系可以用一张矩阵表示：

```
                  fire-and-forget     可等待(wait)
                 +------------------+------------------+
  无 context     |      Go          |     GoWait       |
                 +------------------+------------------+
  有 context     |      GoCtx       |     GoCtxWait    |
                 +------------------+------------------+
```

> 设计决策：`GoCtx` 的 ctx 仅传入 `PanicInfo.Ctx` 供回调使用，不会自动取消 fn 的执行。这遵循了 Go 社区"显式优于隐式"的惯例——取消逻辑应由 fn 内部通过 `ctx.Done()` 自行处理。

### 2.3 Defer 风格的 Recover 辅助函数

除了"包装执行"模式，`safe` 包还提供了一组 `defer` 风格的轻量辅助函数，适用于无法使用 `Run/Go` 包装的场景：

| 函数 | 用途 | 典型场景 |
|------|------|---------|
| `RecoverToError(&err)` | panic 转 error，保留调用栈 | 需要返回 error 的函数 |
| `RecoverWithHandler(fn)` | panic 时调用自定义 handler | 需要自定义处理但不想用完整 Option |
| `RecoverNoop()` | 静默吞掉 panic | 明确知道 panic 可忽略的清理逻辑 |
| `RecoverAndStackHandler(fn)` | 回调接收 `(msg, stack)` | 需要分离 panic 值和堆栈 |
| `RecoverWithDebugStackHandler(fn)` | 同上，使用 `debug.Stack()` | 需要完整的 debug 堆栈信息 |

```go
func ProcessItem(item Item) (err error) {
    defer safe.RecoverToError(&err)
    // 如果这里 panic，err 会被设置为包含调用栈的 error
    return transform(item)
}
```

## 三、Functional Options 模式：灵活且向后兼容的配置体系

`safe` 包使用经典的 Functional Options 模式来配置行为，这是 Go 社区广泛认可的 API 设计模式。

### 3.1 Option 定义

```go
type Option func(*config)

type config struct {
    label         string
    log           Logger
    onPanic       func(PanicInfo)
    repanic       bool
    stackMaxBytes int
    sanitizer     PanicSanitizer
}
```

### 3.2 六个配置项

```
+---------------------+--------------------------------------------+
|      Option         |              作用                          |
+---------------------+--------------------------------------------+
| WithLabel(s)        | 为日志附加标签，便于定位问题来源          |
| WithLogger(l)       | 自定义 logger，未设置时使用标准库 log     |
| WithOnPanic(fn)     | panic 后的回调：指标上报、告警、资源清理   |
| WithRepanic(bool)   | 同步模式下 re-panic；异步模式自动忽略      |
| WithStackMaxBytes(n)| 限制堆栈长度，防止日志系统被撑爆           |
| WithPanicSanitizer  | 脱敏处理，防止 token/密码泄漏到日志        |
+---------------------+--------------------------------------------+
```

### 3.3 设计亮点

**nil 安全**：`newConfig` 在遍历 Options 时会跳过 nil，这意味着调用方可以安全地传入条件 Option：

```go
safe.Run(fn,
    safe.WithLabel("task"),
    optionalOption, // 可能为 nil，不会 panic
)
```

**后者覆盖前者**：同名 Option 多次传入时，后传入的值生效，符合直觉的"最后写入胜出"语义。

## 四、四层防护链：核心架构深度剖析

`safe` 包最精妙的设计在于其四层防护链——不仅保护业务代码的 panic，还保护防护机制自身不会因为扩展点的 bug 而失效。

### 4.1 Panic 处理全流程

```
   业务函数 fn 发生 panic
          |
          v
   +------+-------+
   |    guard()    |  第 0 层：捕获业务 panic
   |  defer recover|
   +------+--------+
          |
          v
   +------+-----------+
   | handlePanic()    |  核心处理流程
   |                  |
   |  1. debug.Stack  |  获取原始堆栈
   |  2. copyStack    |  第 1 次截断
   |  3. sanitizer    |  脱敏处理（如果配置了）
   |  4. copyStack    |  第 2 次截断（脱敏后可能变长）
   |  5. safeLog      |  安全记录日志
   |  6. safeOnPanic  |  安全触发回调
   |  7. maybeRepanic |  是否重新 panic
   +------------------+
```

### 4.2 四层防护的 safe 包装

这里是整个框架最核心的设计理念：每一个可能失败的扩展点都被独立的 recover 包裹。

```
+---------------------------------------------------+
|              handlePanic (主流程)                 |
|                                                   |
|  +------------------+                             |
|  | safeSanitize()   |  <-- 脱敏函数自身 panic?    |
|  | defer recover    |      回退到原始reason/stack |
|  +------------------+                             |
|           |                                       |
|           v                                       |
|  +------------------+                             |
|  | safeLog()        |  <-- logger 自身 panic?     |
|  | defer recover    |      降级到 log.Printf      |
|  +------------------+                             |
|           |                                       |
|           v                                       |
|  +------------------+                             |
|  | safeOnPanic()    |  <-- 回调函数自身 panic?    |
|  | defer recover    |      记录日志但不传播        |
|  +------------------+                             |
|           |                                       |
|           v                                       |
|  +------------------+                             |
|  | safeWarn()       |  <-- 警告日志自身 panic?    |
|  | defer recover    |      降级到 log.Printf      |
|  +------------------+                             |
+---------------------------------------------------+
```

来看 `safeLog` 的实现，感受这种"层层兜底"的防御思路：

```go
func safeLog(cfg *config, label, stackStr string, reason any) {
    defer func() {
        if r := recover(); r != nil {
            // 自定义 logger 炸了？降级到标准库
            log.Printf(
                "[safe] panic in logger itself, label: %s, logPanic: %v, "+
                "originalReason: %v, stack: %s",
                label, r, reason, stackStr,
            )
        }
    }()
    logPanic(cfg, label, stackStr, reason)
}
```

`safeOnPanic` 同理：

```go
func safeOnPanic(cfg *config, label string, pi PanicInfo) {
    defer func() {
        if r := recover(); r != nil {
            log.Printf(
                "[safe] panic in OnPanic callback, label: %s, "+
                "callbackPanic: %v, originalReason: %v, stack: %s",
                label, r, pi.Reason, string(pi.Stack),
            )
        }
    }()
    cfg.onPanic(pi)
}
```

降级到标准库 `log` 是最后的保底手段——标准库 `log.Printf` 几乎不可能 panic，即使自定义 logger 完全不可用，关键信息也不会丢失。

### 4.3 脱敏器的双重截断机制

`PanicSanitizer` 可能将一个短 stack 替换为一个很长的"安全"stack，导致突破 `WithStackMaxBytes` 限制。`safe` 包通过双重截断解决这个问题：

```
 原始 debug.Stack()
       |
       v
  copyStack (第 1 次截断，按 stackMaxBytes)
       |
       v
  sanitizer (可能产生更长的 stack)
       |
       v
  copyStack (第 2 次截断，再次按 stackMaxBytes)
       |
       v
  最终的 finalStack (保证不超限)
```

对应代码逻辑：

```go
if cfg.sanitizer != nil {
    finalReason, finalStack = safeSanitize(cfg.sanitizer, reason, stack, label)
    var reTruncated bool
    finalStack, reTruncated = copyStack(finalStack, cfg.stackMaxBytes)
    if reTruncated {
        truncated = true
    }
}
```

## 五、同步 vs 异步：WithRepanic 的精确语义控制

`WithRepanic` 的设计体现了对 Go 并发模型的深刻理解：

```
+--------------------+-------------------+-------------------+
|                    |    同步 (Run)      |  异步 (Go/GoWait) |
+--------------------+-------------------+-------------------+
| WithRepanic(false) | recover + 返回     | recover + 返回    |
| WithRepanic(true)  | recover + re-panic | recover + 警告    |
+--------------------+-------------------+-------------------+
```

为什么异步模式要忽略 re-panic？

在同步模式下，调用者可以在外层用 `recover()` 捕获 re-panic，实现"记录日志后继续向上传播"的语义。但在异步模式下，re-panic 会导致整个进程崩溃——这恰恰是 `safe` 包要防止的事情。

因此异步模式下 re-panic 请求会被忽略，并通过 `safeWarn` 输出警告日志，提醒开发者检查配置。

re-panic 使用原始值：即使配置了 `PanicSanitizer`，re-panic 抛出的也是原始、未脱敏的 panic 值。这确保了上游 `recover()` 能够拿到真实 panic 值进行精确错误处理。

## 六、堆栈截断：copyStack 的实现细节

堆栈截断看似简单，实际上有几个值得注意的工程细节：

```go
func copyStack(full []byte, max int) ([]byte, bool) {
    if max <= 0 || len(full) <= max {
        return cloneBytes(full), false
    }

    out := cloneBytes(full[:max])
    if max > len(shortTruncationSuffix) {
        copy(out[max-len(shortTruncationSuffix):], shortTruncationSuffix)
    }
    return out, true
}
```

三个设计决策：

1. **深拷贝**：使用 `cloneBytes` 而非直接切片引用，避免持有 `debug.Stack()` 返回的大数组引用，防止内存滞留。
2. **截断标记**：在截断末尾附加 `...`，让日志阅读者明确知道堆栈被截断。
3. **极端短 max 的处理**：当 `max <= len("...")` 时不附加截断标记，避免覆盖掉有限的有效信息。

## 七、生产实战：在通用后台服务中的应用

在典型的微服务、任务调度器或插件化后台系统中，可以用 `safe.Go` 包裹所有后台组件启动点：

```go
// 核心后台任务
safe.Go(func() {
    defer runWg.Done()
    if err := worker.Run(ctx); err != nil && ctx.Err() == nil {
        logger.Warnf("worker exited: %v", err)
    }
})

// 插件或子任务
for _, plugin := range plugins {
    runWg.Add(1)
    plugin := plugin
    safe.Go(func() {
        defer runWg.Done()
        plugin.Run(ctx)
    })
}
```

这种模式带来几个显著优势：

1. **进程稳定性**：任何一个插件或任务 panic 都不会拖垮整个服务。
2. **问题可追踪**：统一日志格式更易接入日志平台和告警系统。
3. **代码简洁**：业务代码无需重复 recover 模板，只需用 `safe.Go` 替代 `go`。

## 八、最佳实践总结

从 `safe` 包的设计与使用中，可以提炼出以下工程实践：

### 实践一：永远不要让 goroutine "裸奔"

规则：项目中所有 `go func()` 都应通过 `safe.Go` 或等价安全包装器启动。

可以通过 linter（例如自定义 `go vet` 检查或 `golangci-lint` 插件）在 CI 中强制执行。

### 实践二：防护代码本身也需要防护

四层 `defer recover` 揭示了一个深层原则：任何扩展点都可能失败。如果 recover 逻辑依赖了 logger、metrics client、HTTP client，这些依赖本身也可能 panic。

```
设计原则：防护层的每一个外部调用都必须有独立 recover 保护。
降级策略：标准库 log 是最后防线。
```

### 实践三：区分同步和异步的 re-panic 语义

同步场景下 re-panic 是合理的——调用方可以 recover 并继续做决策。但异步 goroutine 中 re-panic 等于自杀，必须在框架层面禁止。

### 实践四：日志脱敏是框架责任，不是业务责任

通过 `WithPanicSanitizer`，脱敏逻辑可以提升到框架层：

```go
safe.Go(handler,
    safe.WithPanicSanitizer(func(reason any, stack []byte) (any, []byte) {
        s := fmt.Sprint(reason)
        s = tokenRegex.ReplaceAllString(s, "[REDACTED]")
        return s, stack
    }),
)
```

业务代码无需在每个 recover 里重复脱敏逻辑，也不会因遗漏导致敏感信息泄漏。

### 实践五：堆栈长度必须有上限

生产环境中，一个深递归 panic 可能产生数十 KB 堆栈。如果直接写入日志系统，可能触发：

- 日志采集组件消息大小限制
- Kafka/ES 等存储层单条消息限制
- 日志查看工具渲染卡顿

`WithStackMaxBytes` 可在框架层统一解决，建议设置为 4KB 到 16KB。

### 实践六：深拷贝堆栈数据

`debug.Stack()` 返回的 `[]byte` 可能指向较大底层数组。如果直接长期持有切片引用，会阻止 GC 回收整块底层数组。`safe` 包通过 `cloneBytes` 深拷贝避免这一陷阱。

### 实践七：用 Functional Options 保持 API 演进能力

Option 模式使新增配置项不需要修改已有调用代码。未来若新增 `WithRetry(n)` 或 `WithTimeout(d)`，只需增加 Option 函数和 `config` 字段，现有调用方零改动。

## 九、与社区方案的对比

| 特性 | 手动 defer/recover | safe 包 | 社区 errgroup |
|------|-------------------|---------|--------------|
| 统一日志格式 | ✗ | ✓ | ✗ |
| 堆栈信息 | 手动获取 | 自动捕获 + 截断 | ✗ |
| 敏感信息脱敏 | ✗ | ✓ (sanitizer) | ✗ |
| panic 回调 | ✗ | ✓ (OnPanic) | ✗ |
| 防护链自身安全 | ✗ | ✓ (四层保护) | ✗ |
| 同步/异步语义 | 无区分 | 精确区分 | 仅异步 |
| re-panic 控制 | 手动 | 框架管控 | ✗ |
| 等待完成 | 手动 WaitGroup | GoWait 内置 | ✓ |
| context 传递 | 手动 | GoCtx 内置 | ✓ |

`errgroup` 专注于"一组 goroutine 的错误收集与取消"，`safe` 包专注于"panic 防护与可观测性"。两者解决的是不同层面问题，在实际项目中可以互补使用。

## 十、总结

`safe` 包的设计理念可以浓缩为一句话：让 panic 防护成为基础设施，而非散落在业务代码中的临时补丁。

它通过以下设计实现这一目标：

1. 统一入口：`Run/Go/GoWait/GoCtx/GoCtxWait` 覆盖同步、异步、等待、上下文传递组合。
2. Functional Options：灵活配置、向后兼容、零侵入。
3. 四层防护链：sanitizer -> logger -> callback -> warn，每一层都有独立 recover。
4. 精确语义控制：同步 re-panic vs 异步忽略，原始值 re-panic vs 脱敏值日志。
5. 工程化细节：深拷贝堆栈、双重截断、nil Option 安全、标准库降级。

这些设计决策背后的共同原则是：在分布式系统中，任何组件都可能以意想不到的方式失败，而基础设施代码的职责就是确保失败不会级联扩散。

## 十一、附录：safe 包完整源码

本节给出自包含的完整实现。`Logger` 接口在包内定义，未注入自定义 logger 时统一降级到标准库 `log.Printf`。

### 11.1 常量、类型与入口 API

```go
package safe

import (
    "context"
    "fmt"
    "log"
    "runtime"
    "runtime/debug"
    "sync"
    "time"
)

const shortTruncationSuffix = "..."

type Logger interface {
    Debugf(format string, args ...any)
    Infof(format string, args ...any)
    Warnf(format string, args ...any)
    Errorf(format string, args ...any)
    Fatalf(format string, args ...any)
}

type execMode uint8

const (
    execModeSync execMode = iota
    execModeAsync
)

type PanicSanitizer func(reason any, stack []byte) (safeReason any, safeStack []byte)

type PanicInfo struct {
    Ctx         context.Context
    Reason      any
    Stack       []byte
    Truncated   bool
    RecoveredAt time.Time
}

func Run(fn func(), opts ...Option) {
    cfg := newConfig(opts)
    guard(&cfg, nil, false, execModeSync, fn)
}

func Go(fn func(), opts ...Option) {
    cfg := newConfig(opts)
    go func() {
        guard(&cfg, nil, false, execModeAsync, fn)
    }()
}

func GoWait(fn func(), opts ...Option) func() {
    cfg := newConfig(opts)
    var wg sync.WaitGroup
    wg.Go(func() {
        guard(&cfg, nil, false, execModeAsync, fn)
    })
    return wg.Wait
}

func GoCtx(ctx context.Context, fn func(), opts ...Option) {
    if ctx == nil {
        ctx = context.Background()
    }
    cfg := newConfig(opts)
    captured := ctx
    go func() {
        guard(&cfg, captured, true, execModeAsync, fn)
    }()
}

func GoCtxWait(ctx context.Context, fn func(), opts ...Option) func() {
    if ctx == nil {
        ctx = context.Background()
    }
    cfg := newConfig(opts)
    captured := ctx
    var wg sync.WaitGroup
    wg.Go(func() {
        guard(&cfg, captured, true, execModeAsync, fn)
    })
    return wg.Wait
}

func FormatPanicInfo(pi PanicInfo) string {
    return fmt.Sprintf(
        "reason: %v, truncated: %t, recoveredAt: %s, stack: %s",
        pi.Reason,
        pi.Truncated,
        pi.RecoveredAt.Format(time.RFC3339),
        string(pi.Stack),
    )
}
```

### 11.2 Functional Options

```go
type Option func(*config)

type config struct {
    label         string
    log           Logger
    onPanic       func(PanicInfo)
    repanic       bool
    stackMaxBytes int
    sanitizer     PanicSanitizer
}

func newConfig(opts []Option) config {
    c := config{}
    for _, o := range opts {
        if o != nil {
            o(&c)
        }
    }
    return c
}

func WithLabel(label string) Option {
    return func(c *config) { c.label = label }
}

func WithLogger(log Logger) Option {
    return func(c *config) { c.log = log }
}

func WithOnPanic(h func(PanicInfo)) Option {
    return func(c *config) { c.onPanic = h }
}

func WithPanicSanitizer(s PanicSanitizer) Option {
    return func(c *config) { c.sanitizer = s }
}

func WithRepanic(repanic bool) Option {
    return func(c *config) { c.repanic = repanic }
}

func WithStackMaxBytes(n int) Option {
    return func(c *config) {
        if n < 0 {
            n = 0
        }
        c.stackMaxBytes = n
    }
}
```

### 11.3 核心 Panic 处理链

```go
func guard(cfg *config, opCtx context.Context, ctxForPanic bool, mode execMode, fn func()) {
    defer func() {
        if r := recover(); r != nil {
            handlePanic(cfg, opCtx, ctxForPanic, mode, r)
        }
    }()
    fn()
}

func handlePanic(cfg *config, opCtx context.Context, ctxForPanic bool, mode execMode, reason any) {
    full := debug.Stack()
    stack, truncated := copyStack(full, cfg.stackMaxBytes)
    label := cfg.label

    if label == "" {
        label = "-"
    }

    recoveredAt := time.Now()
    finalReason := reason
    finalStack := stack

    if cfg.sanitizer != nil {
        finalReason, finalStack = safeSanitize(cfg.sanitizer, reason, stack, label)
        var reTruncated bool
        finalStack, reTruncated = copyStack(finalStack, cfg.stackMaxBytes)
        if reTruncated {
            truncated = true
        }
    }

    stackStr := string(finalStack)
    safeLog(cfg, label, stackStr, finalReason)

    pi := PanicInfo{
        Reason:      finalReason,
        Stack:       finalStack,
        Truncated:   truncated,
        RecoveredAt: recoveredAt,
    }

    if ctxForPanic {
        pi.Ctx = opCtx
    }

    if cfg.onPanic != nil {
        safeOnPanic(cfg, label, pi)
    }

    maybeRepanic(cfg, label, mode, reason)
}

func maybeRepanic(cfg *config, label string, mode execMode, reason any) {
    if !cfg.repanic {
        return
    }
    if mode == execModeAsync {
        safeWarn(cfg, "WithRepanic is ignored in async mode, label: %s", label)
        return
    }
    panic(reason)
}
```

### 11.4 四层安全包装

```go
func safeLog(cfg *config, label, stackStr string, reason any) {
    defer func() {
        if r := recover(); r != nil {
            log.Printf(
                "[safe] panic in logger itself, label: %s, logPanic: %v, originalReason: %v, stack: %s",
                label, r, reason, stackStr,
            )
        }
    }()
    logPanic(cfg, label, stackStr, reason)
}

func logPanic(cfg *config, label, stackStr string, reason any) {
    if cfg.log != nil {
        cfg.log.Errorf("panic recovered, label: %s, reason: %v, stack: %s", label, reason, stackStr)
    } else {
        log.Printf("[safe] panic recovered, label: %s, reason: %v, stack: %s", label, reason, stackStr)
    }
}

func safeOnPanic(cfg *config, label string, pi PanicInfo) {
    defer func() {
        if r := recover(); r != nil {
            log.Printf(
                "[safe] panic in OnPanic callback, label: %s, callbackPanic: %v, originalReason: %v, stack: %s",
                label, r, pi.Reason, string(pi.Stack),
            )
        }
    }()
    cfg.onPanic(pi)
}

func safeWarn(cfg *config, format string, args ...any) {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("[safe] logger panic while writing warning: "+format, args...)
        }
    }()
    if cfg.log != nil {
        cfg.log.Warnf(format, args...)
        return
    }
    log.Printf("[safe] warning: "+format, args...)
}

func safeSanitize(
    s PanicSanitizer,
    reason any,
    stack []byte,
    label string,
) (safeReason any, safeStack []byte) {
    safeReason = reason
    safeStack = stack
    defer func() {
        if r := recover(); r != nil {
            log.Printf(
                "[safe] panic in PanicSanitizer, label: %s, sanitizerPanic: %v",
                label, r,
            )
            safeReason = reason
            safeStack = stack
        }
    }()
    return s(reason, stack)
}
```

### 11.5 堆栈工具函数

```go
func copyStack(full []byte, max int) ([]byte, bool) {
    if max <= 0 || len(full) <= max {
        return cloneBytes(full), false
    }
    out := cloneBytes(full[:max])
    if max > len(shortTruncationSuffix) {
        copy(out[max-len(shortTruncationSuffix):], shortTruncationSuffix)
    }
    return out, true
}

func cloneBytes(src []byte) []byte {
    out := make([]byte, len(src))
    copy(out, src)
    return out
}
```

### 11.6 defer 辅助函数

```go
func RecoverToError(err *error) {
    if r := recover(); r != nil {
        const size = 64 << 10
        stack := make([]byte, size)
        stack = stack[:runtime.Stack(stack, false)]
        *err = fmt.Errorf("recovered from panic: panic=%v (original err=%v)\nstack:\n%s", r, *err, string(stack))
    }
}

func RecoverWithHandler(handler func(any)) func() {
    return func() {
        if r := recover(); r != nil {
            if handler != nil {
                safeCallHandler(handler, r)
                return
            }
            const size = 64 << 10
            buf := make([]byte, size)
            buf = buf[:runtime.Stack(buf, false)]
            log.Printf("[safe] panic recovered, label: -, reason: %v, stack: %s", r, string(buf))
        }
    }
}

func safeCallHandler(handler func(any), panicValue any) {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("[safe] panic in RecoverWithHandler callback: callback=%v, original=%v", r, panicValue)
        }
    }()
    handler(panicValue)
}

func RecoverNoop() {
    _ = recover()
}

func RecoverAndStackHandler(handler func(panicMsg, stackTrace string)) func() {
    return func() {
        if r := recover(); r != nil {
            const size = 64 << 10
            buf := make([]byte, size)
            buf = buf[:runtime.Stack(buf, false)]
            if handler != nil {
                handler(fmt.Sprint(r), string(buf))
            }
        }
    }
}

func RecoverWithDebugStackHandler(handler func(panicMsg, fullStack string)) func() {
    return func() {
        if r := recover(); r != nil {
            if handler != nil {
                handler(fmt.Sprint(r), string(debug.Stack()))
            }
        }
    }
}
```
