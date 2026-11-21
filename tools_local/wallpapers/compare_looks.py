#!/usr/bin/env python3
"""The same wallpapers in each look, side by side at the panel's own size.

    python3 tools_local/wallpapers/compare_looks.py <folder of originals> <out folder> [id ...]

For each picture, one PNG three panels wide: the pack as it is built today
(Pillow's Floyd-Steinberg), Stucki with a serpentine scan, and the S-curve
followed by Stucki. Nothing is scaled, so a pixel here is a pixel on the glass.
"""
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_pack import H, W, picture  # noqa: E402

LOOKS = (("none", "pillow"), ("none", "stucki"), ("scurve", "stucki"))


def main():
    folder, out = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3:])
    os.makedirs(out, exist_ok=True)
    for c in json.load(open(os.path.join(folder, "elegidos.json"), encoding="utf-8")):
        path = os.path.join(folder, "%s.jpg" % c["id"])
        if (only and str(c["id"]) not in only) or not os.path.exists(path):
            continue
        sheet = Image.new("L", (len(LOOKS) * (W + 8) - 8, H), 128)
        for i, (tone, dither) in enumerate(LOOKS):
            sheet.paste(picture(path, c.get("focus", 0.5), c.get("trim", True), tone, dither).convert("L"),
                        (i * (W + 8), 0))
        sheet.save(os.path.join(out, "%s.png" % c["id"]))
        print(c["id"])


if __name__ == "__main__":
    main()
