package perf

import (
	"testing"
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
	for i := 0; i < b.N; i++ {
		SettleV1(c)
	}
}

func BenchmarkSettleV2(b *testing.B) {
	c := sampleCart()
	b.ReportAllocs()
	for i := 0; i < b.N; i++ {
		SettleV2(c)
	}
}
