#!/usr/bin/env python3
"""在 drafts 下 Markdown 的一级标题后插入作者与「更新时间」（取文件创建时间）。"""

from __future__ import annotations

import argparse
import platform
import re
import subprocess
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DRAFTS = ROOT / "drafts"
AUTHOR = "岭南过客"
MARKER = f"**作者：**{AUTHOR}"


def _birth_timestamp(path: Path) -> float:
    """优先系统报告的创建时间；不可用时回退到 st_birthtime / mtime。"""
    p = str(path.resolve())
    try:
        if platform.system() == "Darwin":
            out = subprocess.check_output(["stat", "-f", "%B", p], text=True).strip()
        else:
            out = subprocess.check_output(["stat", "-c", "%W", p], text=True).strip()
        w = int(out)
        if w > 0:
            return float(w)
    except (subprocess.CalledProcessError, ValueError, FileNotFoundError):
        pass
    st = path.stat()
    bt = getattr(st, "st_birthtime", None)
    if bt is not None and bt > 0:
        return float(bt)
    return float(st.st_mtime)


def _format_date(ts: float) -> str:
    return datetime.fromtimestamp(ts).strftime("%Y-%m-%d")


def _has_metadata_block(text: str) -> bool:
    return MARKER in text


def _strip_metadata_block(text: str) -> str:
    return re.sub(
        r"\n\n> \*\*作者：\*\*\s*岭南过客\s*\n> \*\*更新时间：\*\*\s*[^\n]+\n(?:\n)?",
        "\n",
        text,
        count=1,
    )


def _insert_after_first_heading(text: str, block: str) -> str:
    # 优先一级标题；若无（个别文以 ## 作文档标题），则用第一个 ##
    m = re.search(r"(?m)^\s*#\s+.+\s*$", text)
    if not m:
        m = re.search(r"(?m)^\s*##\s+.+\s*$", text)
    if not m:
        raise ValueError("未找到标题（# 或 ## …）")
    end = m.end()
    tail = text[end:].lstrip("\n")
    # 引用块后必须有空行，否则下一段会被算进 blockquote
    return text[:end] + "\n\n" + block + "\n\n" + tail


def process_file(path: Path, *, force: bool) -> bool:
    raw = path.read_text(encoding="utf-8")
    if _has_metadata_block(raw):
        if not force:
            return False
        raw = _strip_metadata_block(raw)
        if MARKER in raw:
            raise SystemExit(f"无法安全移除旧元信息，请手动检查：{path}")

    ts = _birth_timestamp(path)
    date_s = _format_date(ts)
    block = (
        f"> **作者：**{AUTHOR}  \n"
        f"> **更新时间：**{date_s}"
    )
    new_text = _insert_after_first_heading(raw, block)
    path.write_text(new_text, encoding="utf-8")
    return True


def main() -> None:
    ap = argparse.ArgumentParser(description="为 drafts/*.md 标题下增加作者与更新时间（文件创建时间）。")
    ap.add_argument(
        "--force",
        action="store_true",
        help="已存在作者信息时仍重写（按当前文件创建时间更新日期）",
    )
    args = ap.parse_args()

    paths = sorted(DRAFTS.rglob("*.md"))
    if not paths:
        raise SystemExit(f"未找到 Markdown：{DRAFTS}")

    n = 0
    for p in paths:
        if process_file(p, force=args.force):
            print(f"ok: {p.relative_to(ROOT)}")
            n += 1
        else:
            print(f"skip（已有作者信息，使用 --force 可重写）: {p.relative_to(ROOT)}")

    print(f"完成：写入 {n} 个文件。")


if __name__ == "__main__":
    main()
