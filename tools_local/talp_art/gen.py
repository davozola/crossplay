#!/usr/bin/env python3
"""Draws EL TALP's pictures and writes them as run-length rows.

Every picture is a whole cell: a hole in a meadow, empty or with somebody in
it, awake or just bopped. Drawn at four times the size in two layers -- line
art, which is thresholded so its edges stay clean, and flat tones, which
become fixed dither patterns -- then cut down to the panel's one bit.

  python3 tools_local/talp_art/gen.py            # writes MoleArt.h
  python3 tools_local/talp_art/gen.py --sheet x.png   # and a contact sheet
"""
import math
import sys
from PIL import Image, ImageDraw

SS = 4
U = 208  # the design grid; a cell is U x U


class Canvas:
    def __init__(self, size):
        self.size = size
        self.k = size / U * SS
        n = size * SS
        self.ink = Image.new("L", (n, n), 255)
        self.tone = Image.new("L", (n, n), 0)
        self.di = ImageDraw.Draw(self.ink)
        self.dt = ImageDraw.Draw(self.tone)

    def s(self, v):
        return v * self.k

    def box(self, cx, cy, rx, ry):
        return [self.s(cx - rx), self.s(cy - ry), self.s(cx + rx), self.s(cy + ry)]

    # fill: 'w' white, 'k' black, or a tone 1..3 (light..dark)
    def _fill(self, shape, fill):
        if fill == "k":
            shape(self.di, 0)
            shape(self.dt, 0)
        elif fill == "w":
            shape(self.di, 255)
            shape(self.dt, 0)
        else:
            shape(self.di, 255)
            shape(self.dt, fill)

    def ellipse(self, cx, cy, rx, ry, fill="w", line=0):
        if line:
            self._fill(lambda d, v: d.ellipse(self.box(cx, cy, rx, ry), fill=v), "k")
            rx, ry = rx - line, ry - line
        if rx > 0 and ry > 0:
            self._fill(lambda d, v: d.ellipse(self.box(cx, cy, rx, ry), fill=v), fill)

    def poly(self, pts, fill="w"):
        p = [(self.s(x), self.s(y)) for x, y in pts]
        self._fill(lambda d, v: d.polygon(p, fill=v), fill)

    def blob(self, pts, fill="w", line=0):
        """A closed smooth shape through pts (Catmull-Rom), optionally outlined."""
        sm = smooth(pts)
        if line:
            self.poly(sm, "k")
            cx = sum(x for x, _ in sm) / len(sm)
            cy = sum(y for _, y in sm) / len(sm)
            inner = []
            for x, y in sm:
                d = math.hypot(x - cx, y - cy) or 1
                inner.append((x - (x - cx) / d * line, y - (y - cy) / d * line))
            self.poly(inner, fill)
        else:
            self.poly(sm, fill)

    def line(self, pts, w, fill="k", closed=False):
        p = [(self.s(x), self.s(y)) for x, y in pts]
        if closed:
            p.append(p[0])
        v = 0 if fill == "k" else 255
        self.di.line(p, fill=v, width=max(1, int(round(self.s(w)))), joint="curve")
        r = self.s(w) / 2
        for x, y in p:
            self.di.ellipse([x - r, y - r, x + r, y + r], fill=v)
        if fill == "w":
            self.dt.line(p, fill=0, width=max(1, int(round(self.s(w)))))
            for x, y in p:
                self.dt.ellipse([x - r, y - r, x + r, y + r], fill=0)

    def arc(self, cx, cy, rx, ry, a0, a1, w, fill="k"):
        n = 24
        pts = [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * i / n)),
                cy + ry * math.sin(math.radians(a0 + (a1 - a0) * i / n))) for i in range(n + 1)]
        self.line(pts, w, fill)

    def star(self, cx, cy, r, fill="k", rot=0):
        pts = []
        for i in range(10):
            a = math.radians(-90 + rot + i * 36)
            rr = r if i % 2 == 0 else r * 0.45
            pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
        self.poly(pts, fill)

    def finish(self):
        n = self.size
        ink = self.ink.resize((n, n), Image.LANCZOS)
        tone = self.tone.resize((n, n), Image.NEAREST)
        out = Image.new("1", (n, n), 1)
        ip, tp, op = ink.load(), tone.load(), out.load()
        for y in range(n):
            for x in range(n):
                if ip[x, y] < 128:
                    op[x, y] = 0
                else:
                    t = tp[x, y]
                    black = False
                    if t == 1:  # light: one in eight, staggered
                        black = (x % 4 == 0 and y % 4 == 0) or (x % 4 == 2 and y % 4 == 2)
                    elif t == 2:  # mid: one in four, staggered
                        black = (x + y) % 2 == 0 and y % 2 == 0 or ((x % 4 == 1) and (y % 4 == 1)) or ((x % 4 == 3) and (y % 4 == 3))
                    elif t == 3:  # dark: checker
                        black = (x + y) % 2 == 0
                    op[x, y] = 0 if black else 1
        return out


