package perf

// Sku 表示购物车里的一个商品行。
type Sku struct {
	ID       int64   `json:"id"`
	Name     string  `json:"name"`
	UnitPrice float64 `json:"unit_price"`
	Qty      int     `json:"qty"`
}

// Cart 是用户的购物车，聚合了若干商品行。
type Cart struct {
	UserID int64  `json:"user_id"`
	Items  []Sku `json:"items"`
}
