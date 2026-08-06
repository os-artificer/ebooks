#!/usr/bin/env python3
"""Remove bottom-right AI watermark from sketch images and save as JPEG (pure PIL)."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageStat


def remove_watermark(input_path: Path, output_path: Path, quality: int = 85) -> None:
    img = Image.open(input_path).convert("RGB")
    w, h = img.size

    # Watermark sits in a small bottom-right patch.
    patch_w, patch_h = 220, 60
    x0, y0 = w - patch_w, h - patch_h

    # Sample local paper color from the clean strip directly above the watermark.
    strip = img.crop((x0, y0 - 40, w, y0))
    stat = ImageStat.Stat(strip)
    paper = tuple(int(c) for c in stat.mean)

    # Create a feathered patch of that paper color.
    patch = Image.new("RGBA", (patch_w, patch_h), (*paper, 255))
    draw = ImageDraw.Draw(patch)
    # Fade top edge into transparency over 20 px.
    for i in range(20):
        alpha = int(255 * (i / 19.0))
        draw.line([(0, i), (patch_w, i)], fill=(*paper, alpha))

    patch = patch.filter(ImageFilter.GaussianBlur(radius=1.2))

    base = img.convert("RGBA")
    base.paste(patch, (x0, y0), patch)
    final = base.convert("RGB")
    final.save(output_path, "JPEG", quality=quality, optimize=True)
    print(f"Saved {output_path} ({output_path.stat().st_size / 1024:.1f} KB)")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: remove_watermark.py <input.png> <output.jpg>")
        sys.exit(1)
    remove_watermark(Path(sys.argv[1]), Path(sys.argv[2]))