def smooth(pts, per=10):
    n = len(pts)
    out = []
    for i in range(n):
        p0, p1, p2, p3 = pts[(i - 1) % n], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        for j in range(per):
            t = j / per
            t2, t3 = t * t, t * t * t
            out.append(tuple(0.5 * ((2 * p1[k]) + (-p0[k] + p2[k]) * t + (2 * p0[k] - 5 * p1[k] + 4 * p2[k] - p3[k]) * t2 +
                                    (-p0[k] + 3 * p1[k] - 3 * p2[k] + p3[k]) * t3) for k in (0, 1)))
    return out


HX, HY, HRX, HRY = 104, 160, 80, 21  # the hole
LW = 5  # outline weight


def hole_back(c):
    c.ellipse(HX, HY, HRX, HRY, "k")


def front(c):
    """Everything in front of whoever is in the hole: the near ground, the
    mound, the grass. Drawn after the animal, hiding its lower half."""
    n = c.size * SS
    # Clear what lies outside the hole below its middle.
    for layer, v in ((c.ink, 255), (c.tone, 0)):
        px = layer.load()
        for y in range(int(c.s(HY)), n):
            uy = y / c.k
            for x in range(n):
                ux = x / c.k
                inside = ((ux - HX) / HRX) ** 2 + ((uy - HY) / HRY) ** 2 <= 1
                if not inside:
                    px[x, y] = v
    # The mound: a lip of earth along the near edge.
    lip = [(HX - HRX - 12, HY + 2), (HX - HRX - 2, HY - 7), (HX - HRX + 6, HY + 3)]
    for i in range(0, 13):
        a = math.radians(180 - i * 15)
        lip.append((HX + (HRX + 1) * math.cos(a), HY + (HRY + 1) * math.sin(a)))
    lip += [(HX + HRX + 3, HY - 7), (HX + HRX + 13, HY + 3), (HX + HRX + 6, HY + 17), (HX + 58, HY + 33),
            (HX + 22, HY + 40), (HX - 24, HY + 40), (HX - 60, HY + 33), (HX - HRX - 6, HY + 17)]
    c.poly(lip, "k")
    inner = [(HX - HRX - 5, HY + 4)]
    for i in range(0, 13):
        a = math.radians(180 - i * 15)
        inner.append((HX + (HRX + 6) * math.cos(a), HY + (HRY + 6) * math.sin(a)))
    inner += [(HX + HRX + 6, HY + 5), (HX + HRX + 1, HY + 14), (HX + 56, HY + 28), (HX + 22, HY + 35),
              (HX - 24, HY + 35), (HX - 58, HY + 28), (HX - HRX - 1, HY + 14)]
    c.poly(inner, 2)
    # Clods and pebbles on it.
    for x, y, r in ((HX - 52, HY + 24, 4), (HX - 18, HY + 31, 3), (HX + 30, HY + 29, 4.5), (HX + 60, HY + 20, 3),
                    (HX - 70, HY + 13, 2.5), (HX + 6, HY + 30, 2)):
        c.ellipse(x, y, r, r * 0.8, "w", 1.6)
    # Grass at both ends, and a flower.
    for gx, flip in ((HX - HRX - 14, -1), (HX + HRX + 16, 1)):
        base = HY + 6
        for dx, h, lean in ((-9, 20, -6), (-2, 30, -1), (5, 24, 5), (11, 15, 9)):
            c.line([(gx + dx * flip, base), (gx + (dx + lean * 0.4) * flip, base - h * 0.6),
                    (gx + (dx + lean) * flip, base - h)], 3.2)
    fx, fy = HX + HRX + 2, HY + 34
    c.line([(fx, fy + 12), (fx + 1, fy)], 2.6)
    for i in range(5):
        a = math.radians(i * 72 - 90)
        c.ellipse(fx + 6.5 * math.cos(a), fy - 4 + 6.5 * math.sin(a), 4.6, 4.6, "w", 1.7)
    c.ellipse(fx, fy - 4, 3, 3, "k")
    # The ground line beyond the mound.
    c.line([(2, HY + 8), (HX - HRX - 26, HY + 8)], 2.6)
    c.line([(HX + HRX + 30, HY + 8), (U - 2, HY + 8)], 2.6)


