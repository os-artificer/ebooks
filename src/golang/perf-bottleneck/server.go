//go:build ignore

// server.go 是一个演示用的 HTTP 服务入口，用来展示"真实服务里
// 怎么在线抓性能数据"：它注册了 net/http/pprof 的处理器，
// 启动后就能用 `go tool pprof` 远程抓取 CPU profile / heap profile。
//
// 用 //go:build ignore 排除出正常 `go build .`（本目录已有一个
// 含 main 的 bench/main，不能有两个 main）。演示时这样跑：
//   go run server.go
package main

import (
	"log"
	"net/http"
	_ "net/http/pprof" // 注册 /debug/pprof/* 路由

	"perf" // 引用本目录的业务逻辑（需先 go mod init perf）
)

func main() {
	c := perf.Cart{UserID: 1}
	// 一个简单的结算端点，故意循环调用以制造可被 pprof 捕捉的 CPU 热点。
	http.HandleFunc("/settle", func(w http.ResponseWriter, r *http.Request) {
		for i := 0; i < 1000; i++ {
			_, _ = perf.SettleV1(c) // 用 V1 故意留坑，方便抓到热点
		}
		_, _ = w.Write([]byte("ok"))
	})

	log.Println("pprof on http://localhost:6060/debug/pprof/ ; settle on http://localhost:6060/settle")
	log.Fatal(http.ListenAndServe("localhost:6060", nil))
}
