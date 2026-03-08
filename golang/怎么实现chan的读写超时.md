# Go 中怎么实现 chan 读写超时？

Go 的 chan 读写默认是阻塞的：没有数据时读会一直等，缓冲区满时写也会一直等。

实际业务里往往需要“等一会儿就放弃”，也就是带超时的读写。

做法就是用 `select`：在“正常读写”和“超时”两个分支里二选一，谁先满足就执行谁。

下面按超时检测方式分类说明。

---

**一、基于 time 包**

time.After：单次读超时或写超时最简写法，在 `select` 里用 `case <-time.After(d)` 与读写 case 二选一。读超时：同时监听“从 chan 收数据”和“超时”，先到先执行；写超时同理，`case ch <- value` 与 `case <-time.After(d)` 二选一。

读超时示例：

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string, 1)
    go func() {
        time.Sleep(2 * time.Second)
        ch <- "result"
    }()

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-time.After(1 * time.Second):
        fmt.Println("读超时")
    }
}
```

写超时示例：

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan int, 1)
    ch <- 1 // 已满，再写会阻塞

    select {
    case ch <- 2:
        fmt.Println("写入成功")
    case <-time.After(100 * time.Millisecond):
        fmt.Println("写超时") // 无消费者时会走这里
    }
}
```

注意：`time.After(d)` 每次调用都会新建一个 `time.Timer`，到期前不会被 GC 回收，在长循环或高并发里反复调用会泄漏。循环里做超时请用下面的显式 Timer 或 context。

**推荐**：单次读/写超时直接用。
**不推荐**：循环或高并发里反复用，此时应改用显式 Timer 或 context。

显式 Timer：用 `time.NewTimer(d)`，在 `select` 里 `case <-timer.C`。用完后 `timer.Stop()` 回收，适合循环里复用。

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    timer := time.NewTimer(1 * time.Second)
    defer timer.Stop()

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-timer.C:
        fmt.Println("读超时")
    }
}
```

**推荐**：循环或需要复用同一 Timer 时用，记得 `defer timer.Stop()`，无泄漏问题。

Timer.Reset 循环复用：循环里多次做超时检测时，只建一个 Timer，每轮开始时 `timer.Reset(d)` 复用，避免每轮 NewTimer 或 time.After 泄漏。

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    go func() {
        time.Sleep(2 * time.Second)
        ch <- "result"
    }()

    timer := time.NewTimer(1 * time.Second)
    defer timer.Stop()
    for {
        timer.Reset(1 * time.Second)
        select {
        case v := <-ch:
            fmt.Println("收到:", v)
            return
        case <-timer.C:
            fmt.Println("本轮超时，继续等")
        }
    }
}
```

**推荐**：循环里多次做超时时用，一个 Timer 反复 `Reset`，既避免泄漏又清晰。
**不推荐**：每轮都 `NewTimer` 或 `time.After`。

time.AfterFunc：超时后执行回调，在回调里关闭或向 done chan 发送，适合超时后做清理或通知多处。

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    done := make(chan struct{})
    time.AfterFunc(1*time.Second, func() { close(done) })

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-done:
        fmt.Println("读超时")
    }
}
```

**推荐**：超时后需要执行回调、或通知多个 goroutine 时用。
**不推荐**：只做"到点就停"的简单超时，用 Timer/context 更直观。

**二、基于 context**

WithTimeout：最常用，超时在调用链里传递、统一取消都方便。读超时：`select` 里 `case v := <-ch` 与 `case <-ctx.Done()`；写超时把前者换成 `case ch <- value` 即可。

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
    defer cancel()

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-ctx.Done():
        fmt.Println("超时:", ctx.Err())
    }
}
```

**推荐**：超时需在调用链传递或统一取消时首选；代码清晰、无 Timer 泄漏。