def eyes(c, cx, cy, gap, r, bopped, lid=False):
    for side in (-1, 1):
        ex = cx + side * gap
        if bopped:
            d = r * 0.85
            c.line([(ex - d, cy - d), (ex + d, cy + d)], 9.5, "w")
            c.line([(ex - d, cy + d), (ex + d, cy - d)], 9.5, "w")
            c.line([(ex - d, cy - d), (ex + d, cy + d)], 4.4)
            c.line([(ex - d, cy + d), (ex + d, cy - d)], 4.4)
        else:
            c.ellipse(ex, cy, r, r * 1.15, "w", 3.2)
            c.ellipse(ex + side * -1.5, cy + 1.5, r * 0.55, r * 0.62, "k")
            c.ellipse(ex + side * -1.5 - r * 0.2, cy - r * 0.2, r * 0.2, r * 0.2, "w")


def dizzy(c, cx, top):
    """Stars going round above a bopped head."""
    top -= 10
    c.arc(cx, top - 6, 46, 11, 200, 340, 2.6)
    c.arc(cx, top - 6, 46, 11, 20, 160, 2.6)
    for x, y, r, rot in ((cx - 44, top - 12, 11, -12), (cx + 2, top - 22, 14, 8), (cx + 46, top - 8, 10, 20)):
        c.star(x, y, r + 2.6, "k", rot)
        c.star(x, y, r - 0.6, "w", rot)


def paws(c, fill, claws=True):
    for side in (-1, 1):
        px = HX + side * 50
        c.ellipse(px, HY + 14, 15, 10, fill, 4)
        if claws:
            for dx in (-7, 0, 7):
                c.line([(px + dx, HY + 16), (px + dx, HY + 23)], 2.4)


def mole(c, bopped):
    drop = 20 if bopped else 0
    top = 44 + drop
    # Body: a dark dome.
    c.blob([(HX, top), (HX + 44, top + 14), (HX + 60, top + 62), (HX + 60, 215), (HX - 60, 215), (HX - 60, top + 62),
            (HX - 44, top + 14)], 3, LW)
    # Ears.
    for side in (-1, 1):
        c.ellipse(HX + side * 47, top + 22, 9, 10, 3, 3.5)
    # Face.
    c.blob([(HX, top + 50), (HX + 34, top + 58), (HX + 38, top + 86), (HX + 20, top + 108), (HX - 20, top + 108),
            (HX - 38, top + 86), (HX - 34, top + 58)], "w", 3.5)
    eyes(c, HX, top + 40, 21, 10, bopped)
    # Nose, big and shiny.
    c.ellipse(HX, top + 64, 15, 11, "k")
    c.ellipse(HX - 5, top + 60, 4, 2.6, "w")
    # Whiskers.
    for side in (-1, 1):
        for dy, lift in ((-4, -7), (4, 0), (12, 8)):
            c.line([(HX + side * 22, top + 72 + dy), (HX + side * 52, top + 70 + dy + lift)], 2.2)
    if bopped:
        # A wobbly mouth with the tongue out.
        c.arc(HX, top + 86, 14, 6, 0, 180, 3.2)
        c.blob([(HX + 2, top + 90), (HX + 12, top + 92), (HX + 12, top + 104), (HX + 6, top + 108),
                (HX, top + 104)], 2, 2.6)
    else:
        # A grin and two teeth.
        c.arc(HX, top + 80, 20, 10, 10, 170, 3.4)
        c.poly([(HX - 9, top + 88), (HX + 9, top + 88), (HX + 8, top + 103), (HX - 8, top + 103)], "k")
        c.poly([(HX - 6.5, top + 90), (HX - 0.8, top + 90), (HX - 0.8, top + 100.5), (HX - 6, top + 100.5)], "w")
        c.poly([(HX + 0.8, top + 90), (HX + 6.5, top + 90), (HX + 6, top + 100.5), (HX + 0.8, top + 100.5)], "w")
    return top


