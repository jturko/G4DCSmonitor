#!/usr/bin/env python3
"""Composite four per-reflector muon-movie frame sets into one 2x2 movie frame.

build_movie_quad.sh renders the SAME event for reflector 0/1/2/3 and writes one
rasterised PNG per time step into <base>/refl<r>/<stem>_<k>.png.  This script
pastes the four panes for each time step k into a single 2x2 canvas (reflector
0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right) with a small text label
in each pane, so the assembled GIF/MP4 shows all four reflectors advancing in
time together.

Usage: montage_quad.py <base_dir> <out_dir> <stem> [scale]
  base_dir = quad root (contains refl0/ .. refl3/)
  out_dir  = where the composited quad_<k>.png frames are written
  stem     = per-pane frame basename (e.g. qframe)
  scale    = optional resize factor for the composite (default 1.0)
"""
import os
import sys
import glob

from PIL import Image, ImageDraw, ImageFont

REFLECTORS = [0, 1, 2, 3]
LABELS = {
    0: "refl 0: none (bare)",
    1: "refl 1: TiO2 paint",
    2: "refl 2: Al foil",
    3: "refl 3: glossy paper",
}


def load_font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


def draw_label(draw, xy, text, font):
    l, t, r, b = draw.textbbox(xy, text, font=font)
    pad = 3
    draw.rectangle([l - pad, t - pad, r + pad, b + pad], fill=(0, 0, 0))
    draw.text(xy, text, font=font, fill=(255, 245, 120))


def main():
    if len(sys.argv) < 4:
        print("usage: montage_quad.py <base_dir> <out_dir> <stem> [scale]",
              file=sys.stderr)
        return 1
    base, out_dir, stem = sys.argv[1], sys.argv[2], sys.argv[3]
    scale = float(sys.argv[4]) if len(sys.argv) > 4 else 1.0

    panel = {}
    for r in REFLECTORS:
        panel[r] = sorted(glob.glob(os.path.join(base, f"refl{r}", f"{stem}_*.png")))
        if not panel[r]:
            print(f"montage_quad: no frames for reflector {r}: "
                  f"{base}/refl{r}/{stem}_*.png", file=sys.stderr)
            return 1

    counts = [len(v) for v in panel.values()]
    if len(set(counts)) != 1:
        print(f"montage_quad: warning: panes have differing frame counts "
              f"{counts}; using the first {min(counts)}", file=sys.stderr)
    n = min(counts)

    os.makedirs(out_dir, exist_ok=True)

    w, h = Image.open(panel[0][0]).size
    font = load_font(max(16, h // 22))
    corners = {0: (0, 0), 1: (w, 0), 2: (0, h), 3: (w, h)}

    for k in range(n):
        canvas = Image.new("RGB", (2 * w, 2 * h), (255, 255, 255))
        for r in REFLECTORS:
            canvas.paste(Image.open(panel[r][k]).convert("RGB"), corners[r])
        draw = ImageDraw.Draw(canvas)
        for r in REFLECTORS:
            ox, oy = corners[r]
            draw_label(draw, (ox + 8, oy + 8), LABELS[r], font)
        if scale != 1.0:
            canvas = canvas.resize(
                (max(1, int(2 * w * scale)), max(1, int(2 * h * scale))),
                Image.LANCZOS)
        canvas.save(os.path.join(out_dir, f"quad_{k:04d}.png"))

    print(f"montage_quad: composited {n} frames ({2 * w}x{2 * h}) -> {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
