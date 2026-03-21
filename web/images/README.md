# 图片说明

- **`wechat.png`**：公众号二维码原图（体积较大，约数 MB），保留作备份或线下使用。
- **`wechat-qrcode.png` / `wechat-qrcode.webp`**：从 `wechat.png` 缩放并压缩后的网页用图（约 400px 宽），供 `page/welcome.html` 等引用，加载更快。

重新生成压缩图（需已安装 Pillow，例如 `ebooks/.venv`）：

```bash
.venv/bin/python3 <<'PY'
from pathlib import Path
from PIL import Image
root = Path("images")
im = Image.open(root / "wechat.png")
if im.mode not in ("RGB", "RGBA"):
    im = im.convert("RGBA")
im.thumbnail((400, 400), Image.Resampling.LANCZOS)
im.save(root / "wechat-qrcode.png", "PNG", optimize=True, compress_level=9)
im.save(root / "wechat-qrcode.webp", "WEBP", quality=82, method=6)
PY
```

（在仓库 `ebooks/` 目录下执行。）