def rabbit(c, bopped):
    drop = 20 if bopped else 0
    top = 74 + drop
    # Ears first, behind the head: one up, one bent when bopped.
    for side in (-1, 1):
        if bopped:
            # Both ears flopped out sideways.
            m = lambda pts: [(HX + side * (x - HX), y) for x, y in pts]
            c.blob(m([(HX + 16, top + 14), (HX + 30, top - 8), (HX + 62, top - 6), (HX + 84, top + 16),
                      (HX + 78, top + 30), (HX + 56, top + 18), (HX + 34, top + 24)]), "w", LW)
            c.blob(m([(HX + 34, top + 8), (HX + 46, top + 1), (HX + 62, top + 4), (HX + 72, top + 16),
                      (HX + 56, top + 10)]), 2)
        else:
            ex = HX + side * 22
            lean = side * 7
            c.blob([(ex - 13, top + 10), (ex - 15 + lean, top - 38), (ex + lean * 1.6, top - 70),
                    (ex + 15 + lean, top - 38), (ex + 13, top + 10)], "w", LW)
            c.blob([(ex - 5, top + 2), (ex - 6 + lean, top - 36), (ex + lean * 1.5, top - 56),
                    (ex + 6 + lean, top - 36), (ex + 5, top + 2)], 2)
    # Head and shoulders.
    c.blob([(HX, top), (HX + 40, top + 8), (HX + 56, top + 40), (HX + 48, top + 76), (HX + 56, 215), (HX - 56, 215),
            (HX - 48, top + 76), (HX - 56, top + 40), (HX - 40, top + 8)], "w", LW)
    eyes(c, HX, top + 32, 21, 10.5, bopped)
    # Cheeks.
    for side in (-1, 1):
        c.ellipse(HX + side * 37, top + 53, 9, 6, 1)
    # Nose and mouth.
    c.poly([(HX - 7, top + 48), (HX + 7, top + 48), (HX, top + 57)], "k")
    c.line([(HX, top + 57), (HX, top + 63)], 3)
    if bopped:
        c.ellipse(HX, top + 72, 9, 8, "k")
        c.ellipse(HX, top + 75, 5, 4, 2)
    else:
        c.arc(HX - 9, top + 62, 9, 7, 0, 180, 3)
        c.arc(HX + 9, top + 62, 9, 7, 0, 180, 3)
        c.poly([(HX - 7, top + 68), (HX + 7, top + 68), (HX + 6, top + 82), (HX - 6, top + 82)], "k")
        c.poly([(HX - 4.6, top + 69), (HX - 0.7, top + 69), (HX - 0.7, top + 79.5), (HX - 4.2, top + 79.5)], "w")
        c.poly([(HX + 0.7, top + 69), (HX + 4.6, top + 69), (HX + 4.2, top + 79.5), (HX + 0.7, top + 79.5)], "w")
    for side in (-1, 1):
        for dy, lift in ((0, -6), (7, 2)):
            c.line([(HX + side * 20, top + 60 + dy), (HX + side * 50, top + 58 + dy + lift)], 2)
    return top - (4 if bopped else 62)