WithDeadline：传入绝对截止时间，与 `WithTimeout` 等价，适合“在某时刻前必须完成”的场景。用法同上，仅创建 ctx 时用 `context.WithDeadline(parent, time)`。

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    deadline := time.Now().Add(1 * time.Second) // 绝对截止时刻
    ctx, cancel := context.WithDeadline(context.Background(), deadline)
    defer cancel()

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-ctx.Done():
        fmt.Println("超时:", ctx.Err())
    }
}
```

**推荐**：按绝对截止时间控制时用。

WithTimeoutCause（Go 1.20+）：与 `WithTimeout` 相同，但可传入自定义"取消原因"，便于区分超时、主动取消等。

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    ctx, cancel := context.WithTimeoutCause(context.Background(), 1*time.Second, fmt.Errorf("自定义超时原因"))
    defer cancel()

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-ctx.Done():
        fmt.Println("超时:", context.Cause(ctx)) // 拿到 WithTimeoutCause 传入的 error
    }
}
```

**推荐**：需要区分"超时"与"主动取消"、或在日志/监控里记录具体原因时用。Go 1.20 以下不可用。


**三、自建超时 chan**

单独 goroutine 在 sleep 后 close 或发送，select 监听该 chan，便于复用。

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    timeout := make(chan struct{})
    go func() {
        time.Sleep(1 * time.Second)
        close(timeout)
    }()

    select {
    case v := <-ch:
        fmt.Println("收到:", v)
    case <-timeout:
        fmt.Println("读超时")
    }
}
```

**推荐**：需要复用同一个 timeout channel、或自定义超时前后逻辑时用。**不推荐**仅为了“单次超时”而写，用 `time.After` 或 context 更简单。

**四、基于 reflect**

reflect.Select：channel 或 case 数量在运行时才确定时，用 `reflect.Select` 构造 select，把超时（如 `timer.C` 或 done chan）作为一个 case 加入即可。

```go
package main

import (
    "fmt"
    "reflect"
    "time"
)

func main() {
    ch := make(chan string)
    timer := time.NewTimer(1 * time.Second)
    defer timer.Stop()
    cases := []reflect.SelectCase{
        {Dir: reflect.SelectRecv, Chan: reflect.ValueOf(ch)},
        {Dir: reflect.SelectRecv, Chan: reflect.ValueOf(timer.C)},
    }
    chosen, v, _ := reflect.Select(cases)
    if chosen == 0 {
        fmt.Println("收到:", v.Interface())
    } else {
        fmt.Println("读超时")
    }
}
```

**推荐**：仅当 chan 或 case 数量在运行时才确定、必须动态构造 select 时用。**不推荐**常规业务用，可读性差、有反射开销。

**五、轮询方式**

轮询 + 截止时间：循环里用 `default` 非阻塞读，再 `time.Sleep` 一小段，重复直到超时或读到数据。

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string)
    deadline := time.Now().Add(1 * time.Second)
    for time.Now().Before(deadline) {
        select {
        case v := <-ch:
            fmt.Println("收到:", v)
            return
        default:
        }
        time.Sleep(10 * time.Millisecond)
    }
    fmt.Println("读超时")
}
```

**不推荐**：占用 CPU 做空转、间隔不好选（太短浪费，太长响应慢）、代码啰嗦。仅在没有 select 的极少数环境或历史代码里可能见到，新代码用 select + Timer/context。

---

**小结**

chan 读/写超时的本质都是：用 `select` 在“正常读写”和“超时信号”之间二选一。

| 场景/方式 | 推荐 | 不推荐 |
|-----------|------|--------|
| 单次超时 | `time.After`、`context.WithTimeout` | — |
| 循环/高并发超时 | 显式 `NewTimer`+`Stop`/`Reset`、context | 循环里用 `time.After`（泄漏） |
| 需区分取消原因 | `WithTimeoutCause`（Go 1.20+） | — |
| 动态多 chan | `reflect.Select` | — |
| 简单超时 | — | 自建 timeout chan、AfterFunc（过度设计） |
| 轮询实现 | — | 轮询+截止时间（浪费 CPU、难维护） |
