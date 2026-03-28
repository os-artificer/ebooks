#!/usr/bin/env python3
"""
Render markdown slices to PNG images for 小红书-style vertical notes.
Content is preserved verbatim per slice (line ranges from source file).
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import markdown
from playwright.sync_api import sync_playwright

# 1080px width is common for XHS; background and typography tuned for readability.
HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<!-- Headless Linux 常无黑体/苹方；用 Web 字体避免中文变 □ -->
<link rel="preconnect" href="https://fonts.googleapis.com"/>
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin/>
<link href="https://fonts.googleapis.com/css2?family=Noto+Sans+SC:wght@400;600;700&display=swap" rel="stylesheet"/>
<style>
  * {{ box-sizing: border-box; }}
  html {{ background: #1a1a1a; }}
  body {{
    margin: 0;
    padding: 56px 52px 72px;
    width: 1080px;
    background: linear-gradient(180deg, #fffef7 0%, #fff9f0 100%);
    color: #1c1c1c;
    font-family: "Noto Sans SC", "PingFang SC", "Hiragino Sans GB", "Microsoft YaHei",
      "Source Han Sans SC", "WenQuanYi Micro Hei", "Noto Sans CJK SC", sans-serif;
    font-size: 28px;
    line-height: 1.65;
    word-wrap: break-word;
    overflow-wrap: break-word;
  }}
  h1 {{
    font-size: 40px;
    font-weight: 700;
    margin: 0 0 28px;
    line-height: 1.35;
    color: #0d0d0d;
  }}
  h2 {{
    font-size: 34px;
    font-weight: 700;
    margin: 36px 0 20px;
    line-height: 1.35;
    color: #141414;
  }}
  h2:first-child {{ margin-top: 0; }}
  h3 {{
    font-size: 30px;
    font-weight: 600;
    margin: 28px 0 14px;
    line-height: 1.4;
    color: #222;
  }}
  p {{ margin: 14px 0; }}
  ul, ol {{ margin: 14px 0; padding-left: 1.2em; }}
  li {{ margin: 10px 0; }}
  strong {{ font-weight: 600; color: #000; }}
  hr {{
    border: none;
    border-top: 2px solid rgba(0,0,0,0.12);
    margin: 32px 0;
  }}
  blockquote {{
    margin: 16px 0;
    padding: 12px 20px;
    border-left: 4px solid #c9a227;
    background: rgba(201, 162, 39, 0.08);
    color: #333;
  }}
  table {{
    width: 100%;
    border-collapse: collapse;
    margin: 20px 0;
    font-size: 24px;
    line-height: 1.5;
  }}
  th, td {{
    border: 1px solid rgba(0,0,0,0.15);
    padding: 14px 12px;
    vertical-align: top;
  }}
  th {{ background: rgba(0,0,0,0.06); font-weight: 600; }}
  pre {{
    margin: 18px 0;
    padding: 22px 20px;
    background: #f4f1ea;
    border: 1px solid rgba(0,0,0,0.1);
    border-radius: 8px;
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
  }}
  code {{
    font-family: "JetBrains Mono", "SF Mono", "Consolas", "Menlo", "Noto Sans SC", monospace;
    font-size: 21px;
    line-height: 1.5;
  }}
  pre code {{
    font-size: 21px;
    white-space: pre;
    display: block;
    overflow-x: auto;
  }}
  p code, li code, td code {{
    font-size: 24px;
    background: rgba(0,0,0,0.06);
    padding: 2px 8px;
    border-radius: 4px;
  }}
</style>
</head>
<body>
{body}
</body>
</html>
"""


def md_to_html_fragment(md: str) -> str:
    return markdown.markdown(
        md,
        extensions=[
            "markdown.extensions.tables",
            "markdown.extensions.fenced_code",
            "markdown.extensions.nl2br",
        ],
    )


def render_pngs(
    md_path: Path,
    out_dir: Path,
    line_slices: list[tuple[int, int]],
) -> None:
    lines = md_path.read_text(encoding="utf-8").splitlines()
    out_dir.mkdir(parents=True, exist_ok=True)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page(viewport={"width": 1080, "height": 800})
        for i, (a, b) in enumerate(line_slices, start=1):
            # 1-based inclusive line numbers
            chunk = "\n".join(lines[a - 1 : b])
            body = md_to_html_fragment(chunk)
            full_html = HTML_TEMPLATE.format(body=body)
            # 拉取 Noto Sans SC 并等字体就绪后再截图，否则中文多为 □
            page.set_content(full_html, wait_until="networkidle", timeout=120_000)
            page.evaluate("() => document.fonts.ready")
            png_path = out_dir / f"{i:02d}.png"
            page.locator("body").screenshot(path=str(png_path), type="png")
            print(png_path, flush=True)
        browser.close()


def default_slices_内存池() -> list[tuple[int, int]]:
    """Line ranges (1-based inclusive) for 如何设计一个内存池.md — code block kept whole."""
    return [
        (1, 12),
        (14, 38),
        (40, 54),
        (56, 69),
        (70, 125),
        (127, 160),
        (161, 177),
        (178, 198),
        (199, 233),
        (234, 242),
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("markdown", type=Path, help="Source .md file")
    parser.add_argument(
        "-o",
        "--out",
        type=Path,
        required=True,
        help="Output directory for PNGs",
    )
    parser.add_argument(
        "--preset",
        choices=["内存池"],
        default=None,
        help="Use built-in line slices for a known article",
    )
    args = parser.parse_args()

    if not args.markdown.is_file():
        print(f"Not found: {args.markdown}", file=sys.stderr)
        sys.exit(1)

    if args.preset == "内存池":
        slices = default_slices_内存池()
    else:
        print("Specify --preset 内存池 or extend script for custom slices.", file=sys.stderr)
        sys.exit(1)

    render_pngs(args.markdown, args.out, slices)


if __name__ == "__main__":
    main()
