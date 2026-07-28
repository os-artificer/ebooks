package perf

import "encoding/json"

// SettleV2 是针对 V1 三个坑的优化版：
//
//  1. 整张购物车一次性 json.Marshal，反射只发生一次，
//     且标准库内部会预分配缓冲；
//  2. 总价循环与序列化合并，不该存在的 []string 中间层彻底消失；
//  3. 删掉那个毫无收益、却额外分配哈希表的 map。
func SettleV2(c Cart) (string, float64) {
	blob, err := json.Marshal(c) // 一次反射 + 内部预分配
	if err != nil {
		return "[]", 0
	}

	var total float64
	for _, it := range c.Items {
		total += it.UnitPrice * float64(it.Qty)
	}
	return string(blob), total
}
