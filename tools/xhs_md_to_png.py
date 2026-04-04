#!/usr/bin/env python3
"""
Render markdown slices to PNG images for 小红书-style vertical notes.
Content is preserved verbatim per slice (line ranges from source file).
Batch mode: drafts/cpp and drafts/golang → repo/xhs/<lang>/<article>/.
Also writes 00-cover.png (graphic poster: title, heading-derived keywords, optional one-line hook).
"""
from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path

import markdown
from playwright.sync_api import sync_playwright

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_XHS_ROOT = REPO_ROOT / "xhs"
DRAFT_CPP = REPO_ROOT / "drafts" / "cpp"
DRAFT_GOLANG = REPO_ROOT / "drafts" / "golang"

# Offline-safe rendering: cache TTFs here; @font-face uses file:// absolute URLs (Playwright has no set_content base_url).
XHS_FONT_CACHE = REPO_ROOT / "tools" / "fonts" / "xhs_cache"
# (dest_filename, gstatic URL) — Noto Sans SC + JetBrains Mono weights used by poster & body HTML.
_XHS_FONT_DOWNLOADS: list[tuple[str, str]] = [
    (
        "NotoSansSC-400.ttf",
        "https://fonts.gstatic.com/s/notosanssc/v40/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaG9_FnYw.ttf",
    ),
    (
        "NotoSansSC-600.ttf",
        "https://fonts.gstatic.com/s/notosanssc/v40/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaGwHCnYw.ttf",
    ),
    (
        "NotoSansSC-700.ttf",
        "https://fonts.gstatic.com/s/notosanssc/v40/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaGzjCnYw.ttf",
    ),
    (
        "JetBrainsMono-400.ttf",
        "https://fonts.gstatic.com/s/jetbrainsmono/v24/tDbY2o-flEEny0FZhsfKu5WU4zr3E_BX0PnT8RD8yKxjPQ.ttf",
    ),
    (
        "JetBrainsMono-600.ttf",
        "https://fonts.gstatic.com/s/jetbrainsmono/v24/tDbY2o-flEEny0FZhsfKu5WU4zr3E_BX0PnT8RD8FqtjPQ.ttf",
    ),
]


def _xhs_download_file(url: str, dest: Path) -> None:
    req = urllib.request.Request(
        url,
        headers={"User-Agent": "Mozilla/5.0 (compatible; ebooks-xhs-md-to-png/1.0)"},
    )
    with urllib.request.urlopen(req, timeout=180) as resp:
        dest.write_bytes(resp.read())


_SYSTEM_FONT_DIR = Path("/usr/share/fonts/truetype/xhs-cache")


def ensure_xhs_font_cache() -> Path:
    """Download fonts into tools/fonts/xhs_cache and install into system font dir for Chromium."""
    XHS_FONT_CACHE.mkdir(parents=True, exist_ok=True)
    need_fc_cache = False
    for name, url in _XHS_FONT_DOWNLOADS:
        dest = XHS_FONT_CACHE / name
        if not (dest.is_file() and dest.stat().st_size > 4096):
            tmp = dest.with_suffix(dest.suffix + ".part")
            try:
                _xhs_download_file(url, tmp)
                tmp.replace(dest)
            except (urllib.error.URLError, OSError) as e:
                if tmp.is_file():
                    tmp.unlink(missing_ok=True)
                print(
                    f"xhs_md_to_png: failed to download font {name!r} ({e}). "
                    "Check network or place the file manually under tools/fonts/xhs_cache/.",
                    file=sys.stderr,
                )
                continue
        sys_dest = _SYSTEM_FONT_DIR / name
        if not (sys_dest.is_file() and sys_dest.stat().st_size == dest.stat().st_size):
            _SYSTEM_FONT_DIR.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dest, sys_dest)
            need_fc_cache = True
    if need_fc_cache and shutil.which("fc-cache"):
        subprocess.run(["fc-cache", "-f", str(_SYSTEM_FONT_DIR)], check=False)
    return XHS_FONT_CACHE


def xhs_local_font_faces_style() -> str:
    """Empty tag: fonts are now system-installed; CSS font-family fallback handles loading."""
    ensure_xhs_font_cache()
    return ""

