//go:build ignore

// profile_main.go 演示"脱离 HTTP、直接在程序里抓取并自采样 CPU profile"
// 的离线方式：适合在分析阶段快速定位热函数，不必起服务。
//
// 跑法：
//   go run profile_main.go            # 默认跑 3s 采样，打印 top
//   go tool pprof -http=:8080 cpu.out # 结束后用浏览器看火焰图
package main

import (
	"os"
	"runtime/pprof"
	"time"

	"perf"
)

func main() {
	f, _ := os.Create("cpu.out")
	pprof.StartCPUProfile(f)
	defer pprof.StopCPUProfile()

	c := perf.Cart{UserID: 1}
	// 故意用 V1 制造 CPU 热点：逐行序列化 + 多余 map 分配
	deadline := time.Now().Add(3 * time.Second)
	for time.Now().Before(deadline) {
		for i := 0; i < 2000; i++ {
			_, _ = perf.SettleV1(c)
		}
	}
}
