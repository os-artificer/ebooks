package main

import (
	"fmt"
	"os"
)

//go:generate go run gen.go

// main 不是本例重点，它只是用来验证"生成物已就位"。
// 真正的产物是 gen.go 渲染出来的 index.html。
func main() {
	info, err := os.Stat("index.html")
	if err != nil {
		fmt.Println("index.html 还没生成，请先执行: go generate ./...")
		return
	}
	fmt.Printf("index.html 已生成，大小 %d 字节，用浏览器打开即可查看\n", info.Size())
}
