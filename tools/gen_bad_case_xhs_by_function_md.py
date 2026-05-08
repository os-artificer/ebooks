#!/usr/bin/env python3
"""
Emit drafts/cpp/bad-case-按函数切片.md from src/cpp/bad_case.cpp.

Each ## section is one catalog block: from the line `/* ---------- N. …`
through the line before the next such header (full verbatim excerpt within
the anonymous namespace). Append full `print_catalog` + closing namespace,
then full `main`.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SRC = REPO_ROOT / "src" / "cpp" / "bad_case.cpp"
OUT = REPO_ROOT / "drafts" / "cpp" / "bad-case-按函数切片.md"

CATALOG_HDR = re.compile(r"^/\* ---------- (\d+)\.\s*(.+?)\s*-+\s*$")

# Optional **说明** by catalog number (full slice already includes types / #if 0).
SECTION_NOTE: dict[int, str] = {
    23: "段末 `demo_signed_range_hint` 仅作控制台提示，引出下一节有符号溢出。",
    60: "前两段为推荐写法与说明；本节针对的误区是滥用 `return std::move(局部)` 妨碍 NRVO。",
    82: "注册信号处理函数本身合法；风险在上一条 handler 内使用非 async-signal-safe 接口。",
}

TAIL_NOTE: dict[str, str] = {
    "print_catalog": "打印 86 条误区短标题索引，与同页注释编号一致；含匿名 `namespace` 收尾，与原文一致。",
    "main": "可执行入口：调用索引输出与若干常规编译下的演示，不含条件编译屏蔽的 UB 示例。",
}


def _norm_catalog_title(title: str) -> str:
    return title.replace("不配對", "不匹配")


def _parse_catalog(lines: list[str]) -> list[tuple[int, int, str]]:
    """(line_index_0based, catalog_num, title)."""
    out: list[tuple[int, int, str]] = []
    for i, ln in enumerate(lines):
        m = CATALOG_HDR.match(ln.strip())
        if not m:
            continue
        num = int(m.group(1))
        title = _norm_catalog_title(m.group(2).strip().rstrip("-").strip())
        out.append((i, num, title))
    return out


def _find_line_startswith(
    lines: list[str], prefix: str, start: int = 0
) -> int:
    for i in range(start, len(lines)):
        if lines[i].startswith(prefix):
            return i
    raise ValueError(f"line not found startswith: {prefix!r}")


def _find_line_contains(lines: list[str], substr: str, start: int = 0) -> int:
    for i in range(start, len(lines)):
        if substr in lines[i]:
            return i
    raise ValueError(f"line not found containing: {substr!r}")


def extract_slices(lines: list[str]) -> list[tuple[str, int | None, str, str, str]]:
    """
    Returns list of (heading, catalog_num|None, title_line, note, body).
    title_line is catalog title for numbered entries, or function name for tail.
    """
    cat = _parse_catalog(lines)
    if len(cat) != 86:
        raise SystemExit(f"expected 86 catalog entries, got {len(cat)}")

    print_i = _find_line_startswith(lines, "void print_catalog(")
    main_i = _find_line_startswith(lines, "int main(")
    ns_close = _find_line_contains(lines, "}  // namespace", print_i)

    out: list[tuple[str, int | None, str, str, str]] = []
    for i, (s0, num, title) in enumerate(cat):
        if i + 1 < len(cat):
            e0 = cat[i + 1][0] - 1
        else:
            e0 = print_i - 1
        while e0 >= s0 and not lines[e0].strip():
            e0 -= 1
        body = "\n".join(lines[s0 : e0 + 1])
        note = SECTION_NOTE.get(num, "")
        out.append((f"#{num}：{title}", num, title, note, body))

    note_p = TAIL_NOTE["print_catalog"]
    tail_body = "\n".join(lines[print_i : ns_close + 1])
    out.append(("print_catalog", None, "print_catalog", note_p, tail_body))

    note_m = TAIL_NOTE["main"]
    main_body = "\n".join(lines[main_i:])
    out.append(("main", None, "main", note_m, main_body))

    return out


def emit_md(slices: list[tuple[str, int | None, str, str, str]]) -> str:
    chunks: list[str] = []
    chunks.append("# 86 条 C/C++ 错误范例：条目全文摘录")
    chunks.append("")
    chunks.append("> **作者：**岭南过客  ")
    chunks.append("> **更新时间：**2026-05-01")
    chunks.append("")
    chunks.append(
        "在 C/C++ 里，能通过编译并不等于避开了标准意义上的未定义行为。"
    )
    chunks.append("")
    chunks.append(
        "下文自反面案例汇总中原样抄录：按注释目录 `/* ---------- N.` 划段，"
        "每节 **连续收录该段落的全部行**（含讲解、后果、正确说明、`#if 0`、"
        "头文件、类型定义与同段内所有函数），不拆条、不删行。"
    )
    chunks.append("")
    chunks.append(
        "编号条目共 86 段；篇末另附 `print_catalog`、`main` 的完整片段。"
        "文中 **UB** 指未定义行为；**实现定义**、**未指定** 与标准及常用教材用法一致。"
    )
    chunks.append("")
    chunks.append("---")
    chunks.append("")

    for heading, num, _title, note, body in slices:
        if num is not None:
            chunks.append(f"## {heading}")
            chunks.append("")
        else:
            chunks.append(f"## `{heading}`")
            chunks.append("")
        if note:
            chunks.append(f"**说明：** {note}")
            chunks.append("")

        fixed = body.replace("\t", "    ")
        chunks.append("```cpp")
        chunks.append(fixed)
        chunks.append("```")
        chunks.append("")

    return "\n".join(chunks).rstrip() + "\n"


def main() -> None:
    lines = SRC.read_text(encoding="utf-8").splitlines()
    slices = extract_slices(lines)
    md = emit_md(slices)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(md, encoding="utf-8")
    print(f"Wrote {OUT} ({len(slices)} sections)", flush=True)


if __name__ == "__main__":
    main()
