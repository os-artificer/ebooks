from __future__ import annotations

import argparse
import json
import re
import shutil
from pathlib import Path

from markdown import markdown as md_to_html

# 本脚本在 tools/ 下，仓库根为上一级目录
ROOT = Path(__file__).resolve().parent.parent
DRAFTS = ROOT / "drafts"
NAV_PATH = ROOT / "web" / "articles" / "nav.json"
# 与 publish_all 中一致：仅这些分类参与导航同步
NAV_SYNC_KEYS = frozenset(
    {"cpp", "golang", "stl", "linux", "libc-gcc", "tech-arch"}
)


def guess_title(md_text: str, fallback: str) -> str:
    # Prefer the first top-level heading.
    m = re.search(r"(?m)^\s*#\s+(.+?)\s*$", md_text)
    if m:
        return m.group(1).strip()
    m = re.search(r"(?m)^\s*##\s+(.+?)\s*$", md_text)
    if m:
        return m.group(1).strip()
    return fallback


def convert_one(md_path: Path, out_html_path: Path) -> None:
    md_text = md_path.read_text(encoding="utf-8")
    title = guess_title(md_text, md_path.stem)

    # Avoid duplicate title:
    # - Our HTML wrapper adds an <h1> with `title`
    # - Markdown also typically converts the first heading line to <h1>/<h2>
    # So we remove the first heading line from the markdown before conversion.
    md_text = re.sub(r"(?m)^(#{1,6})\s+.+\s*$\n?", "", md_text, count=1)

    # extensions:
    # - fenced_code: ``` code blocks
    # - tables: |a|b| table
    # - toc: allow [toc] patterns if present
    html_body = md_to_html(
        md_text,
        extensions=["fenced_code", "tables"],
    )
    # 草稿在 drafts/<分类>/ 下用 ../images/...；发布到 web/page/<分类>/ 需多一层 ../
    html_body = html_body.replace('src="../images/', 'src="../../images/')

    out_html_path.parent.mkdir(parents=True, exist_ok=True)
    page = "\n".join(
        [
            "<!DOCTYPE html>",
            "<html>",
            "<head>",
            '    <meta charset="UTF-8">',
            '    <meta name="viewport" content="width=device-width, initial-scale=1.0">',
            f"    <title>{title}</title>",
            "    <style>",
            "      body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Helvetica,Arial,sans-serif;line-height:1.7;margin:24px;}",
            "      pre{background:#0b1020;color:#e6edf3;padding:12px;border-radius:10px;overflow:auto;}",
            "      code{font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,Liberation Mono,monospace;}",
            "      a{color:#2563eb;text-decoration:none;}",
            "      a:hover{text-decoration:underline;}",
            "      img{max-width:100%;height:auto;}",
            "      table{border-collapse:collapse;}",
            "      th,td{border:1px solid #d1d5db;padding:6px 10px;}",
            "      th{background:#f9fafb;}",
            "      article>blockquote:first-of-type{margin:0.5rem 0 1rem;padding:0;border-left:none;color:#4b5563;font-size:0.92em;}",
            "      article>blockquote:first-of-type p{margin:0.2em 0;line-height:1.5;}",
            "    </style>",
            "</head>",
            "<body>",
            f"    <article>",
            f"      <h1 style=\"margin-top:0\">{title}</h1>",
            f"      {html_body}",
            f"    </article>",
            "</body>",
            "</html>",
            "",
        ]
    )
    out_html_path.write_text(page, encoding="utf-8")


def _resolve_md_arg(raw: str) -> Path:
    p = Path(raw).expanduser()
    if not p.is_absolute():
        cwd_try = Path.cwd() / p
        if cwd_try.exists():
            p = cwd_try.resolve()
        else:
            p = (ROOT / p).resolve()
    else:
        p = p.resolve()
    if not p.exists():
        raise SystemExit(f"文件不存在: {raw}")
    if p.suffix.lower() != ".md":
        raise SystemExit(f"不是 Markdown 文件（.md）: {p}")
    return p


