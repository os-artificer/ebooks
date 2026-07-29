package perf

import (
	"testing"
)

// 包级 sink：消费 benchmark 的返回值，防止编译器把"看似无用"的调用优化掉，
// 测出空转的数字（对应正文 ⚠️ 里讲的那个经典坑）。
var (
	sinkBlob  string
	sinkTotal float64
)

// 构造一个中等规模的购物车（50 个商品行）作为基准输入。
func sampleCart() Cart {
	c := Cart{UserID: 1001}
	c.Items = make([]Sku, 50)
	for i := range c.Items {
		c.Items[i] = Sku{
			ID:        int64(i + 1),
			Name:      "sku-" + string(rune('A'+i%26)) + "-name",
			UnitPrice: 9.9 + float64(i),
			Qty:       (i % 5) + 1,
		}
	}
	return c
}

func BenchmarkSettleV1(b *testing.B) {
	c := sampleCart()
	b.ReportAllocs()
	var s string
	var t float64
	for i := 0; i < b.N; i++ {
		s, t = SettleV1(c)
	}
	sinkBlob, sinkTotal = s, t
}

func BenchmarkSettleV2(b *testing.B) {
	c := sampleCart()
	b.ReportAllocs()
	var s string
	var t float64
	for i := 0; i < b.N; i++ {
		s, t = SettleV2(c)
	}
	sinkBlob, sinkTotal = s, t
}