# 1080px width is common for XHS; background and typography tuned for readability.
HTML_TEMPLATE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
__XHS_LOCAL_FONTS__
<style>
  * {{ box-sizing: border-box; }}
  html {{ background: #1a1a1a; }}
  body {{
    margin: 0;
    padding: 56px 52px 72px;
    width: 1080px;
    min-height: 100%;
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
    overflow-wrap: normal;
    word-break: normal;
  }}
  code {{
    font-family: "JetBrains Mono", "SF Mono", "Consolas", "Menlo", "Noto Sans SC", monospace;
    font-size: 21px;
    line-height: 1.5;
  }}
  pre code {{
    font-size: 21px;
    white-space: pre;
    tab-size: 4;
    -moz-tab-size: 4;
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

# Cover poster CSS: single `{` / `}` only — never mix with str.format (see skill doc).
POSTER_CSS = """
  * { box-sizing: border-box; }
  html, body { margin: 0; padding: 0; background: #0e0c0a; }
  .wrap {
    width: 1080px;
    height: 1440px;
    position: relative;
    overflow: hidden;
    padding: 72px 56px 80px;
    background: #0e0c0a;
    color: #faf6ee;
    font-family: "Noto Sans SC", "Noto Sans CJK SC", "PingFang SC", "Hiragino Sans GB",
      "Microsoft YaHei", "Source Han Sans SC", "WenQuanYi Micro Hei", sans-serif;
  }
  .bg-grid {
    position: absolute;
    inset: 0;
    background-image:
      linear-gradient(rgba(201,162,39,0.07) 1px, transparent 1px),
      linear-gradient(90deg, rgba(201,162,39,0.07) 1px, transparent 1px);
    background-size: 48px 48px;
    mask-image: radial-gradient(ellipse 80% 70% at 30% 25%, black 0%, transparent 70%);
    pointer-events: none;
  }
  .orb {
    position: absolute;
    border-radius: 50%;
    filter: blur(60px);
    opacity: 0.45;
    pointer-events: none;
  }
  .orb1 { width: 420px; height: 420px; background: #3d2a12; top: -80px; right: -100px; }
  .orb2 { width: 360px; height: 360px; background: #1a3d2e; bottom: 120px; left: -80px; }
  .deco-code {
    position: absolute;
    top: 100px;
    left: 56px;
    width: 420px;
    font-family: "JetBrains Mono", "Noto Sans CJK SC", "Noto Sans SC", monospace;
    font-size: 18px;
    line-height: 1.65;
    color: rgba(180, 170, 150, 0.35);
    white-space: pre;
    overflow-wrap: normal;
    word-break: normal;
    pointer-events: none;
    user-select: none;
  }
  .content {
    position: relative;
    z-index: 1;
    margin-top: 320px;
  }
  .badge {
    display: inline-block;
    font-size: 20px;
    font-weight: 600;
    letter-spacing: 0.14em;
    color: #c9a227;
    border: 1px solid rgba(201, 162, 39, 0.5);
    padding: 8px 20px;
    border-radius: 2px;
    margin-bottom: 28px;
  }
  h1 {
    font-size: 48px;
    font-weight: 700;
    line-height: 1.32;
    margin: 0 0 36px;
    color: #fffef8;
    max-width: 920px;
    text-shadow: 0 2px 32px rgba(0,0,0,0.5);
  }
  .chips {
    display: flex;
    flex-wrap: wrap;
    gap: 16px 20px;
    max-width: 960px;
  }
  .chips span {
    font-size: 24px;
    font-weight: 500;
    color: rgba(250,246,238,0.92);
    background: rgba(255,255,255,0.06);
    border: 1px solid rgba(255,255,255,0.12);
    padding: 12px 22px;
    border-radius: 999px;
  }
  .hook {
    margin-top: 48px;
    font-size: 26px;
    line-height: 1.55;
    color: rgba(250,246,238,0.55);
    max-width: 880px;
  }
  .hook:empty { display: none; }
"""

POSTER_DOCUMENT_HEAD = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
__XHS_LOCAL_FONTS__
<style>
"""

POSTER_DOCUMENT_TAIL = """
</style>
</head>
<body>
<div class="wrap">
  <div class="bg-grid"></div>
  <div class="orb orb1"></div>
  <div class="orb orb2"></div>
  <div class="deco-code">__POSTER_DECO__</div>
  <div class="content">
    <span class="badge">__POSTER_BADGE__</span>
    <h1>__POSTER_TITLE__</h1>
    <div class="chips">__POSTER_CHIPS__</div>
    <p class="hook">__POSTER_HOOK__</p>
  </div>
</div>
</body>
</html>
"""


def md_to_html_fragment(md: str) -> str:
    # Do not use nl2br: it can interfere with fenced code newlines / spacing.
    return markdown.markdown(
        md,
        extensions=[
            "markdown.extensions.tables",
            "markdown.extensions.fenced_code",
        ],
    )


def _escape_html(s: str) -> str:
    return (
        s.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
    )


def fence_depth_after(lines: list[str], k: int) -> int:
    """After processing the first k lines (0 <= k <= n), unclosed ``` fence parity (0 = balanced)."""
    if k <= 0:
        return 0
    d = 0
    for i in range(k):
        if lines[i].lstrip().startswith("```"):
            d ^= 1
    return d


def is_root_level_after_line(lines: list[str], line_1based: int) -> bool:
    """True if all ``` fences are closed after line line_1based (inclusive, 1-based index)."""
    return fence_depth_after(lines, line_1based) == 0


def _clean_heading_phrase(raw: str, max_len: int = 20) -> str:
    t = re.sub(r"^#+\s*", "", raw).strip()
    t = re.sub(r"^\d+(\.\d+)*\s*", "", t)
    t = re.sub(r"\s+", " ", t)
    if len(t) > max_len:
        return t[: max_len - 1] + "…"
    return t


def extract_poster_keywords(lines: list[str], limit: int = 6) -> list[str]:
    """Short phrases from ## / ### only (article-derived, no new prose)."""
    seen: set[str] = set()
    out: list[str] = []
    for line in lines:
        s = line.strip()
        if not (re.match(r"^##(?!#)\s+\S", s) or re.match(r"^###(?!#)\s+\S", s)):
            continue
        phrase = _clean_heading_phrase(s, max_len=22)
        if phrase and phrase not in seen:
            seen.add(phrase)
            out.append(phrase)
        if len(out) >= limit:
            break
    return out


def extract_poster_hook(lines: list[str], max_chars: int = 44) -> str:
    """One short verbatim line from the first normal paragraph (not headings/lists/code)."""
    n = len(lines)
    i = 0
    while i < n:
        s = lines[i].strip()
        if s.startswith("#"):
            i += 1
            continue
        break
    while i < n:
        s = lines[i].strip()
        if not s or s.startswith(">") or s in ("---", "***", "___"):
            i += 1
            continue
        if s.startswith("#"):
            i += 1
            continue
        if s.startswith("```"):
            while i < n and not lines[i].strip().startswith("```"):
                i += 1
            if i < n:
                i += 1
            continue
        if s.startswith(("- ", "* ", "1.", "2.", "3.")):
            i += 1
            continue
        plain = re.sub(r"\*\*([^*]+)\*\*", r"\1", s)
        plain = re.sub(r"`([^`]+)`", r"\1", plain)
        plain = re.sub(r"\s+", " ", plain).strip()
        if not plain:
            i += 1
            continue
        if len(plain) > max_chars:
            return plain[: max_chars - 1] + "…"
        return plain
    return ""


def deco_lines_for_lang(lang_label: str) -> str:
    """Decorative pseudo-code block (not article text) for 图文 feel."""
    if lang_label == "Go":
        return (
            "package main\n\n"
            "import (\n"
            '    "context"\n'
            ")\n\n"
            "// supervisor <-> worker\n"
            "func run(ctx context.Context) {\n"
            "    select {\n"
            "    case <-ctx.Done():\n"
            "        return\n"
            "    }\n"
            "}"
        )
    if lang_label == "C++":
        return (
            "#include <thread>\n\n"
            "class Pool {\n"
            "public:\n"
            "    void submit(Task t);\n"
            "private:\n"
            "    std::mutex mu_;\n"
            "};"
        )
    return (
        "fn main() {\n"
        "    // …\n"
        "}"
    )


def build_poster_html(title: str, badge: str, keywords: list[str], hook: str, lang_label: str) -> str:
    chips_html = "".join(f"<span>{_escape_html(k)}</span>" for k in keywords[:8])
    if not chips_html:
        chips_html = f"<span>{_escape_html('要点速览')}</span>"
    hook_html = _escape_html(hook) if hook else ""
    deco = _escape_html(deco_lines_for_lang(lang_label))
    # Assemble with __POSTER_*__ tokens — CSS is plain `{` `}` in POSTER_CSS (no str.format).
    html = (
        POSTER_DOCUMENT_HEAD.replace("__XHS_LOCAL_FONTS__", xhs_local_font_faces_style())
        + POSTER_CSS
        + POSTER_DOCUMENT_TAIL
    )
    return (
        html.replace("__POSTER_DECO__", deco)
        .replace("__POSTER_BADGE__", _escape_html(badge))
        .replace("__POSTER_TITLE__", _escape_html(title))
        .replace("__POSTER_CHIPS__", chips_html)
        .replace("__POSTER_HOOK__", hook_html)
    )


def extract_poster_title(lines: list[str]) -> str:
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("#"):
            return stripped.lstrip("#").strip() or "未命名"
    return "未命名"


def render_cover_png(
    page,
    out_path: Path,
    title: str,
    badge: str,
    keywords: list[str],
    hook: str,
    lang_label: str,
) -> None:
    html = build_poster_html(title, badge, keywords, hook, lang_label)
    page.set_viewport_size({"width": 1080, "height": 1440})
    page.set_content(html, wait_until="load", timeout=120_000)
    page.evaluate("() => document.fonts.ready")
    # Avoid document.fonts.load() here: local file:// @font-face can throw NetworkError in headless.
    page.wait_for_timeout(900)
    page.locator(".wrap").screenshot(path=str(out_path), type="png")


def render_pngs(
    md_path: Path,
    out_dir: Path,
    line_slices: list[tuple[int, int]],
    lang_label: str,
    write_cover: bool,
) -> None:
    lines = md_path.read_text(encoding="utf-8").splitlines()
    out_dir.mkdir(parents=True, exist_ok=True)

    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page(viewport={"width": 1080, "height": 600})
        font_tag = xhs_local_font_faces_style()

        for i, (a, b) in enumerate(line_slices, start=1):
            chunk = "\n".join(lines[a - 1 : b])
            body = md_to_html_fragment(chunk)
            full_html = HTML_TEMPLATE.format(body=body).replace(
                "__XHS_LOCAL_FONTS__", font_tag
            )
            page.set_content(full_html, wait_until="load", timeout=120_000)
            page.evaluate("() => document.fonts.ready")
            h = page.evaluate(
                "() => Math.max(400, Math.ceil(document.documentElement.scrollHeight))"
            )
            page.set_viewport_size({"width": 1080, "height": min(h, 32_000)})
            png_path = out_dir / f"{i:02d}.png"
            page.locator("body").screenshot(path=str(png_path), type="png")
            print(png_path, flush=True)

        if write_cover:
            title = extract_poster_title(lines)
            keywords = extract_poster_keywords(lines)
            hook = extract_poster_hook(lines)
            cover_path = out_dir / "00-cover.png"
            render_cover_png(
                page,
                cover_path,
                title,
                lang_label,
                keywords,
                hook,
                lang_label,
            )
            print(cover_path, flush=True)

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


def auto_slices(lines: list[str], max_lines: int = 55, min_lines: int = 24) -> list[tuple[int, int]]:
    """
    Build 1-based inclusive line slices automatically.
    Heuristics:
    - Prefer splitting near headings / hr / blank lines / ``` lines.
    - Never split so that a slice starts or ends with unbalanced ``` fences (blank lines
      inside code blocks are NOT safe boundaries).
    - Avoid splitting inside fenced code blocks (defer split until fence closes).
    - Keep each slice roughly <= max_lines where possible.
    """
    n = len(lines)
    if n == 0:
        return []

    slices: list[tuple[int, int]] = []
    start = 1
    i = 1
    in_fence = False

    def is_boundary_heuristic(idx: int) -> bool:
        if idx < 1 or idx > n:
            return False
        s = lines[idx - 1].strip()
        if s.startswith("```"):
            return True
        if not s:
            return True
        if s.startswith("#"):
            return True
        if s in ("---", "***", "___"):
            return True
        return False

    def pick_split_end(i_cursor: int) -> int:
        """Choose end line for slice [start, split_at]; i_cursor is first line past max length."""
        high = i_cursor
        for lo_floor in (start + min_lines - 1, start):
            for j in range(high, lo_floor - 1, -1):
                if is_boundary_heuristic(j) and is_root_level_after_line(lines, j):
                    return j
        if is_root_level_after_line(lines, i_cursor):
            return i_cursor
        for j in range(i_cursor, start - 1, -1):
            if is_root_level_after_line(lines, j):
                return j
        return i_cursor

    while i <= n:
        s = lines[i - 1].lstrip()
        if s.startswith("```"):
            in_fence = not in_fence

        cur_len = i - start + 1
        need_split = cur_len >= max_lines
        if need_split and not in_fence:
            split_at = pick_split_end(i)
            if split_at < start:
                split_at = i
            slices.append((start, split_at))
            start = split_at + 1
            i = start
            in_fence = fence_depth_after(lines, start - 1) == 1
            continue

        i += 1

    if start <= n:
        slices.append((start, n))
    return slices


def lang_label_for_path(md_path: Path) -> str:
    try:
        rel = md_path.resolve().relative_to(REPO_ROOT)
        parts = rel.parts
        if len(parts) >= 2 and parts[0] == "drafts":
            if parts[1] == "cpp":
                return "C++"
            if parts[1] == "golang":
                return "Go"
    except ValueError:
        pass
    return "技术笔记"


def collect_batch_sources() -> list[tuple[Path, str]]:
    """Pairs of (md_path, lang_folder_name) for cpp and golang drafts."""
    out: list[tuple[Path, str]] = []
    for folder, name in ((DRAFT_CPP, "cpp"), (DRAFT_GOLANG, "golang")):
        if not folder.is_dir():
            continue
        for p in sorted(folder.glob("*.md")):
            if p.is_file():
                out.append((p, name))
    return out


def article_out_dir(xhs_root: Path, lang_folder: str, md_path: Path) -> Path:
    return xhs_root / lang_folder / md_path.stem


def run_one(
    md_path: Path,
    out_dir: Path,
    slices: list[tuple[int, int]],
    lang_label: str,
    write_cover: bool,
) -> None:
    render_pngs(md_path, out_dir, slices, lang_label=lang_label, write_cover=write_cover)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Markdown → 小红书竖图切片（原文排版，代码块不截断）+ 封面海报"
    )
    parser.add_argument(
        "markdown",
        type=Path,
        nargs="?",
        default=None,
        help="Source .md file (omit with --batch)",
    )
    parser.add_argument(
        "-o",
        "--out",
        type=Path,
        default=None,
        help="Output directory for PNGs (single-file mode)",
    )
    parser.add_argument(
        "--out-root",
        type=Path,
        default=DEFAULT_XHS_ROOT,
        help=f"Root for --batch output (default: {DEFAULT_XHS_ROOT})",
    )
    parser.add_argument(
        "--batch",
        action="store_true",
        help="Process all drafts/cpp and drafts/golang *.md into <out-root>/<lang>/<stem>/",
    )
    parser.add_argument(
        "--no-cover",
        action="store_true",
        help="Skip 00-cover.png poster",
    )
    parser.add_argument(
        "--preset",
        choices=["内存池"],
        default=None,
        help="Use built-in line slices for a known article",
    )
    parser.add_argument(
        "--auto",
        action="store_true",
        help="Auto-generate line slices for any markdown file",
    )
    parser.add_argument(
        "--max-lines",
        type=int,
        default=55,
        help="Approx max lines per image in auto mode (default: 55)",
    )
    args = parser.parse_args()

    write_cover = not args.no_cover

    if args.batch:
        sources = collect_batch_sources()
        if not sources:
            print("No .md files under drafts/cpp or drafts/golang.", file=sys.stderr)
            sys.exit(1)
        for md_path, lang_folder in sources:
            out_dir = article_out_dir(args.out_root, lang_folder, md_path)
            lines = md_path.read_text(encoding="utf-8").splitlines()
            slices = auto_slices(lines, max_lines=max(20, args.max_lines))
            label = lang_label_for_path(md_path)
            print(f"\n== {md_path} -> {out_dir}", flush=True)
            run_one(md_path, out_dir, slices, lang_label=label, write_cover=write_cover)
        return

    if args.markdown is None or not args.markdown.is_file():
        print("Provide a markdown file or use --batch.", file=sys.stderr)
        sys.exit(1)

    if args.out is None:
        print("Single-file mode requires -o/--out (or use --batch).", file=sys.stderr)
        sys.exit(1)

    if args.preset == "内存池":
        slices = default_slices_内存池()
    elif args.auto:
        lines = args.markdown.read_text(encoding="utf-8").splitlines()
        slices = auto_slices(lines, max_lines=max(20, args.max_lines))
    else:
        print("Specify --preset 内存池 or use --auto for generic slicing.", file=sys.stderr)
        sys.exit(1)

    run_one(
        args.markdown,
        args.out,
        slices,
        lang_label=lang_label_for_path(args.markdown),
        write_cover=write_cover,
    )


if __name__ == "__main__":
    main()
