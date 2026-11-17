#!/usr/bin/env python3
"""Join-the-dots figures for ILYA's PUNTS, traced from the game's own pictures.

Each 96px picture in src/apps_local/kids/KidsPictures.h is turned into its
silhouette (everything the white outside cannot reach), the outline of the
largest piece is walked, and that outline is thinned to three sets of dots:
few, some and many. The dots are written to src/apps_local/kids/KidsDots.h.

    python3 tools_local/kids/make_dots.py            # writes the header
    python3 tools_local/kids/make_dots.py --sheet out.png   # and a picture of every figure (needs Pillow)
"""
import math
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, "src", "apps_local", "kids", "KidsPictures.h")
OUT = os.path.join(ROOT, "src", "apps_local", "kids", "KidsDots.h")
SIZE = 96
LEVELS = (9, 16, 26)      # dots asked for at each level
MIN_GAP = (14.0, 9.0, 7.5)  # and the least distance between neighbours, in picture pixels
# Pictures whose largest piece is not the picture: a bicycle traces as one wheel.
SKIP = ("bici",)


def pictures():
    text = open(SRC).read()
    order = re.search(r"kAll\[\] = \{(.*?)\};", text, re.S).group(1)
    names = re.findall(r"&icon_(\w+)_96", order)
    out = []
    for name in names:
        body = re.search(r"icon_%s_96_bits\[\] = \{(.*?)\};" % name, text, re.S).group(1)
        data = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body)]
        grid = [[(data[y * 12 + x // 8] >> (7 - x % 8)) & 1 == 0 for x in range(SIZE)] for y in range(SIZE)]
        out.append((name, grid))
    return out


def silhouette(ink):
    """Everything the outside cannot reach through white, 4-connected."""
    n = SIZE + 2
    outside = [[False] * n for _ in range(n)]
    stack = [(0, 0)]
    outside[0][0] = True
    while stack:
        x, y = stack.pop()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            a, b = x + dx, y + dy
            if 0 <= a < n and 0 <= b < n and not outside[b][a]:
                inside = 1 <= a <= SIZE and 1 <= b <= SIZE and ink[b - 1][a - 1]
                if not inside:
                    outside[b][a] = True
                    stack.append((a, b))
    return [[not outside[y + 1][x + 1] for x in range(SIZE)] for y in range(SIZE)]


def largest(shape):
    seen = [[False] * SIZE for _ in range(SIZE)]
    best = []
    for y in range(SIZE):
        for x in range(SIZE):
            if shape[y][x] and not seen[y][x]:
                piece, stack = [], [(x, y)]
                seen[y][x] = True
                while stack:
                    a, b = stack.pop()
                    piece.append((a, b))
                    for dx in (-1, 0, 1):
                        for dy in (-1, 0, 1):
                            c, d = a + dx, b + dy
                            if 0 <= c < SIZE and 0 <= d < SIZE and shape[d][c] and not seen[d][c]:
                                seen[d][c] = True
                                stack.append((c, d))
                if len(piece) > len(best):
                    best = piece
    keep = set(best)
    return [[(x, y) in keep for x in range(SIZE)] for y in range(SIZE)], len(best)


def outline(shape):
    """Moore-neighbour walk round the piece, clockwise from its top-left pixel."""
    def at(x, y):
        return 0 <= x < SIZE and 0 <= y < SIZE and shape[y][x]
    start = next((x, y) for y in range(SIZE) for x in range(SIZE) if shape[y][x])
    ring = ((-1, 0), (-1, -1), (0, -1), (1, -1), (1, 0), (1, 1), (0, 1), (-1, 1))
    path, cur, back = [start], start, 0
    for _ in range(SIZE * SIZE * 4):
        for k in range(8):
            i = (back + 1 + k) % 8
            nx, ny = cur[0] + ring[i][0], cur[1] + ring[i][1]
            if at(nx, ny):
                back = (i + 4) % 8
                cur = (nx, ny)
                break
        else:
            break
        if cur == start:
            break
        path.append(cur)
    return path


def thin(path, want, gap):
    """The `want` points that keep the outline's shape best: drop, one at a time, the one whose loss costs least."""
    pts = path[:: max(1, len(path) // 240)]

    def cost(i):
        a, b, c = pts[i - 1], pts[i], pts[(i + 1) % len(pts)]
        return abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]))

    while len(pts) > want:
        i = min(range(len(pts)), key=cost)
        del pts[i]
    # Neighbours a finger cannot tell apart are merged.
    changed = True
    while changed and len(pts) > 5:
        changed = False
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            if math.hypot(a[0] - b[0], a[1] - b[1]) < gap:
                del pts[i if cost(i) < cost((i + 1) % len(pts)) else (i + 1) % len(pts)]
                changed = True
                break
    # Start at the top, as a child reading 1 would expect.
    top = min(range(len(pts)), key=lambda i: (pts[i][1], pts[i][0]))
    return pts[top:] + pts[:top]


def figures():
    out = []
    for number, (name, ink) in enumerate(pictures()):
        if name in SKIP:
            continue
        shape, area = largest(silhouette(ink))
        path = outline(shape)
        out.append((name, [thin(path, LEVELS[k], MIN_GAP[k]) for k in range(3)], number))
    return out


def write(figs):
    lines = ["#pragma once", "",
             "// Join-the-dots figures for PUNTS, traced from KidsPictures.h by",
             "// tools_local/kids/make_dots.py. Do not edit: run the tool.",
             "//",
             "// One figure a picture (kPicture says which), at three levels. A dot is",
             "// two bytes, x then y, in the picture's own 96-pixel square.", "",
             "#include <cstdint>", "", "namespace kidsdots {", "",
             "constexpr int kFigures = %d;" % len(figs), "constexpr int kLevels = 3;", "",
             "inline constexpr uint8_t kDots[] = {"]
    index, at = [], 0
    for name, levels, _ in figs:
        for k, pts in enumerate(levels):
            index.append((at, len(pts)))
            row = ", ".join("%d, %d" % p for p in pts)
            lines.append("    // %s, level %d" % (name, k))
            for i in range(0, len(pts), 12):
                lines.append("    " + ", ".join("%d, %d" % p for p in pts[i:i + 12]) + ",")
            at += len(pts)
    lines += ["};", "", "struct Span {", "  uint16_t first;  // index of the figure's first dot, in dots", "  uint8_t count;",
              "};", "inline constexpr Span kSpans[kFigures * kLevels] = {"]
    for i in range(0, len(index), 6):
        lines.append("    " + ", ".join("{%d, %d}" % s for s in index[i:i + 6]) + ",")
    lines += ["};", "", "// The picture each figure is of, an index into KidsCore's words.",
              "inline constexpr uint8_t kPicture[kFigures] = {",
              "    " + ", ".join(str(f[2]) for f in figs) + ",", "};", "", "}  // namespace kidsdots", ""]
    open(OUT, "w").write("\n".join(lines))
    print("%d figures, %d dots, %s" % (len(figs), at, os.path.relpath(OUT, ROOT)))


def sheet(figs, path):
    from PIL import Image, ImageDraw
    s, cols = 3, 9
    rows = (len(figs) + cols - 1) // cols
    im = Image.new("L", (cols * 3 * (SIZE * s // 3 + 4), rows * (SIZE + 14)), 255)
    d = ImageDraw.Draw(im)
    for n, (name, levels, _) in enumerate(figs):
        for k, pts in enumerate(levels):
            ox = (n % cols) * 3 * (SIZE + 4) + k * (SIZE + 4)
            oy = (n // cols) * (SIZE + 14)
            d.polygon([(ox + x, oy + y) for x, y in pts], outline=0)
            for x, y in pts:
                d.ellipse((ox + x - 1, oy + y - 1, ox + x + 1, oy + y + 1), fill=0)
            if k == 0:
                d.text((ox, oy + SIZE), "%d %s" % (n, name), fill=0)
    im.save(path)


if __name__ == "__main__":
    figs = figures()
    write(figs)
    if "--sheet" in sys.argv:
        sheet(figs, sys.argv[sys.argv.index("--sheet") + 1])