def _draft_category(md_path: Path) -> str:
    try:
        rel = md_path.resolve().relative_to(DRAFTS)
    except ValueError:
        raise SystemExit(
            f"必须在 drafts 下: {md_path}\n"
            f"（期望前缀: {DRAFTS}/）"
        )
    if len(rel.parts) < 2:
        raise SystemExit(f"路径无效（需要 drafts/<分类>/<文章>.md）: {md_path}")
    return rel.parts[0]


def sync_nav_full() -> None:
    """根据 drafts/<分类>/*.md 重建 nav.json 中各分类的 items（顺序为文件名排序）。"""
    if not NAV_PATH.is_file():
        raise SystemExit(f"未找到导航文件: {NAV_PATH}")
    data = json.loads(NAV_PATH.read_text(encoding="utf-8"))
    categories = data.get("categories")
    if not isinstance(categories, list):
        raise SystemExit("nav.json 格式异常: 缺少 categories 数组")

    for cat in categories:
        key = cat.get("key")
        if key not in NAV_SYNC_KEYS:
            continue
        in_dir = DRAFTS / key
        if not in_dir.is_dir():
            cat["items"] = []
            continue
        md_files = sorted(in_dir.glob("*.md"))
        items = []
        for md_path in md_files:
            md_text = md_path.read_text(encoding="utf-8")
            title = guess_title(md_text, md_path.stem)
            href = f"./page/{key}/{md_path.stem}.html"
            items.append({"href": href, "text": title})
        cat["items"] = items

    NAV_PATH.write_text(
        json.dumps(data, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )


def copy_tech_arch_assets_if_needed(categories: set[str]) -> None:
    if "tech-arch" not in categories:
        return
    assets_in = DRAFTS / "images" / "tech-arch"
    assets_out = ROOT / "web" / "images" / "tech-arch"
    if not assets_in.exists():
        return
    assets_out.parent.mkdir(parents=True, exist_ok=True)
    if assets_out.exists():
        shutil.rmtree(assets_out)
    shutil.copytree(assets_in, assets_out)


def publish_all() -> None:
    categories = ["cpp", "golang", "stl", "linux", "libc-gcc", "tech-arch"]
    for cat in categories:
        in_dir = DRAFTS / cat
        out_dir = ROOT / "web" / "page" / cat
        out_dir.mkdir(parents=True, exist_ok=True)

        md_files = sorted(in_dir.glob("*.md"))
        for md_path in md_files:
            out_html_path = out_dir / f"{md_path.stem}.html"
            convert_one(md_path, out_html_path)

        if cat == "tech-arch":
            assets_in = DRAFTS / "images" / "tech-arch"
            assets_out = ROOT / "web" / "images" / "tech-arch"
            if assets_in.exists():
                assets_out.parent.mkdir(parents=True, exist_ok=True)
                if assets_out.exists():
                    shutil.rmtree(assets_out)
                shutil.copytree(assets_in, assets_out)

    sync_nav_full()


def publish_paths(md_paths: list[Path]) -> None:
    categories: set[str] = set()
    for md_path in md_paths:
        cat = _draft_category(md_path)
        categories.add(cat)
        out_dir = ROOT / "web" / "page" / cat
        out_html_path = out_dir / f"{md_path.stem}.html"
        convert_one(md_path, out_html_path)
    copy_tech_arch_assets_if_needed(categories)
    sync_nav_full()


def main() -> None:
    parser = argparse.ArgumentParser(
        description="将 drafts 下的 Markdown 发布为 web/page 下的 HTML。"
    )
    parser.add_argument(
        "articles",
        nargs="*",
        metavar="MD",
        help="可选：一篇或多篇 .md 路径（相对仓库根、当前目录或绝对路径）。"
        " 省略则发布全部分类下的所有文章。",
    )
    args = parser.parse_args()

    if args.articles:
        paths = [_resolve_md_arg(a) for a in args.articles]
        publish_paths(paths)
    else:
        publish_all()


if __name__ == "__main__":
    main()
