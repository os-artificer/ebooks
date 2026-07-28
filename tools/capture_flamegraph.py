#!/usr/bin/env python3
"""抓取 go tool pprof 的 Flame Graph 并渲染为 PNG，嵌进文章。

原理（不依赖浏览器点 UI，最稳）:
    pprof 的 Web UI 内置 Flame Graph 视图，其后端端点 /flamegraph
    返回一个"内嵌 <svg> 火焰图的完整 HTML 页面"。我们:
      1. 起 `go tool pprof -http=:PORT cpu.out`
      2. curl -L 抓 /flamegraph 页面（含真实 SVG）
      3. 用正则提取 <svg>...</svg>
      4. 包一层最小 HTML（白底、居中）交给无头 Chromium 截图成 PNG

依赖:
    - Graphviz(dot) 可选（flamegraph 本身不需要，调用图视图才需要）
    - Playwright + Chromium
"""
import subprocess
import sys
import time
import signal
import os
import re

WORKSPACE = "/Users/ylgeeker/.workbuddy/binaries/node/workspace"
NODE = "/Users/ylgeeker/.workbuddy/binaries/node/versions/22.22.2/bin/node"

RENDER_HTML = r"""
const { chromium } = require('playwright');
(async () => {
  const htmlPath = process.argv[2];
  const outPng = process.argv[3];
  const browser = await chromium.launch();
  const page = await browser.newPage({ viewport: { width: 900, height: 2600 }, deviceScaleFactor: 2 });
  await page.goto('file://' + htmlPath, { waitUntil: 'networkidle', timeout: 30000 });
  // 整页截图（火焰图是纵向长图）
  await page.screenshot({ path: outPng, fullPage: true });
  await browser.close();
  console.log('OK ' + outPng);
})().catch(e => { console.error('FAIL', e.message); process.exit(1); });
"""


def main():
    if len(sys.argv) < 4:
        print("usage: capture_flamegraph.py <cpu.out> <output.png> [port]")
        sys.exit(2)
    cpu_out = sys.argv[1]
    out_png = sys.argv[2]
    port = sys.argv[3] if len(sys.argv) > 3 else "8099"

    env = dict(os.environ)
    env["NODE_PATH"] = os.path.join(WORKSPACE, "node_modules")
    tmp_dir = "/tmp"
    page_html = os.path.join(tmp_dir, "flame_page.html")
    wrap_html = os.path.join(tmp_dir, "flame_wrap.html")
    render_js = os.path.join(WORKSPACE, "_flame_render_tmp.js")

    # 1) 起 pprof web 服务
    srv = subprocess.Popen(
        ["go", "tool", "pprof", "-http=:" + port, cpu_out],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    try:
        time.sleep(4)
        # 2) 抓 flamegraph 页面（跟随重定向）
        curl = subprocess.run(
            ["curl", "-s", "--noproxy", "localhost", "-L",
             f"http://localhost:{port}/flamegraph", "-o", page_html],
            env=env, timeout=30,
        )
        if curl.returncode != 0 or not os.path.exists(page_html):
            print("FAIL: curl /flamegraph failed")
            sys.exit(1)
        raw = open(page_html, encoding="utf-8", errors="ignore").read()
        m = re.search(r"<svg.*?</svg>", raw, re.S)
        if not m:
            print("FAIL: no <svg> in flamegraph page")
            sys.exit(1)
        svg = m.group(0)
        # 3) 包一层白底最小 HTML 便于截图
        wrap = (
            "<!doctype html><html><head><meta charset='utf-8'>"
            "<style>html,body{margin:0;background:#fff;}</style></head>"
            f"<body>{svg}</body></html>"
        )
        open(wrap_html, "w", encoding="utf-8").write(wrap)
        open(render_js, "w", encoding="utf-8").write(RENDER_HTML)
        # 4) Playwright 渲染成 PNG
        r = subprocess.run(
            [NODE, render_js, wrap_html, out_png],
            cwd=WORKSPACE, env=env, capture_output=True, text=True, timeout=90,
        )
        if r.returncode != 0:
            print("STDOUT:\n", r.stdout)
            print("STDERR:\n", r.stderr)
            sys.exit(1)
        print(r.stdout.strip())
    finally:
        srv.send_signal(signal.SIGTERM)
        try:
            srv.wait(timeout=5)
        except Exception:
            srv.kill()


if __name__ == "__main__":
    main()