def frog(c, bopped):
    drop = 18 if bopped else 0
    top = 70 + drop
    # Head: wide and low.
    c.blob([(HX, top + 10), (HX + 46, top + 12), (HX + 70, top + 46), (HX + 62, top + 84), (HX + 62, 215),
            (HX - 62, 215), (HX - 62, top + 84), (HX - 70, top + 46), (HX - 46, top + 12)], 1, LW)
    # Eye bumps on top.
    for side in (-1, 1):
        ex = HX + side * 34
        c.ellipse(ex, top + 8, 25, 25, 1, LW)
        if bopped:
            d = 10
            c.line([(ex - d, top + 8 - d), (ex + d, top + 8 + d)], 4.6)
            c.line([(ex - d, top + 8 + d), (ex + d, top + 8 - d)], 4.6)
        else:
            c.ellipse(ex, top + 8, 16, 16, "w", 3)
            c.ellipse(ex + side * -3, top + 10, 8.5, 9.5, "k")
            c.ellipse(ex + side * -3 - 3, top + 6, 3, 3, "w")
    # Erase the outline where the bumps meet the head.
    c.ellipse(HX - 34, top + 26, 17, 8, 1)
    c.ellipse(HX + 34, top + 26, 17, 8, 1)
    # Nostrils, cheeks, belly spots.
    c.ellipse(HX - 7, top + 44, 2.4, 2.4, "k")
    c.ellipse(HX + 7, top + 44, 2.4, 2.4, "k")
    for side in (-1, 1):
        c.ellipse(HX + side * 50, top + 56, 8, 5.5, 3)
    if bopped:
        c.arc(HX, top + 70, 30, 9, 180, 360, 3.6)
        c.blob([(HX + 4, top + 66), (HX + 22, top + 66), (HX + 24, top + 86), (HX + 13, top + 94),
                (HX + 2, top + 86)], 2, 2.8)
    else:
        # The widest smile on the meadow.
        c.arc(HX, top + 50, 52, 24, 12, 168, 4)
        c.arc(HX - 51, top + 54, 6, 6, 200, 330, 3)
        c.arc(HX + 51, top + 54, 6, 6, 210, 340, 3)
    return top - (18 if bopped else 18)


def chick(c, bopped):
    drop = 20 if bopped else 0
    top = 58 + drop
    # Tuft.
    for dx, h, lean in ((-8, 20, -9), (0, 27, 0), (8, 20, 9)):
        c.blob([(HX + dx - 5, top + 8), (HX + dx + lean - 2, top - h), (HX + dx + lean + 4, top - h + 2),
                (HX + dx + 6, top + 8)], "w", 3.4)
    # Round body.
    c.blob([(HX, top), (HX + 42, top + 10), (HX + 60, top + 48), (HX + 54, top + 88), (HX + 58, 215), (HX - 58, 215),
            (HX - 54, top + 88), (HX - 60, top + 48), (HX - 42, top + 10)], "w", LW)
    # Down: a few flecks.
    for x, y in ((HX - 40, top + 30), (HX + 42, top + 34), (HX - 46, top + 74), (HX + 46, top + 72),
                 (HX - 14, top + 16), (HX + 18, top + 14)):
        c.arc(x, y, 5, 4, 200, 340, 2)
    eyes(c, HX, top + 38, 23, 10, bopped)
    for side in (-1, 1):
        c.ellipse(HX + side * 38, top + 60, 9, 6, 1)
    # Beak.
    if bopped:
        c.poly([(HX - 15, top + 54), (HX + 15, top + 54), (HX + 6, top + 66), (HX - 6, top + 66)], "k")
        c.poly([(HX - 9, top + 57), (HX + 9, top + 57), (HX + 4, top + 63), (HX - 4, top + 63)], 2)
        c.poly([(HX - 10, top + 68), (HX + 10, top + 68), (HX, top + 84)], "k")
        c.poly([(HX - 5.5, top + 70.5), (HX + 5.5, top + 70.5), (HX, top + 79)], 2)
    else:
        c.poly([(HX - 16, top + 54), (HX + 16, top + 54), (HX, top + 78)], "k")
        c.poly([(HX - 10, top + 57.5), (HX + 10, top + 57.5), (HX, top + 72)], 2)
        c.line([(HX - 11, top + 62), (HX + 11, top + 62)], 2.4)
    return top - (22 if bopped else 24)


