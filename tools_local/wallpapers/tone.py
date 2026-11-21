#!/usr/bin/env python3
"""Tone mapping and error diffusion for the wallpaper pack, after esp32-photoframe.

The S-curve and the four diffusion kernels are the ones in
aitjcize/epaper-image-convert (MIT, Copyright (c) 2026 Wei-Ning Huang), the
converter behind aitjcize/esp32-photoframe, rewritten for a one-bit panel:
there is no palette to match here, only ink or paper, so what carries over is
the tone curve that runs before the dither and the choice of kernel. The scan
is serpentine, which that converter does not do; it stops the diagonal drift
a one-way scan leaves in flat skies. Credit is in THIRD-PARTY-NOTICES.md.

    from tone import scurve, diffuse
"""
import numpy as np
from PIL import Image

# (dx, dy, weight): where each pixel's rounding error goes, scanning rightwards.
KERNELS = {
    "floyd": ((1, 0, 7 / 16), (-1, 1, 3 / 16), (0, 1, 5 / 16), (1, 1, 1 / 16)),
    "stucki": ((1, 0, 8 / 42), (2, 0, 4 / 42), (-2, 1, 2 / 42), (-1, 1, 4 / 42), (0, 1, 8 / 42), (1, 1, 4 / 42),
               (2, 1, 2 / 42), (-2, 2, 1 / 42), (-1, 2, 2 / 42), (0, 2, 4 / 42), (1, 2, 2 / 42), (2, 2, 1 / 42)),
    "burkes": ((1, 0, 8 / 32), (2, 0, 4 / 32), (-2, 1, 2 / 32), (-1, 1, 4 / 32), (0, 1, 8 / 32), (1, 1, 4 / 32),
               (2, 1, 2 / 32)),
    "sierra": ((1, 0, 5 / 32), (2, 0, 3 / 32), (-2, 1, 2 / 32), (-1, 1, 4 / 32), (0, 1, 5 / 32), (1, 1, 4 / 32),
               (2, 1, 2 / 32), (-1, 2, 2 / 32), (0, 2, 3 / 32), (1, 2, 2 / 32)),
}


def scurve(im, strength=0.9, shadow=0.0, highlight=1.5, midpoint=0.5):
    """Lifts or holds the shadows and compresses the highlights around `midpoint`.

    Below the midpoint a tone v becomes (v/m)^(1 - strength*shadow) * m; above
    it, m + ((v-m)/(1-m))^(1 + strength*highlight) * (1-m). With the defaults
    the dark half is untouched and the light half is pulled down, which is what
    keeps pale paper and skies from dithering away to bare white.
    """
    v = np.asarray(im.convert("L"), dtype=np.float32) / 255.0
    low = np.power(np.clip(v / midpoint, 0, 1), 1.0 - strength * shadow) * midpoint
    high = midpoint + np.power(np.clip((v - midpoint) / (1.0 - midpoint), 0, 1), 1.0 + strength * highlight) * (
        1.0 - midpoint)
    out = np.where(v <= midpoint, low, high)
    return Image.fromarray(np.uint8(np.round(np.clip(out, 0, 1) * 255)), "L")


def diffuse(im, kernel="stucki", serpentine=True):
    """A grey picture to one bit by error diffusion. Returns a mode "1" image."""
    taps = KERNELS[kernel]
    reach = max(abs(dx) for dx, _, _ in taps)
    depth = max(dy for _, dy, _ in taps)
    grey = np.asarray(im.convert("L"), dtype=np.float32)
    h, w = grey.shape
    # A margin on every side the kernel reaches, so no tap needs a bounds test.
    work = np.zeros((h + depth, w + 2 * reach), dtype=np.float32)
    work[:h, reach:reach + w] = grey
    out = np.zeros((h, w), dtype=np.uint8)
    for y in range(h):
        row = work[y]
        backwards = serpentine and (y & 1)
        xs = range(w - 1, -1, -1) if backwards else range(w)
        sign = -1 if backwards else 1
        for x in xs:
            old = row[x + reach]
            new = 255.0 if old >= 127.5 else 0.0
            out[y, x] = 255 if new else 0
            err = old - new
            if err:
                for dx, dy, weight in taps:
                    work[y + dy, x + reach + sign * dx] += err * weight
    return Image.fromarray(out, "L").convert("1", dither=Image.NONE)
