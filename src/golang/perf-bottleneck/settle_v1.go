package perf

import (
	"encoding/json"
	"strings"
)

// SettleV1 是初版结算逻辑，埋了几个真实项目里常见的性能坑：
//
//  1. 逐行 json.Marshal 再拼字符串 —— 每一行都触发一次反射序列化，
//     还产生大量短命临时 []byte；
//  2. []string 没预分配容量，append 过程反复扩容拷贝；
//  3. 用 map 做"单价聚合"纯属多余，却额外分配了一棵哈希表，
//     而且遍历 map 是无序的，没有任何收益。
func SettleV1(c Cart) (string, float64) {
	// 坑 1 + 坑 2：逐行序列化，且 parts 零容量起步
	parts := make([]string, 0)
	for _, it := range c.Items {
		b, _ := json.Marshal(it) // 每行一次反射
		parts = append(parts, string(b))
	}
	blob := "[" + strings.Join(parts, ",") + "]"

	// 坑 3：无意义的 map 聚合，只为演示"多余分配"
	byPrice := make(map[float64]int)
	for _, it := range c.Items {
		byPrice[it.UnitPrice]++
	}
	_ = byPrice // 占位：真实项目里可能是"统计不同单价个数"之类

	// 真正有用的总价计算（这一步本身没问题，但前面的浪费已经造成）
	var total float64
	for _, it := range c.Items {
		total += it.UnitPrice * float64(it.Qty)
	}
	return blob, total
}