ANIMALS = [("talp", mole, 3), ("conill", rabbit, "w"), ("granota", frog, 1), ("pollet", chick, "w")]


def cell(size, animal=None, bopped=False):
    c = Canvas(size)
    hole_back(c)
    top = None
    if animal is not None:
        top = ANIMALS[animal][1](c, bopped)
    front(c)
    if animal is not None:
        paws(c, ANIMALS[animal][2], claws=animal == 0)
        if bopped:
            dizzy(c, HX, top)
    return c.finish()


def rows(img):
    """Each row as runs: white, black, white ... as bytes; 255 continues."""
    px = img.load()
    out = bytearray()
    for y in range(img.height):
        colour, run = 1, 0
        row = bytearray()
        for x in range(img.width):
            v = 1 if px[x, y] else 0
            if v == colour:
                run += 1
            else:
                while run >= 255:
                    row += bytes([255, 0])
                    run -= 255
                row.append(run)
                colour, run = v, 1
        # The last run is implied by the width when it is white.
        if colour == 0:
            while run >= 255:
                row += bytes([255, 0])
                run -= 255
            row.append(run)
        out.append(len(row))
        assert len(row) < 256
        out += row
    return bytes(out)


def main():
    sizes = (208, 144)
    pics = []  # (name, size, image)
    for size in sizes:
        pics.append((f"forat_{size}", size, cell(size)))
        for i, (name, _, _) in enumerate(ANIMALS):
            pics.append((f"{name}_{size}", size, cell(size, i, False)))
            pics.append((f"{name}_pam_{size}", size, cell(size, i, True)))
    icon = ("talp_96", 96, cell(96, 0, False))
    if "--sheet" in sys.argv:
        path = sys.argv[sys.argv.index("--sheet") + 1]
        sheet = Image.new("1", (5 * 216, 2 * 216 + 2 * 152), 1)
        for i, (_, size, img) in enumerate(pics[:9]):
            sheet.paste(img, ((i % 5) * 216, (i // 5) * 216))
        for i, (_, size, img) in enumerate(pics[9:]):
            sheet.paste(img, ((i % 5) * 216, 432 + (i // 5) * 152))
        sheet.save(path)
    total = 0
    with open(sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else "MoleArt.h", "w") as f:
        f.write("#pragma once\n\n// EL TALP's pictures. Generated by tools_local/talp_art/gen.py; do not edit.\n"
                "//\n// Each is a whole square cell. A row is a count byte and then that many run\n"
                "// lengths, white first and alternating; a row's last white run is left off.\n// A run of 255 is "
                "followed by 0 and carries on in the same colour.\n\n#include <cstdint>\n\nnamespace moleart {\n\n"
                "struct Picture {\n  const uint8_t* rows;\n  uint16_t size;\n};\n\n")
        for name, size, img in pics + [icon]:
            data = rows(img)
            total += len(data)
            f.write(f"inline constexpr uint8_t {name}_rows[] = {{\n")
            for i in range(0, len(data), 24):
                f.write("    " + ", ".join(str(b) for b in data[i:i + 24]) + ",\n")
            f.write("};\n")
        f.write("\nconstexpr int kAnimals = 4;\n\n")
        for size in sizes:
            f.write(f"inline constexpr Picture kHole{size} = {{forat_{size}_rows, {size}}};\n")
            f.write(f"inline constexpr Picture kUp{size}[kAnimals] = {{\n")
            for name, _, _ in ANIMALS:
                f.write(f"    {{{name}_{size}_rows, {size}}},\n")
            f.write("};\n")
            f.write(f"inline constexpr Picture kBopped{size}[kAnimals] = {{\n")
            for name, _, _ in ANIMALS:
                f.write(f"    {{{name}_pam_{size}_rows, {size}}},\n")
            f.write("};\n")
        f.write("inline constexpr Picture kIcon = {talp_96_rows, 96};\n")
        f.write("\n}  // namespace moleart\n")
    print("bytes", total)


if __name__ == "__main__":
    main()
