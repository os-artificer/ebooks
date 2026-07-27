//go:build ignore

// gen.go 是一个自定义 generator：读取 data.json 作为数据源，用 text/template
// 渲染 page.tmpl，产出 index.html。
// 开头的 //go:build ignore 让正常 `go build .` 跳过它，它只由
// `go generate` 触发（即 `go run gen.go`）。
package main

import (
	"encoding/json"
	"log"
	"os"
	"text/template"
	"time"
)

// Link 是数据源 data.json 里的一条记录。
type Link struct {
	Name string `json:"name"`
	URL  string `json:"url"`
	Desc string `json:"desc"`
}

// pageData 是传给模板的全部数据。
type pageData struct {
	Title       string
	Links       []Link
	GeneratedAt string
}

func check(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	raw, err := os.ReadFile("data.json")
	check(err)

	var links []Link
	check(json.Unmarshal(raw, &links))

	tmpl, err := template.ParseFiles("page.tmpl")
	check(err)

	out, err := os.Create("index.html")
	check(err)
	defer out.Close()

	check(tmpl.Execute(out, pageData{
		Title:       "Go 学习资源导航",
		Links:       links,
		GeneratedAt: time.Now().Format("2006-01-02 15:04:05"),
	}))
	log.Printf("generated index.html (%d links)\n", len(links))
}
