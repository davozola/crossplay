#!/usr/bin/env python3
"""Draws the Sticky wallpaper pack: sixteen 480x800 1-bit BMPs, every one made
here from arithmetic and a fixed seed, so there is no artwork of anyone else's
in it and the pack can be rebuilt byte for byte.

    python3 tools_local/sticky_wallpapers/gen.py OUT_DIR [--sheet sheet.png]

Writes OUT_DIR/<stem>.bmp for each, and OUT_DIR/sticky.dat: the files end to
end in the order of STEMS, which is the order WallpapersCore's kExtras lists.
"""
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

W, H = 480, 800
SS = 2  # drawn at twice the size, then reduced


def rng(seed):
    return np.random.default_rng(seed)


def fbm1(n, seed, octaves=5, base=4):
    """A 1-D fractal profile of n samples in 0..1."""
    r = rng(seed)
    out = np.zeros(n)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        k = base * 2**o + 3
        pts = r.random(k)
        x = np.linspace(0, k - 3, n)
        i = x.astype(int)
        f = x - i
        f = f * f * (3 - 2 * f)
        out += amp * (pts[i] * (1 - f) + pts[i + 1] * f)
        total += amp
        amp *= 0.5
    return out / total


def fbm2(w, h, seed, octaves=5, base=3):
    """A 2-D fractal field, h x w, in 0..1."""
    r = rng(seed)
    out = np.zeros((h, w), dtype=np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        k = base * 2**o
        small = (r.random((k * h // w + 2, k + 2)) * 255).astype(np.uint8)
        big = Image.fromarray(small).resize((w, h), Image.BICUBIC)
        out += amp * np.asarray(big, dtype=np.float32) / 255
        total += amp
        amp *= 0.5
    out /= total
    return (out - out.min()) / (out.max() - out.min())


def new(shade=1.0):
    return Image.new("L", (W * SS, H * SS), int(shade * 255))


def down(img):
    return img.resize((W, H), Image.LANCZOS)


BAYER8 = (
    np.array(
        [
            [0, 32, 8, 40, 2, 34, 10, 42],
            [48, 16, 56, 24, 50, 18, 58, 26],
            [12, 44, 4, 36, 14, 46, 6, 38],
            [60, 28, 52, 20, 62, 30, 54, 22],
            [3, 35, 11, 43, 1, 33, 9, 41],
            [51, 19, 59, 27, 49, 17, 57, 25],
            [15, 47, 7, 39, 13, 45, 5, 37],
            [63, 31, 55, 23, 61, 29, 53, 21],
        ]
    )
    + 0.5
) / 64


def dither(img, how="fs"):
    """An 'L' image at W x H to a boolean array, True for white."""
    g = np.asarray(img, dtype=np.float32) / 255
    if how == "bayer":
        t = np.tile(BAYER8, (H // 8 + 1, W // 8 + 1))[:H, :W]
        return (g > t).copy()
    return np.array(Image.fromarray((g * 255).astype(np.uint8)).convert("1"), dtype=bool)


def to_img(bits):
    return Image.fromarray((bits * 255).astype(np.uint8)).convert("1")


def ridge_poly(profile, base, amp):
    """Polygon points (at SS scale) under a ridge line."""
    n = len(profile)
    pts = [(i * W * SS / (n - 1), (base - amp * profile[i]) * SS) for i in range(n)]
    return pts + [(W * SS, H * SS), (0, H * SS)]


def stars(bits, seed, count, where):
    """Scatter single and plus-shaped stars where `where` is true and the sky is dark."""
    r = rng(seed)
    for _ in range(count):
        x, y = int(r.integers(2, W - 2)), int(r.integers(2, H - 2))
        if not where[y, x] or bits[y - 2 : y + 3, x - 2 : x + 3].any():
            continue
        bits[y, x] = True
        if r.random() < 0.18:
            bits[y - 1, x] = bits[y + 1, x] = bits[y, x - 1] = bits[y, x + 1] = True
        if r.random() < 0.04:
            bits[y - 2, x] = bits[y + 2, x] = bits[y, x - 2] = bits[y, x + 2] = True


def gradient(top, bottom, y0=0, y1=H):
    """A vertical ramp as an 'L' image at SS scale."""
    col = np.clip((np.arange(H * SS) / SS - y0) / max(1, y1 - y0), 0, 1)
    col = top + (bottom - top) * col
    return Image.fromarray((np.tile(col[:, None], (1, W * SS)) * 255).astype(np.uint8))


# ---------------------------------------------------------------- the pieces


def moonrise():
    img = gradient(0.0, 0.30, 0, 620)
    d = ImageDraw.Draw(img)
    cx, cy, rad = 250, 250, 150
    # The moon: a lit sphere with maria from a noise field.
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    dx, dy = (xx - cx) / rad, (yy - cy) / rad
    rr = dx * dx + dy * dy
    inside = rr < 1
    z = np.sqrt(np.clip(1 - rr, 0, 1))
    light = np.clip(0.55 * z + 0.45 * (-0.5 * dx - 0.45 * dy + 0.74 * z), 0, 1)
    maria = fbm2(W * SS, H * SS, 11, octaves=5, base=5)
    shade = 0.62 + 0.38 * light - 0.42 * np.clip((maria - 0.52) * 4, 0, 1)
    arr = np.asarray(img, dtype=np.float32) / 255
    glow = np.clip(1 - (np.sqrt(rr) - 1) / 0.55, 0, 1) ** 2 * 0.35
    arr = np.where(inside, np.clip(shade, 0.25, 1), np.maximum(arr, arr + glow * (~inside)))
    img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # Ridges, nearer ones darker and lower.
    for i, (base, amp, shade_) in enumerate([(560, 150, 0.30), (640, 150, 0.18), (720, 140, 0.08), (800, 110, 0.0)]):
        d.polygon(ridge_poly(fbm1(240, 20 + i, octaves=6, base=3 + i), base, amp), fill=int(shade_ * 255))
    small = down(img)
    bits = dither(small)
    sky = np.asarray(small) < 90
    sky[420:, :] = False
    stars(bits, 5, 260, sky)
    return bits


def swell():
    img = new(0.0)
    d = ImageDraw.Draw(img)
    horizon = 300
    # Sky: a low moon and its halo.
    arr = np.zeros((H * SS, W * SS), dtype=np.float32)
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    r = np.hypot(xx - 240, yy - 170)
    arr = np.clip(1 - (r - 46) / 170, 0, 1) ** 2.2 * 0.45
    arr[r < 46] = 1.0
    arr[int(horizon * SS) :, :] = 0
    img = Image.fromarray((arr * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # Sea: wave crests that widen and thicken toward the viewer, brightest under the moon.
    y = horizon + 3.0
    k = 0
    while y < H + 20:
        t = (y - horizon) / (H - horizon)
        amp = 1.5 + 16 * t * t
        length = 26 + 240 * t
        thick = 1.0 + 5.5 * t
        phase = k * 1.9
        pts_top, pts_bot = [], []
        for x in range(-10, W + 11, 3):
            wob = math.sin(x / length * 2 * math.pi + phase) + 0.45 * math.sin(x / (length * 0.37) * 2 * math.pi + phase * 1.7)
            near = math.exp(-(((x - 240) / (26 + 150 * t)) ** 2))
            w_ = thick * (0.25 + 0.95 * near) * (0.6 + 0.4 * math.sin(x / (length * 0.6) + phase))
            yc = y + amp * wob
            pts_top.append((x * SS, (yc - max(0.0, w_)) * SS))
            pts_bot.append((x * SS, (yc + max(0.0, w_) * 0.6) * SS))
        d.polygon(pts_top + pts_bot[::-1], fill=255)
        y += 5 + 26 * t
        k += 1
    small = down(img)
    bits = dither(small)
    sky = np.zeros((H, W), dtype=bool)
    sky[: horizon - 6, :] = True
    sky &= np.asarray(small) < 30
    stars(bits, 9, 200, sky)
    return bits


def regatta():
    img = new(1.0)
    d = ImageDraw.Draw(img)
    horizon = 520
    # A big sun cut by slats that widen toward the water.
    cx, cy, rad = 240, 330, 170
    d.ellipse([(cx - rad) * SS, (cy - rad) * SS, (cx + rad) * SS, (cy + rad) * SS], fill=0)
    y, gap = cy - 20.0, 3.0
    while y < cy + rad:
        d.rectangle([0, y * SS, W * SS, (y + gap) * SS], fill=255)
        y += gap + 22 - gap * 0.6
        gap += 3.4
    d.rectangle([0, horizon * SS, W * SS, H * SS], fill=255)
    # Water: broken horizontal strokes, denser near the horizon.
    r = rng(3)
    y = horizon + 2.0
    while y < H:
        t = (y - horizon) / (H - horizon)
        n = int(26 - 14 * t)
        for _ in range(n):
            x = r.random() * W
            ln = (10 + 70 * t) * (0.4 + r.random())
            under = math.exp(-(((x - 240) / (150 + 60 * t)) ** 2))
            if r.random() < 0.25 + 0.75 * under:
                d.rounded_rectangle([(x - ln / 2) * SS, y * SS, (x + ln / 2) * SS, (y + 1.6 + 4.5 * t) * SS], radius=4, fill=0)
        y += 6 + 16 * t
    d.rectangle([0, horizon * SS, W * SS, (horizon + 2.5) * SS], fill=0)

    # Two boats: hull, mast, main and jib.
    def boat(x, y, s, flip=1):
        hull = [(-34, 0), (40, 0), (30, 11), (-26, 11)]
        main = [(-2, -6), (-2, -112), (-46, -6)]
        jib = [(4, -8), (4, -92), (36, -8)]
        for poly in (hull, main, jib):
            d.polygon([((x + flip * px * s) * SS, (y + py * s) * SS) for px, py in poly], fill=0)
        d.rectangle([(x - 1.2 * s) * SS, (y - 118 * s) * SS, (x + 1.2 * s) * SS, y * SS], fill=0)
        # Its reflection, as a few shrinking bars.
        for i in range(5):
            wd = (30 - i * 5) * s
            d.rectangle([(x - wd) * SS, (y + (16 + i * 7) * s) * SS, (x + wd) * SS, (y + (18.5 + i * 7) * s) * SS], fill=0)

    boat(150, 590, 1.25)
    boat(350, 545, 0.55, -1)
    for bx, by in [(90, 150), (120, 138), (385, 110)]:  # gulls
        d.arc([(bx - 14) * SS, (by - 6) * SS, bx * SS, (by + 8) * SS], 200, 340, fill=0, width=5)
        d.arc([bx * SS, (by - 6) * SS, (bx + 14) * SS, (by + 8) * SS], 200, 340, fill=0, width=5)
    return dither(down(img))


def dunes():
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    arr = np.clip(0.5 + 0.45 * (yy / 400), 0, 0.96)  # sky, paler toward the sand
    r = np.hypot(xx - 345, yy - 150)
    arr = np.where(r < 58, 1.0, arr)
    cols = np.arange(W * SS) / SS
    # Dunes back to front. Each crest is a smooth curve; the side facing the sun
    # is pale, the lee side dark, and both ease toward mid-tone down the slope.
    for i in range(6):
        base = 300 + i * 96
        amp = 70 + i * 22
        crest = base - amp * (0.5 + 0.5 * np.sin(cols / W * math.pi * (1.3 + 0.35 * i) + i * 2.1)) - 22 * np.sin(cols / W * math.pi * 5.3 + i)
        slope = np.gradient(crest)
        lee = 1 / (1 + np.exp(-slope * SS * 14))  # 1 where the crest falls to the right
        lit, dark = 0.93 - i * 0.03, 0.30 - i * 0.045
        depth = np.clip((yy - crest[None, :]) / (110 + i * 30), 0, 1)
        tone = (lit + (dark - lit) * lee)[None, :]
        tone = tone + (0.62 - i * 0.06 - tone) * depth**0.8
        # Wind ripples: fine lines that follow the crest, stronger on the near dunes.
        ripple = np.sin((yy - crest[None, :]) * (0.95 - i * 0.06) + 3 * np.sin(cols / 23)[None, :])
        tone = tone + ripple * (0.02 + 0.022 * i) * (depth > 0.04)
        arr = np.where(yy > crest[None, :], tone, arr)
    return dither(down(Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))))


def summit():
    img = gradient(0.92, 1.0, 0, 300)
    d = ImageDraw.Draw(img)
    d.ellipse([300 * SS, 90 * SS, 400 * SS, 190 * SS], fill=255, outline=110, width=3)
    layers = [(380, 230, 0.80, 2), (470, 250, 0.62, 3), (560, 230, 0.44, 3), (650, 210, 0.26, 4), (745, 190, 0.10, 4), (830, 150, 0.0, 5)]
    for i, (base, amp, shade, b) in enumerate(layers):
        prof = fbm1(300, 70 + i, octaves=7, base=b)
        prof = np.clip(prof * 1.25 - 0.1, 0, 1) ** 1.25
        layer = Image.new("L", img.size, 0)
        ImageDraw.Draw(layer).polygon(ridge_poly(prof, base, amp), fill=255)
        # Each range fades into mist toward its foot.
        fade = np.clip((np.arange(H * SS) / SS - (base - amp)) / (amp + 90), 0, 1)
        tone = shade + (min(1.0, shade + 0.42) - shade) * fade**1.6
        tone_img = Image.fromarray((np.tile(tone[:, None], (1, W * SS)) * 255).astype(np.uint8))
        img.paste(tone_img, (0, 0), layer)
    d = ImageDraw.Draw(img)
    for bx, by, s in [(110, 210, 1.0), (140, 232, 0.7), (82, 240, 0.6), (170, 205, 0.5)]:
        d.arc([(bx - 15 * s) * SS, (by - 6 * s) * SS, bx * SS, (by + 9 * s) * SS], 200, 340, fill=0, width=5)
        d.arc([bx * SS, (by - 6 * s) * SS, (bx + 15 * s) * SS, (by + 9 * s) * SS], 200, 340, fill=0, width=5)
    return dither(down(img))


def orbit():
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    arr = np.zeros((H * SS, W * SS), dtype=np.float32)
    cx, cy, rad = 240, 400, 138
    tilt = math.radians(-24)
    # Coordinates turned into the ring's frame.
    ux = (xx - cx) * math.cos(tilt) + (yy - cy) * math.sin(tilt)
    uy = -(xx - cx) * math.sin(tilt) + (yy - cy) * math.cos(tilt)
    rr = np.hypot(xx - cx, yy - cy) / rad
    z = np.sqrt(np.clip(1 - rr * rr, 0, 1))
    nx, ny = (xx - cx) / rad, (yy - cy) / rad
    light = np.clip(0.2 + 0.95 * (-0.62 * nx - 0.5 * ny + 0.6 * z), 0, 1)
    bands = 0.5 + 0.5 * np.sin(uy / rad * 9.5 + 0.8 * np.sin(ux / rad * 2.2))
    planet = np.clip(light * (0.62 + 0.38 * bands), 0, 1)
    # The rings: an ellipse of concentric bands with gaps.
    squash = 0.30
    ring_r = np.hypot(ux, uy / squash) / rad
    ring = ((ring_r > 1.32) & (ring_r < 2.25)).astype(np.float32)
    tone = 0.55 + 0.45 * np.sin(ring_r * 46)
    tone = np.where((ring_r > 1.72) & (ring_r < 1.80), 0, tone)
    ring *= np.clip(tone, 0, 1)
    # The planet's shadow falls across the far side of the rings.
    shadow = (ux > 0) & (np.abs(uy) < rad * 0.9 * np.sqrt(np.clip(1 - 0, 0, 1))) & (uy < 0) & (ux < rad * 2.4) & (np.abs(uy / squash) < rad * 1.0)
    front = uy > 0
    arr = np.where(rr < 1, planet, 0)
    arr = np.where((ring > 0) & ((rr >= 1) | front), ring * np.where(shadow & ~front, 0.0, 1.0), arr)
    img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # Two moons and their faint orbits.
    for a, b, ang in [(300, 96, 0.6), (390, 126, 3.9)]:
        pts = []
        for k in range(361):
            t = math.radians(k)
            px, py = a * math.cos(t), b * math.sin(t)
            pts.append(((cx + px * math.cos(tilt) - py * math.sin(tilt)) * SS, (cy + px * math.sin(tilt) + py * math.cos(tilt)) * SS))
        for k in range(0, 360, 4):
            seg = pts[k : k + 2]
            mx, my = seg[0][0] / SS, seg[0][1] / SS
            if math.hypot(mx - cx, my - cy) > rad * 2.3:
                d.line(seg, fill=150, width=3)
        px, py = a * math.cos(ang), b * math.sin(ang)
        mx, my = cx + px * math.cos(tilt) - py * math.sin(tilt), cy + px * math.sin(tilt) + py * math.cos(tilt)
        d.ellipse([(mx - 9) * SS, (my - 9) * SS, (mx + 9) * SS, (my + 9) * SS], fill=235)
    small = down(img)
    bits = dither(small)
    stars(bits, 21, 380, np.asarray(small) < 12)
    return bits


def lighthouse():
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    lx, ly = 300, 318
    ang = np.degrees(np.arctan2(yy - ly, xx - lx))
    dist = np.hypot(xx - lx, yy - ly)
    arr = np.zeros((H * SS, W * SS), dtype=np.float32)
    # Two beams, each brightest along its axis and fading with distance.
    for centre, half in [(-158, 15), (22, 11)]:
        off = np.abs(((ang - centre + 180) % 360) - 180)
        beam = np.clip(1 - off / half, 0, 1) ** 0.8
        arr = np.maximum(arr, beam * np.clip(1.05 - dist / 620, 0.12, 1))
    arr = np.maximum(arr, np.clip(1 - dist / 46, 0, 1) ** 1.5)
    horizon = 600
    img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # Sea: dark, with crest strokes that catch the light.
    d.rectangle([0, horizon * SS, W * SS, H * SS], fill=0)
    r = rng(8)
    y = horizon + 4.0
    while y < H:
        t = (y - horizon) / (H - horizon)
        for _ in range(int(22 - 10 * t)):
            x = r.random() * W
            ln = (8 + 60 * t) * (0.4 + r.random())
            d.rounded_rectangle([(x - ln / 2) * SS, y * SS, (x + ln / 2) * SS, (y + 1.2 + 3 * t) * SS], radius=3, fill=int(120 + 135 * r.random()))
        y += 5 + 15 * t
    # The rock and the tower.
    rock = fbm1(120, 31, octaves=5, base=3)
    pts = [((150 + i * 2.6) * SS, (horizon + 14 - 74 * rock[i] * math.sin(i / 119 * math.pi) ** 0.6) * SS) for i in range(120)]
    d.polygon(pts + [(462 * SS, (horizon + 30) * SS), (150 * SS, (horizon + 30) * SS)], fill=0)
    d.polygon([(272 * SS, 560 * SS), (286 * SS, 352 * SS), (314 * SS, 352 * SS), (328 * SS, 560 * SS)], fill=0)
    for k, (ya, yb) in enumerate([(380, 420), (460, 500)]):  # the white bands
        xa0, xa1 = 286 - (ya - 352) * 14 / 208, 314 + (ya - 352) * 14 / 208
        xb0, xb1 = 286 - (yb - 352) * 14 / 208, 314 + (yb - 352) * 14 / 208
        d.polygon([(xa0 * SS, ya * SS), (xa1 * SS, ya * SS), (xb1 * SS, yb * SS), (xb0 * SS, yb * SS)], fill=255)
    d.rectangle([278 * SS, 344 * SS, 322 * SS, 352 * SS], fill=0)  # gallery
    d.rectangle([288 * SS, 300 * SS, 312 * SS, 344 * SS], outline=0, width=6, fill=255)  # lantern
    d.polygon([(284 * SS, 300 * SS), (300 * SS, 276 * SS), (316 * SS, 300 * SS)], fill=0)
    d.line([(300 * SS, 276 * SS), (300 * SS, 262 * SS)], fill=0, width=4)
    d.rectangle([296 * SS, 520 * SS, 304 * SS, 540 * SS], fill=255)  # door light
    small = down(img)
    bits = dither(small)
    sky = np.asarray(small) < 10
    sky[horizon - 70 :, :] = False
    stars(bits, 2, 420, sky)
    return bits


def eixample():
    """A city of chamfered blocks seen from above, with one avenue cut across it."""
    img = new(1.0)
    d = ImageDraw.Draw(img)
    pitch, street, cham = 60, 13, 12
    r = rng(14)
    rot = math.radians(45)
    cx, cy = W / 2, H / 2

    def turn(px, py):
        return ((cx + px * math.cos(rot) - py * math.sin(rot)) * SS, (cy + px * math.sin(rot) + py * math.cos(rot)) * SS)

    for gy in range(-12, 13):
        for gx in range(-12, 13):
            x0, y0 = gx * pitch + street / 2, gy * pitch + street / 2
            x1, y1 = x0 + pitch - street, y0 + pitch - street
            outer = [(x0 + cham, y0), (x1 - cham, y0), (x1, y0 + cham), (x1, y1 - cham), (x1 - cham, y1), (x0 + cham, y1), (x0, y1 - cham), (x0, y0 + cham)]
            d.polygon([turn(*p) for p in outer], fill=0)
            # The courtyard inside each block: some open, some built over, a few gardens.
            m = 11
            inner = [(x0 + m, y0 + m), (x1 - m, y0 + m), (x1 - m, y1 - m), (x0 + m, y1 - m)]
            kind = r.random()
            if kind < 0.62:
                d.polygon([turn(*p) for p in inner], fill=255)
                if kind < 0.2:
                    mx, my = (x0 + x1) / 2, (y0 + y1) / 2
                    c = turn(mx, my)
                    d.ellipse([c[0] - 7 * SS, c[1] - 7 * SS, c[0] + 7 * SS, c[1] + 7 * SS], fill=0)
            elif kind < 0.8:
                d.polygon([turn(*p) for p in inner], fill=150)
    # The avenue: a broad white cut on the diagonal, with a planted median.
    a0, a1 = (-40, 610), (520, 150)
    d.line([(a0[0] * SS, a0[1] * SS), (a1[0] * SS, a1[1] * SS)], fill=255, width=30 * SS)
    n = 46
    for k in range(n):
        t = k / (n - 1)
        px, py = a0[0] + (a1[0] - a0[0]) * t, a0[1] + (a1[1] - a0[1]) * t
        d.ellipse([(px - 3.2) * SS, (py - 3.2) * SS, (px + 3.2) * SS, (py + 3.2) * SS], fill=0)
    # A square where the avenue meets the grid.
    d.ellipse([(250 - 44) * SS, (372 - 44) * SS, (250 + 44) * SS, (372 + 44) * SS], fill=255)
    d.ellipse([(250 - 30) * SS, (372 - 30) * SS, (250 + 30) * SS, (372 + 30) * SS], fill=0)
    d.ellipse([(250 - 18) * SS, (372 - 18) * SS, (250 + 18) * SS, (372 + 18) * SS], fill=255)
    return dither(down(img), "bayer")


def outrun():
    img = new(0.0)
    d = ImageDraw.Draw(img)
    horizon = 470
    cx, cy, rad = 240, 330, 150
    d.ellipse([(cx - rad) * SS, (cy - rad) * SS, (cx + rad) * SS, (cy + rad) * SS], fill=255)
    y, gap = cy + 6.0, 4.0
    while y < cy + rad:
        d.rectangle([0, y * SS, W * SS, (y + gap) * SS], fill=0)
        y += gap + 18 - gap * 0.5
        gap += 3.2
    # A jagged range in outline against the sun.
    prof = fbm1(60, 91, octaves=4, base=6)
    edge = np.abs(np.linspace(-1, 1, 60)) ** 0.7
    pts = [(i * W / 59 * SS, (horizon - (26 + 150 * prof[i]) * edge[i]) * SS) for i in range(60)]
    d.polygon(pts + [(W * SS, horizon * SS), (0, horizon * SS)], fill=0)
    d.line(pts, fill=255, width=4)
    for i in range(2, 58, 3):  # facets
        d.line([pts[i], ((pts[i][0] + (pts[i][0] - 240 * SS) * 0.12), horizon * SS)], fill=255, width=2)
    d.rectangle([0, horizon * SS, W * SS, H * SS], fill=0)
    # The floor: lines to a vanishing point, and cross lines that bunch at the horizon.
    for k in range(-14, 15):
        d.line([(240 * SS + k * 9 * SS, horizon * SS), (240 * SS + k * 120 * SS, H * SS)], fill=255, width=4)
    z = 1.0
    while True:
        y = horizon + (H - horizon) / z
        if y - horizon < 2:
            break
        d.line([(0, y * SS), (W * SS, y * SS)], fill=255, width=max(2, int(7 / z)))
        z *= 1.33
    d.line([(0, horizon * SS), (W * SS, horizon * SS)], fill=255, width=5)
    small = down(img)
    bits = dither(small)
    sky = np.zeros((H, W), dtype=bool)
    sky[:300, :] = True
    sky &= np.asarray(small) < 10
    stars(bits, 4, 240, sky)
    return bits


def lissajous():
    img = new(0.0)
    d = ImageDraw.Draw(img)
    # Graticule.
    for gx in range(0, W + 1, 60):
        d.line([(gx * SS, 0), (gx * SS, H * SS)], fill=70, width=2)
    for gy in range(40, H, 60):
        d.line([(0, gy * SS), (W * SS, gy * SS)], fill=70, width=2)
    figures = [(3, 2, math.pi / 2, 240, 215, 170, 150), (5, 4, math.pi / 4, 240, 585, 170, 150)]
    for a, b, ph, cx, cy, rx, ry in figures:
        pts = [((cx + rx * math.sin(a * t + ph)) * SS, (cy + ry * math.sin(b * t)) * SS) for t in np.linspace(0, 2 * math.pi, 2400)]
        glow = Image.new("L", img.size, 0)
        ImageDraw.Draw(glow).line(pts, fill=255, width=16)
        glow = glow.filter(ImageFilter.GaussianBlur(10))
        img = Image.fromarray(np.maximum(np.asarray(img), (np.asarray(glow) * 0.55).astype(np.uint8)))
        d = ImageDraw.Draw(img)
        d.line(pts, fill=255, width=7, joint="curve")
    return dither(down(img))


def jellyfish():
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    arr = np.clip(0.16 - yy / H * 0.16, 0, 1)  # a little light from above
    img = Image.fromarray((arr * 255).astype(np.uint8))
    r = rng(17)

    def jelly(cx, cy, s, seed):
        nonlocal img
        rr = rng(seed)
        d = ImageDraw.Draw(img)
        # Tentacles first, so the bell sits over them.
        for k in range(13):
            x0 = cx + (k - 6) * 9.5 * s
            ph, wl, amp = rr.random() * 6, 60 + rr.random() * 60, (6 + rr.random() * 12) * s
            ln = (190 + rr.random() * 230) * s
            pts = []
            for i in range(120):
                t = i / 119
                pts.append(((x0 + amp * t * math.sin(t * ln / wl * 2 * math.pi + ph) + (k - 6) * 3 * t * s) * SS, (cy + 20 * s + t * ln) * SS))
            d.line(pts, fill=int(110 + rr.random() * 110), width=max(2, int((2.4 if k % 3 else 4.2) * s)))
        # The bell: a dome, lit at the rim, ribbed.
        bw, bh = 96 * s, 86 * s
        layer = np.asarray(img, dtype=np.float32) / 255
        dx, dy = (xx - cx) / bw, (yy - cy) / bh
        dome = (dx * dx + dy * dy < 1) & (dy < 0.28 + 0.06 * np.cos(dx * 22))
        rim = np.sqrt(np.clip(dx * dx + dy * dy, 0, 1))
        ribs = 0.5 + 0.5 * np.cos(np.arctan2(dx, -dy + 1.35) * 34)
        tone = 0.30 + 0.55 * rim**3 + 0.22 * ribs * (1 - rim) + 0.25 * np.clip(-dy, 0, 1) * (1 - rim)
        layer = np.where(dome, np.clip(tone, 0, 1), layer)
        img = Image.fromarray((layer * 255).astype(np.uint8))
        d = ImageDraw.Draw(img)
        for k in range(5):  # the frilled arms under the bell
            x0 = cx + (k - 2) * 20 * s
            pts = [((x0 + 9 * s * math.sin(i / 7 + k)) * SS, (cy + 20 * s + i * 2.2 * s) * SS) for i in range(60)]
            d.line(pts, fill=235, width=int(7 * s))

    jelly(300, 250, 1.25, 1)
    jelly(120, 470, 0.72, 2)
    jelly(390, 600, 0.45, 3)
    small = down(img)
    bits = dither(small)
    # Marine snow.
    dark = np.asarray(small) < 30
    for _ in range(170):
        x, y = int(r.integers(1, W - 1)), int(r.integers(1, H - 1))
        if dark[y, x]:
            bits[y, x] = True
    return bits


def pines():
    img = gradient(1.0, 0.9, 0, 500)
    r = rng(23)

    def tree(d, x, base, h, shade):
        w = h * 0.34
        d.rectangle([(x - h * 0.012) * SS, (base - h * 0.2) * SS, (x + h * 0.012) * SS, base * SS], fill=shade)
        tiers = 9
        for k in range(tiers):
            t = k / tiers
            y_top = base - h + t * h * 0.86
            y_bot = y_top + h * 0.2 + t * h * 0.05
            half = w * (0.08 + t * 0.92) / 2 * (0.86 + 0.28 * r.random())
            skew = (r.random() - 0.5) * half * 0.3
            d.polygon([(x * SS, y_top * SS), ((x + half + skew) * SS, y_bot * SS), ((x + half * 0.35) * SS, (y_bot - h * 0.03) * SS), (x * SS, (y_bot + h * 0.012) * SS), ((x - half * 0.35) * SS, (y_bot - h * 0.03) * SS), ((x - half + skew) * SS, y_bot * SS)], fill=shade)

    rows = [(430, 130, 0.78, 16), (520, 190, 0.60, 12), (620, 270, 0.40, 9), (730, 380, 0.20, 6), (850, 560, 0.0, 4)]
    for base, h, shade, n in rows:
        layer = Image.new("L", img.size, 0)
        ld = ImageDraw.Draw(layer)
        xs = np.linspace(-30, W + 30, n) + (r.random(n) - 0.5) * (W / n) * 0.9
        prof = fbm1(n, int(base), octaves=2, base=2)
        for i, x in enumerate(xs):
            tree(ld, x, base + 30 * prof[i], h * (0.7 + 0.5 * r.random()), 255)
        if base + 6 < H:
            ld.rectangle([0, (base + 6) * SS, W * SS, H * SS], fill=255)
        tone_img = Image.new("L", img.size, int(shade * 255))
        img.paste(tone_img, (0, 0), layer)
        # Mist settles between the rows.
        mist = np.clip(1 - np.abs(np.arange(H * SS) / SS - (base + 10)) / 70, 0, 1) ** 2 * (0.5 if shade > 0.1 else 0)
        arr = np.asarray(img, dtype=np.float32) / 255
        arr = arr + (1 - arr) * mist[:, None]
        img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    d.ellipse([70 * SS, 90 * SS, 150 * SS, 170 * SS], fill=255, outline=120, width=3)
    return dither(down(img))


def chart():
    """An invented island, drawn the way a sea chart draws one."""
    f = fbm2(W, H, 77, octaves=5, base=2)
    yy, xx = np.mgrid[0:H, 0:W].astype(np.float32)
    fall = np.hypot((xx - 250) / 250, (yy - 400) / 400)
    h = f * 1.25 - fall * 0.85 + 0.12  # above ~0.5 is land
    img = Image.new("L", (W, H), 255)
    bits = np.ones((H, W), dtype=bool)
    land = h > 0.5

    def iso(level):
        m = h > level
        e = np.zeros_like(m)
        e[1:, :] |= m[1:, :] != m[:-1, :]
        e[:, 1:] |= m[:, 1:] != m[:, :-1]
        return e

    # Depth contours as dotted lines, land contours solid, the coast heavy.
    for i, level in enumerate([0.26, 0.34, 0.42]):
        e = iso(level)
        dots = ((xx + yy) % (5 - i)) < 1.5
        bits[e & dots] = False
    for level in np.arange(0.58, 1.2, 0.07):
        bits[iso(level)] = False
    coast = iso(0.5)
    thick = np.asarray(Image.fromarray((coast * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))) > 0
    bits[thick] = False
    # Shallows stippled, more densely toward the shore.
    r = rng(5)
    shallow = (h > 0.42) & ~land
    bits[shallow & (r.random((H, W)) < 0.10)] = False
    im = to_img(bits)
    d = ImageDraw.Draw(im)
    # Graticule and a border with ticks.
    for gx in range(60, W, 120):
        for y in range(0, H, 6):
            if not land[y, gx]:
                d.point((gx, y), fill=0)
    for gy in range(100, H, 150):
        for x in range(0, W, 6):
            if not land[gy, x]:
                d.point((x, gy), fill=0)
    d.rectangle([10, 10, W - 11, H - 11], outline=0, width=2)
    d.rectangle([16, 16, W - 17, H - 17], outline=0, width=1)
    for k in range(16, W - 16, 12):
        d.line([(k, 10), (k, 16 if (k // 12) % 5 else 22)], fill=0)
        d.line([(k, H - 11), (k, H - 17 if (k // 12) % 5 else H - 23)], fill=0)
    for k in range(16, H - 16, 12):
        d.line([(10, k), (16 if (k // 12) % 5 else 22, k)], fill=0)
        d.line([(W - 11, k), (W - 17 if (k // 12) % 5 else W - 23, k)], fill=0)
    # A compass rose in open water.
    cx, cy, R = 96, 668, 58
    d.ellipse([cx - R, cy - R, cx + R, cy + R], fill=255, outline=0, width=2)
    d.ellipse([cx - R + 7, cy - R + 7, cx + R - 7, cy + R - 7], outline=0, width=1)
    for k in range(72):
        a = math.radians(k * 5)
        r0 = R - (7 if k % 2 else 12)
        d.line([(cx + r0 * math.sin(a), cy - r0 * math.cos(a)), (cx + R * math.sin(a), cy - R * math.cos(a))], fill=0)
    for k in range(8):
        a = math.radians(k * 45)
        ln = (R - 14) if k % 2 == 0 else (R - 30)
        tip = (cx + ln * math.sin(a), cy - ln * math.cos(a))
        l = (cx + 7 * math.sin(a - math.pi / 2), cy - 7 * math.cos(a - math.pi / 2))
        rgt = (cx + 7 * math.sin(a + math.pi / 2), cy - 7 * math.cos(a + math.pi / 2))
        d.polygon([tip, l, (cx, cy)], fill=0)
        d.polygon([tip, rgt, (cx, cy)], fill=255, outline=0)
    # Soundings: small crosses in the deeps.
    for _ in range(40):
        x, y = int(r.integers(30, W - 30)), int(r.integers(30, H - 30))
        if h[y, x] < 0.36 and math.hypot(x - cx, y - cy) > R + 12:
            d.line([(x - 2, y), (x + 2, y)], fill=0)
            d.line([(x, y - 2), (x, y + 2)], fill=0)
    return np.asarray(im, dtype=bool)


def startrails():
    img = new(0.0)
    d = ImageDraw.Draw(img)
    r = rng(12)
    px, py = 300, 300  # the pole
    for _ in range(620):
        rad = 6 + r.random() ** 0.75 * 780
        a0 = r.random() * 360
        sweep = 46 + r.random() * 8
        shade = int(120 + r.random() * 135)
        wd = 3 if r.random() < 0.75 else 6
        d.arc([(px - rad) * SS, (py - rad) * SS, (px + rad) * SS, (py + rad) * SS], a0, a0 + sweep, fill=shade, width=wd)
    # A ridge and a lone tree against the sky.
    prof = fbm1(220, 55, octaves=6, base=3)
    d.polygon(ridge_poly(prof, 790, 190), fill=0)
    pts = ridge_poly(prof, 790, 190)[:-2]
    d.line(pts, fill=150, width=3)
    small = down(img)
    bits = dither(small)
    im = to_img(bits)
    d = ImageDraw.Draw(im)
    # A tent glowing at the foot of the ridge.
    tx, ty = 130, 742
    d.polygon([(tx - 30, ty), (tx, ty - 34), (tx + 30, ty)], fill=255)
    d.line([(tx, ty - 34), (tx, ty)], fill=0, width=1)
    d.polygon([(tx - 6, ty), (tx, ty - 16), (tx + 6, ty)], fill=0)
    return np.asarray(im, dtype=bool)


def patchbay():
    """A synth front panel: knobs at their settings, sliders, jacks and patch cables."""
    img = new(0.0)
    d = ImageDraw.Draw(img)
    r = rng(808)

    def knob(x, y, rad, ang):
        for k in range(11):  # the scale round it
            a = math.radians(-225 + k * 27)
            d.line([((x + (rad + 5) * math.cos(a)) * SS, (y + (rad + 5) * math.sin(a)) * SS), ((x + (rad + 10) * math.cos(a)) * SS, (y + (rad + 10) * math.sin(a)) * SS)], fill=255, width=3)
        d.ellipse([(x - rad) * SS, (y - rad) * SS, (x + rad) * SS, (y + rad) * SS], fill=255)
        d.ellipse([(x - rad + 5) * SS, (y - rad + 5) * SS, (x + rad - 5) * SS, (y + rad - 5) * SS], fill=0)
        d.ellipse([(x - rad + 8) * SS, (y - rad + 8) * SS, (x + rad - 8) * SS, (y + rad - 8) * SS], fill=200)
        a = math.radians(-225 + ang * 270)
        d.line([(x * SS, y * SS), ((x + (rad - 6) * math.cos(a)) * SS, (y + (rad - 6) * math.sin(a)) * SS)], fill=0, width=9)

    def label(x, y, w):
        d.rectangle([(x - w / 2) * SS, y * SS, (x + w / 2) * SS, (y + 3) * SS], fill=255)

    d.rectangle([14 * SS, 14 * SS, (W - 14) * SS, (H - 14) * SS], outline=255, width=5)
    for sx, sy in [(30, 30), (W - 30, 30), (30, H - 30), (W - 30, H - 30)]:  # panel screws
        d.ellipse([(sx - 7) * SS, (sy - 7) * SS, (sx + 7) * SS, (sy + 7) * SS], outline=255, width=4)
        d.line([((sx - 5) * SS, (sy - 5) * SS), ((sx + 5) * SS, (sy + 5) * SS)], fill=255, width=3)
    # A small scope at the top.
    d.rounded_rectangle([44 * SS, 54 * SS, 436 * SS, 190 * SS], radius=20, outline=255, width=5)
    pts = [((54 + i * 3.72) * SS, (122 + 44 * math.sin(i / 8.5) * math.sin(i / 31) ) * SS) for i in range(101)]
    d.line(pts, fill=255, width=7, joint="curve")
    # Three rows of knobs.
    for row, y in enumerate([262, 366, 470]):
        n = 4 if row != 1 else 5
        for k in range(n):
            x = 78 + k * (324 / (n - 1))
            knob(x, y, 30 if n == 4 else 24, r.random())
            label(x, y + 46, 34)
    # Sliders.
    for k in range(6):
        x = 66 + k * 44
        d.rounded_rectangle([(x - 3) * SS, 548 * SS, (x + 3) * SS, 660 * SS], radius=6, outline=255, width=3)
        y = 556 + r.random() * 92
        d.rounded_rectangle([(x - 15) * SS, (y - 8) * SS, (x + 15) * SS, (y + 8) * SS], radius=6, fill=255)
        d.line([((x - 15) * SS, y * SS), ((x + 15) * SS, y * SS)], fill=0, width=4)
    # A step row of lamps.
    for k in range(8):
        x, y = 318 + (k % 4) * 34, 572 + (k // 4) * 40
        on = k in (0, 3, 5)
        d.rounded_rectangle([(x - 12) * SS, (y - 12) * SS, (x + 12) * SS, (y + 12) * SS], radius=8, outline=255, width=4, fill=255 if on else 0)
    # Jacks along the bottom, and cables sagging between some of them.
    jacks = [(60 + k * 51.5, y) for y in (716, 760) for k in range(8)]
    for x, y in jacks:
        d.ellipse([(x - 13) * SS, (y - 13) * SS, (x + 13) * SS, (y + 13) * SS], outline=255, width=5)
        d.ellipse([(x - 5) * SS, (y - 5) * SS, (x + 5) * SS, (y + 5) * SS], fill=255)
    for a, b, sag in [(0, 5, 34), (2, 12, 26), (7, 9, 14), (3, 14, 30)]:
        (x0, y0), (x1, y1) = jacks[a], jacks[b]
        pts = []
        for i in range(61):
            t = i / 60
            pts.append(((x0 + (x1 - x0) * t) * SS, (y0 + (y1 - y0) * t - sag * 4 * t * (1 - t)) * SS))
        d.line(pts, fill=0, width=20, joint="curve")
        d.line(pts, fill=255, width=11, joint="curve")
        for x, y in (jacks[a], jacks[b]):
            d.ellipse([(x - 11) * SS, (y - 11) * SS, (x + 11) * SS, (y + 11) * SS], fill=255)
            d.ellipse([(x - 5) * SS, (y - 5) * SS, (x + 5) * SS, (y + 5) * SS], fill=0)
    return dither(down(img), "bayer")


def vinyl():
    yy, xx = np.mgrid[0 : H * SS, 0 : W * SS].astype(np.float32) / SS
    cx, cy = 240, 420
    r = np.hypot(xx - cx, yy - cy)
    a = np.arctan2(yy - cy, xx - cx)
    arr = np.full((H * SS, W * SS), 0.93, dtype=np.float32)
    disc = r < 226
    # Grooves, with the two bright wedges light makes across a record.
    groove = 0.5 + 0.5 * np.sin(r * 2.1)
    sheen = np.clip(np.cos(2 * (a - 0.9)), 0, 1) ** 6
    tone = 0.02 + 0.10 * groove + sheen * (0.55 + 0.3 * groove)
    for gap in (118, 158, 196):  # the gaps between tracks
        tone = np.where(np.abs(r - gap) < 1.6, 0.0, tone)
    arr = np.where(disc, tone, arr)
    arr = np.where(r < 78, 1.0, arr)  # label
    arr = np.where((r < 78) & (r > 72), 0.0, arr)
    arr = np.where((r < 52) & (r > 50), 0.0, arr)
    arr = np.where(r < 7, 0.0, arr)
    # Label lettering stands in as arcs of dashes.
    dash = ((a * 12) % 1.0 < 0.62) & (np.abs(r - 62) < 2.2) & (np.sin(a) < 0.2)
    arr = np.where(dash, 0.0, arr)
    block = (np.abs(xx - cx) < 26) & (yy - cy > 22) & (yy - cy < 30)
    arr = np.where(block, 0.0, arr)
    # The disc's shadow on the table.
    sh = np.hypot(xx - cx - 10, yy - cy - 14)
    arr = np.where(~disc & (sh < 232), 0.55 + 0.38 * np.clip((sh - 214) / 18, 0, 1), arr)
    img = Image.fromarray((np.clip(arr, 0, 1) * 255).astype(np.uint8))
    d = ImageDraw.Draw(img)
    # The tone arm coming in from the top right.
    d.line([(446 * SS, 60 * SS), (372 * SS, 330 * SS)], fill=0, width=18)
    d.line([(446 * SS, 60 * SS), (372 * SS, 330 * SS)], fill=255, width=8)
    d.ellipse([(446 - 26) * SS, (60 - 26) * SS, (446 + 26) * SS, (60 + 26) * SS], fill=0)
    d.ellipse([(446 - 10) * SS, (60 - 10) * SS, (446 + 10) * SS, (60 + 10) * SS], fill=255)
    d.polygon([(356 * SS, 322 * SS), (386 * SS, 332 * SS), (372 * SS, 376 * SS), (342 * SS, 366 * SS)], fill=0)
    return dither(down(img))


PIECES = [
    ("sticky-moonrise", "Moonrise", moonrise),
    ("sticky-swell", "Night Swell", swell),
    ("sticky-regatta", "Regatta", regatta),
    ("sticky-dunes", "Dunes", dunes),
    ("sticky-summit", "Summit", summit),
    ("sticky-orbit", "Orbit", orbit),
    ("sticky-lighthouse", "Lighthouse", lighthouse),
    ("sticky-eixample", "Eixample", eixample),
    ("sticky-outrun", "Outrun", outrun),
    ("sticky-lissajous", "Lissajous", lissajous),
    ("sticky-jellyfish", "Jellyfish", jellyfish),
    ("sticky-pines", "Pines", pines),
    ("sticky-chart", "Sea Chart", chart),
    ("sticky-startrails", "Star Trails", startrails),
    ("sticky-patchbay", "Patchbay", patchbay),
    ("sticky-vinyl", "Vinyl", vinyl),
]
STEMS = [p[0] for p in PIECES]


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    only = [a for a in sys.argv[2:] if not a.startswith("--") and not a.endswith(".png")]
    sheet = next((a for a in sys.argv[2:] if a.endswith(".png")), None)
    made = []
    for stem, _name, fn in PIECES:
        if only and stem.split("-", 1)[1] not in only:
            continue
        bits = fn()
        assert bits.shape == (H, W), stem
        im = to_img(bits)
        im.save(out / f"{stem}.bmp")
        size = (out / f"{stem}.bmp").stat().st_size
        assert size == 48062, (stem, size)
        made.append((stem, im))
    if not only:
        with open(out / "sticky.dat", "wb") as pack:
            for stem in STEMS:
                pack.write((out / f"{stem}.bmp").read_bytes())
    if sheet:
        cols = min(6, len(made))
        rows = (len(made) + cols - 1) // cols
        s = Image.new("L", (cols * 244 + 4, rows * 404 + 4), 128)
        for i, (_stem, im) in enumerate(made):
            s.paste(im.convert("L").resize((240, 400), Image.LANCZOS), (4 + (i % cols) * 244, 4 + (i // cols) * 404))
        s.save(sheet)


if __name__ == "__main__":
    main()
