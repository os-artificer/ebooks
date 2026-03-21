# ebooks 是什么？
算是我写的电子书合集吧。

# ebooks 项目背景？
从2010年大学毕业开始就一直从事软件开发的工作，此前一直忙碌感觉什么也没有留下。
如今终于有些时间了，希望把自己学到的知识进行记录沉淀，于是有了本项目。

# 从 drafts发布

草稿在 **`drafts/`**（`cpp` / `golang` / `tech-arch`）。

在仓库根目录执行 **`python tools/publish_article.py`** 会发布该目录下全部文章，输出到 **`web/page/`**。

只发布某几篇时传入 Markdown 路径（可多篇），须位于 **`drafts/`** 下，例如：

`python tools/publish_article.py drafts/cpp/某篇.md`

# 如何订阅更新？

![微信订阅](./web/images/wechat-qrcode.png)