#!/usr/bin/env python3
"""Builds the wallpaper pack the FONDOS app carries, from the pictures chosen.

Each picture is cropped to the panel's shape (480 by 800, upright), given its
full range of greys, and dithered to black and white; the pack is those as
one-bit BMP files, one after another, behind a list of what they are.

    python3 tools_local/wallpapers/make_pack.py <folder with NNN.jpg and elegidos.json> [--sheet out.png] [--out file]
        [--tone none|scurve] [--dither pillow|floyd|stucki|burkes|sierra]

--tone and --dither choose the look (tone.py, after esp32-photoframe). Left
out, the pack is built the way the one in the tree was: no curve, Pillow's own
Floyd-Steinberg. fetch_originals.py fills the folder from the museum.

Needs Pillow. Writes src/apps_local/fondos/fondos.pack.

The pack:  "FND1", count (u16 LE), bytes per picture (u32 LE), then for each
picture 96 bytes of text (title[56], artist[40], NUL padded, Latin-1), then the
pictures, each a complete 1-bit BMP of exactly that many bytes.
"""
import json
import os
import struct
import sys
import unicodedata

from PIL import Image, ImageFilter, ImageOps

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tone import KERNELS, diffuse, scurve  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "src", "apps_local", "fondos", "fondos.pack")
W, H = 480, 800
ORDER = ("japonesa", "naturaleza", "grabados", "paisaje", "botanica")


def latin(text, cap):
    """What the reader's faces can draw: Latin-1, with everything else folded to its plain letter."""
    out = []
    for ch in text or "":
        try:
            ch.encode("latin-1")
            out.append(ch)
        except UnicodeEncodeError:
            plain = unicodedata.normalize("NFKD", ch).encode("ascii", "ignore").decode()
            out.append(plain or "'" if ch in "‘’" else plain)
    return "".join(out).replace('""', '"').encode("latin-1")[:cap - 1].ljust(cap, b"\0")


def short_title(title):
    # "Hara, from the series "Fifty-three Stations of the Tokaido..." is the print called Hara.
    for cut in (", from the series", ", from ", " (", ", plate"):
        at = (title or "").find(cut)
        if at > 3:
            title = title[:at]
    return title


def trimmed(im):
    """A print photographed on its sheet: cut the empty paper away, down to the plate."""
    small = im.resize((im.width // 4, im.height // 4)).filter(ImageFilter.GaussianBlur(2))
    # The paper is the commonest light tone; anything clearly darker is picture.
    hist = small.histogram()
    paper = max(range(128, 256), key=lambda v: hist[v])
    mask = small.point(lambda v: 255 if v < paper - 38 else 0)
    box = mask.getbbox()
    if not box:
        return im
    l, t, r, b = (v * 4 for v in box)
    # Only when there is a real margin to lose, and never down to a sliver.
    if (r - l) < im.width * 0.35 or (b - t) < im.height * 0.35:
        return im
    if (r - l) > im.width * 0.93 and (b - t) > im.height * 0.93:
        return im
    pad = round(min(im.width, im.height) * 0.012)
    return im.crop((max(0, l - pad), max(0, t - pad), min(im.width, r + pad), min(im.height, b + pad)))


def picture(path, focus, trim=True, tone="none", dither="pillow"):
    im = ImageOps.exif_transpose(Image.open(path)).convert("L")
    if trim:
        im = trimmed(im)
    # Fill the panel: scale so both sides cover it, then crop the overhang, keeping `focus` of the way down.
    scale = max(W / im.width, H / im.height)
    im = im.resize((max(W, round(im.width * scale)), max(H, round(im.height * scale))), Image.LANCZOS)
    left = (im.width - W) // 2
    top = round((im.height - H) * focus)
    im = im.crop((left, top, left + W, top + H))
    im = ImageOps.autocontrast(im, cutoff=1)
    if tone == "scurve":
        im = scurve(im)
    if dither == "pillow":
        return im.convert("1", dither=Image.FLOYDSTEINBERG)
    return diffuse(im, dither)


def bmp(im):
    row = ((W + 31) // 32) * 4
    bits = im.tobytes()  # top-down, 1 = white
    data = b"".join(bits[y * (W // 8):(y + 1) * (W // 8)].ljust(row, b"\0") for y in range(H - 1, -1, -1))
    header = struct.pack("<2sIHHI", b"BM", 62 + len(data), 0, 0, 62)
    info = struct.pack("<IiiHHIIiiII", 40, W, H, 1, 1, 0, len(data), 2835, 2835, 2, 2)
    palette = bytes((0, 0, 0, 0, 255, 255, 255, 0))
    return header + info + palette + data


def main():
    folder = sys.argv[1]
    out_path = sys.argv[sys.argv.index("--out") + 1] if "--out" in sys.argv else OUT
    tone = sys.argv[sys.argv.index("--tone") + 1] if "--tone" in sys.argv else "none"
    dither = sys.argv[sys.argv.index("--dither") + 1] if "--dither" in sys.argv else "pillow"
    if tone not in ("none", "scurve") or (dither != "pillow" and dither not in KERNELS):
        sys.exit("unknown --tone %r or --dither %r" % (tone, dither))
    chosen = json.load(open(os.path.join(folder, "elegidos.json")))
    chosen.sort(key=lambda c: (ORDER.index(c["kind"]) if c["kind"] in ORDER else 9, c.get("artist") or "", c["id"]))
    texts, pictures, sheet = [], [], []
    for c in chosen:
        path = os.path.join(folder, "%s.jpg" % c["id"])
        if not os.path.exists(path):
            print("missing %s" % path)
            continue
        im = picture(path, c.get("focus", 0.5), c.get("trim", True), tone, dither)
        pictures.append(bmp(im))
        texts.append(latin(short_title(c.get("title")), 56) + latin(c.get("artist") or "", 40))
        sheet.append(im)
    size = len(pictures[0])
    assert all(len(p) == size for p in pictures)
    os.makedirs(os.path.dirname(os.path.abspath(out_path)), exist_ok=True)
    with open(out_path, "wb") as f:
        f.write(b"FND1" + struct.pack("<HI", len(pictures), size))
        f.write(b"".join(texts))
        f.write(b"".join(pictures))
    print("%d pictures, %d bytes, %s" % (len(pictures), os.path.getsize(out_path), out_path))
    if "--sheet" in sys.argv:
        cols = 10
        rows = (len(sheet) + cols - 1) // cols
        out = Image.new("L", (cols * 244, rows * 404), 128)
        for i, im in enumerate(sheet):
            out.paste(im.convert("L").resize((240, 400)), ((i % cols) * 244, (i // cols) * 404))
        out.save(sys.argv[sys.argv.index("--sheet") + 1])


if __name__ == "__main__":
    main()
