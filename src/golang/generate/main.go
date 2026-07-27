package main

import "fmt"

//go:generate go run golang.org/x/tools/cmd/stringer@latest -type=Pill

// Pill 是药片种类，只想让它能打印出名字而不是数字。
type Pill int

const (
	Placebo       Pill = iota // 安慰剂
	Aspirin                    // 阿司匹林
	Ibuprofen                 // 布洛芬
	Paracetamol               // 扑热息痛
	Acetaminophen = Paracetamol // 与 Paracetamol 同值
)

func main() {
	fmt.Printf("headache -> take %v\n", Aspirin)
	fmt.Printf("fever    -> take %v\n", Paracetamol)
	fmt.Printf("alias    -> take %v\n", Acetaminophen)
}
