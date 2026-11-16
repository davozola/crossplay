#include "ClockFaces.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxTokens.h"

namespace clockfaces {

namespace {

namespace cc = clockcore;

constexpr float kPi = 3.14159265f;

// The panel as a face sees it.
struct Canvas {
  GfxRenderer& r;
  int w;
  int h;
  bool wide() const { return w > h; }
};

void disc(const Canvas& c, const int cx, const int cy, const int radius, const bool ink = true) {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    c.r.fillRect(cx - half, cy + dy, half * 2 + 1, 1, ink);
  }
}

// A straight stroke with round ends.
void stroke(const Canvas& c, const float x0, const float y0, const float x1, const float y1, const float pen,
            const bool ink = true) {
  const float len = std::hypot(x1 - x0, y1 - y0);
  if (len > 0.5f) {
    const float nx = -(y1 - y0) / len * pen;
    const float ny = (x1 - x0) / len * pen;
    const int xs[4] = {static_cast<int>(std::lround(x0 + nx)), static_cast<int>(std::lround(x1 + nx)),
                       static_cast<int>(std::lround(x1 - nx)), static_cast<int>(std::lround(x0 - nx))};
    const int ys[4] = {static_cast<int>(std::lround(y0 + ny)), static_cast<int>(std::lround(y1 + ny)),
                       static_cast<int>(std::lround(y1 - ny)), static_cast<int>(std::lround(y0 - ny))};
    c.r.fillPolygon(xs, ys, 4, ink);
  }
  disc(c, static_cast<int>(std::lround(x0)), static_cast<int>(std::lround(y0)), static_cast<int>(pen), ink);
  disc(c, static_cast<int>(std::lround(x1)), static_cast<int>(std::lround(y1)), static_cast<int>(pen), ink);
}

// A numeral drawn with a round pen inside the box (x, y, boxW, boxW * 1.6).
// The pen's own width is taken out of the box, so the ink never leaves it.
void numeral(const Canvas& c, const int digit, const float x, const float y, const float boxW, const float pen,
             const bool ink = true) {
  const cc::Stroke* strokes = nullptr;
  const int count = cc::strokesOf(digit, strokes);
  const float sx = boxW - 2 * pen;
  const float sy = (boxW * cc::kNumeralHeight - 2 * pen) / cc::kNumeralHeight;
  const float ox = x + pen;
  const float oy = y + pen;
  // Discs a third of a pen apart read as one smooth stroke.
  const float step = pen / 3 < 1 ? 1 : pen / 3;
  for (int i = 0; i < count; ++i) {
    const cc::Stroke& s = strokes[i];
    if (s.kind == 'L') {
      stroke(c, ox + s.v[0] * sx, oy + s.v[1] * sy, ox + s.v[2] * sx, oy + s.v[3] * sy, pen, ink);
      continue;
    }
    const float rx = s.v[2] * sx;
    const float ry = s.v[3] * sy;
    const float sweep = (s.v[5] - s.v[4]) * kPi / 180;
    const int steps = static_cast<int>(std::fabs(sweep) * (rx > ry ? rx : ry) / step) + 2;
    for (int k = 0; k <= steps; ++k) {
      const float a = s.v[4] * kPi / 180 + sweep * static_cast<float>(k) / static_cast<float>(steps);
      disc(c, static_cast<int>(std::lround(ox + s.v[0] * sx + rx * std::cos(a))),
           static_cast<int>(std::lround(oy + s.v[1] * sy + ry * std::sin(a))), static_cast<int>(pen), ink);
    }
  }
}

// A ring between two radii, from `from` to `to` turns of the dial (0 at twelve
// o'clock, clockwise). `shade` 0 is solid; 1 leaves one pixel in nine, for a
// track that is there without competing.
void ring(const Canvas& c, const int cx, const int cy, const int inner, const int outer, const float from,
          const float to, const int shade) {
  for (int dy = -outer; dy <= outer; ++dy) {
    int runStart = 0;
    bool running = false;
    for (int dx = -outer; dx <= outer + 1; ++dx) {
      bool on = false;
      if (dx <= outer) {
        const int d2 = dx * dx + dy * dy;
        if (d2 <= outer * outer && d2 >= inner * inner) {
          float turn = std::atan2(static_cast<float>(dx), static_cast<float>(-dy)) / (2 * kPi);
          if (turn < 0) turn += 1;
          on = turn >= from && turn <= to;
          if (on && shade == 1) on = ((cx + dx) % 3 == 0) && ((cy + dy) % 3 == 0);
        }
      }
      if (on && !running) {
        running = true;
        runStart = dx;
      } else if (!on && running) {
        running = false;
        c.r.fillRect(cx + runStart, cy + dy, dx - runStart, 1, true);
      }
    }
  }
}

// Text in one of the Toybox cuts, placed by its ink: `top` is where the
// capitals begin. align: -1 left of x, 0 centred on x, 1 right-aligned to x.
void text(const Canvas& c, const int font, const toybox::CutMetrics& cut, const int x, const int top, const char* s,
          const int align = 0, const bool ink = true) {
  const int width = c.r.getTextWidth(font, s);
  const int left = align < 0 ? x : (align == 0 ? x - width / 2 : x - width);
  c.r.drawText(font, left, top - (cut.ascender - cut.inkHeight), s, ink);
}

void dateLine(const Moment& now, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s %d %s", cc::weekdayEs(now.weekday), now.day, cc::monthEs(now.month));
}

// The four numerals of the time, or dashes when the clock has not been set.
struct Digits {
  int d[4];
  bool valid;
};
Digits digitsOf(const Moment& now) {
  return Digits{{now.hour / 10, now.hour % 10, now.minute / 10, now.minute % 10}, now.valid};
}

void dash(const Canvas& c, const float x, const float y, const float boxW, const float pen, const bool ink = true) {
  const float mid = y + boxW * cc::kNumeralHeight / 2;
  stroke(c, x + boxW * 0.2f, mid, x + boxW * 0.8f, mid, pen, ink);
}

// ---------------------------------------------------------------- TRAZO
// The hours heavy, the minutes drawn with a hairline: one numeral face at two
// weights, and nothing else on the page but the date.
void faceTrazo(const Canvas& c, const Moment& now) {
  const Digits t = digitsOf(now);
  char date[32];
  dateLine(now, date, sizeof(date));
  if (c.wide()) {
    const float boxW = 150;
    const float boxH = boxW * cc::kNumeralHeight;
    const float gap = 22;
    const float colon = 56;
    const float total = boxW * 4 + gap * 2 + colon;
    float x = (c.w - total) / 2;
    const float y = (c.h - boxH) / 2 - 24;
    for (int i = 0; i < 4; ++i) {
      const float pen = i < 2 ? boxW * 0.13f : boxW * 0.035f;
      if (t.valid)
        numeral(c, t.d[i], x, y, boxW, pen);
      else
        dash(c, x, y, boxW, pen);
      x += boxW + (i == 1 ? colon : gap);
    }
    if (now.valid) text(c, toybox::kDisplayFontId, toybox::kDisplayCut, c.w / 2, static_cast<int>(y + boxH) + 36, date);
    return;
  }
  const float boxW = 186;
  const float boxH = boxW * cc::kNumeralHeight;
  const float gap = 34;
  const float x0 = (c.w - boxW * 2 - gap) / 2;
  const float y0 = 46;
  for (int i = 0; i < 4; ++i) {
    const float pen = i < 2 ? boxW * 0.13f : boxW * 0.035f;
    const float x = x0 + (i % 2) * (boxW + gap);
    const float y = y0 + (i / 2) * (boxH + 36);
    if (t.valid)
      numeral(c, t.d[i], x, y, boxW, pen);
    else
      dash(c, x, y, boxW, pen);
  }
  if (now.valid) text(c, toybox::kDisplayFontId, toybox::kDisplayCut, c.w / 2, c.h - 80, date);
}

// ---------------------------------------------------------------- PALETAS
// A split-flap board: four dark leaves, each cut across its middle.
void facePaletas(const Canvas& c, const Moment& now) {
  const Digits t = digitsOf(now);
  char date[32];
  dateLine(now, date, sizeof(date));
  const bool wide = c.wide();
  const int cardW = wide ? 168 : 204;
  const int cardH = wide ? 290 : 300;
  const int gap = wide ? 14 : 20;
  const int group = wide ? 44 : 0;  // between the hours and the minutes
  const int left = wide ? (c.w - cardW * 4 - gap * 2 - group) / 2 : (c.w - cardW * 2 - gap) / 2;
  const int top = wide ? (c.h - cardH) / 2 - 26 : 44;
  for (int i = 0; i < 4; ++i) {
    const int x = wide ? left + i * (cardW + gap) + (i >= 2 ? group - gap : 0) : left + (i % 2) * (cardW + gap);
    const int y = wide ? top : top + (i / 2) * (cardH + 34);
    c.r.fillRoundedRect(x, y, cardW, cardH, 22, Color::Black);
    const float boxW = cardW * 0.62f;
    const float boxH = boxW * cc::kNumeralHeight;
    const float nx = x + (cardW - boxW) / 2;
    const float ny = y + (cardH - boxH) / 2;
    if (t.valid)
      numeral(c, t.d[i], nx, ny, boxW, boxW * 0.105f, false);
    else
      dash(c, nx, ny, boxW, boxW * 0.105f, false);
    // The hinge: a clean cut, and a pin at either side.
    c.r.fillRect(x, y + cardH / 2 - 2, cardW, 4, false);
    c.r.fillRect(x - 5, y + cardH / 2 - 12, 10, 24, true);
    c.r.fillRect(x + cardW - 5, y + cardH / 2 - 12, 10, 24, true);
  }
  if (wide) {
    // The pair of dots between hours and minutes.
    const int cx = left + 2 * (cardW + gap) - gap + group / 2;
    disc(c, cx, top + cardH / 2 - 46, 9);
    disc(c, cx, top + cardH / 2 + 46, 9);
  }
  if (now.valid) {
    text(c, toybox::kDisplayFontId, toybox::kDisplayCut, c.w / 2, wide ? top + cardH + 38 : c.h - 78, date);
  }
}

// ---------------------------------------------------------------- ESFERA
// A dial with nothing on it but its marks and two hands, and the date set
// beside it like the page of a calendar.
void faceEsfera(const Canvas& c, const Moment& now) {
  const bool wide = c.wide();
  const int radius = wide ? c.h / 2 - 26 : c.w / 2 - 26;
  const int cx = wide ? radius + 36 : c.w / 2;
  const int cy = wide ? c.h / 2 : radius + 44;
  ring(c, cx, cy, radius - 5, radius, 0, 1, 0);
  for (int k = 0; k < 60; ++k) {
    const float a = static_cast<float>(k) / 60 * 2 * kPi;
    const float sx = std::sin(a);
    const float sy = -std::cos(a);
    if (k % 5 == 0) {
      const float in = radius - (k % 15 == 0 ? 46.0f : 34.0f);
      stroke(c, cx + sx * in, cy + sy * in, cx + sx * (radius - 16), cy + sy * (radius - 16),
             k % 15 == 0 ? 5.0f : 3.0f);
    } else {
      disc(c, static_cast<int>(std::lround(cx + sx * (radius - 18))),
           static_cast<int>(std::lround(cy + sy * (radius - 18))), 1);
    }
  }
  if (now.valid) {
    const float ha = (static_cast<float>(now.hour % 12) + static_cast<float>(now.minute) / 60) / 12 * 2 * kPi;
    const float ma = static_cast<float>(now.minute) / 60 * 2 * kPi;
    // The minute hand is drawn in white first, a little fatter, so it stays
    // readable where it crosses the hour hand.
    stroke(c, cx - std::sin(ha) * 22, cy + std::cos(ha) * 22, cx + std::sin(ha) * radius * 0.5f,
           cy - std::cos(ha) * radius * 0.5f, 9);
    stroke(c, cx - std::sin(ma) * 30, cy + std::cos(ma) * 30, cx + std::sin(ma) * radius * 0.8f,
           cy - std::cos(ma) * radius * 0.8f, 8, false);
    stroke(c, cx - std::sin(ma) * 30, cy + std::cos(ma) * 30, cx + std::sin(ma) * radius * 0.8f,
           cy - std::cos(ma) * radius * 0.8f, 4.5f);
  }
  disc(c, cx, cy, 13);
  disc(c, cx, cy, 5, false);
  if (!now.valid) return;
  // The calendar leaf: weekday, the day large, the month.
  const int px = wide ? cx + radius + (c.w - cx - radius) / 2 : c.w / 2;
  const float boxW = wide ? 92 : 62;
  const float boxH = boxW * cc::kNumeralHeight;
  const int blockH = static_cast<int>(boxH) + 2 * (toybox::kDisplayCut.inkHeight + 26);
  const int py = wide ? (c.h - blockH) / 2 : cy + radius + (c.h - cy - radius - blockH) / 2;
  text(c, toybox::kDisplayFontId, toybox::kDisplayCut, px, py, cc::weekdayEs(now.weekday));
  const int ny = py + toybox::kDisplayCut.inkHeight + 26;
  const bool two = now.day >= 10;
  const float nx = px - (two ? boxW + 7 : boxW / 2);
  if (two) numeral(c, now.day / 10, nx, static_cast<float>(ny), boxW, boxW * 0.1f);
  numeral(c, now.day % 10, two ? nx + boxW + 14 : nx, static_cast<float>(ny), boxW, boxW * 0.1f);
  text(c, toybox::kDisplayFontId, toybox::kDisplayCut, px, ny + static_cast<int>(boxH) + 26, cc::monthEs(now.month));
}

// ---------------------------------------------------------------- ANILLOS
// Two arcs that fill as the hour and the half-day go by, with the time small
// in the middle of them.
void faceAnillos(const Canvas& c, const Moment& now) {
  const Digits t = digitsOf(now);
  const bool wide = c.wide();
  const int outer = (wide ? c.h : c.w) / 2 - 22;
  const int cx = c.w / 2;
  const int cy = wide ? c.h / 2 : outer + 60;
  const int band = 30;
  const int gapBetween = 16;
  const int r1o = outer;
  const int r1i = outer - band;
  const int r2o = r1i - gapBetween;
  const int r2i = r2o - band;
  ring(c, cx, cy, r1i, r1o, 0, 1, 1);
  ring(c, cx, cy, r2i, r2o, 0, 1, 1);
  if (now.valid) {
    const float minutes = static_cast<float>(now.minute) / 60;
    const float hours = (static_cast<float>(now.hour % 12) + minutes) / 12;
    auto arc = [&](const int in, const int out, const float turn) {
      if (turn <= 0.001f) {
        disc(c, cx, cy - (in + out) / 2, band / 2);
        return;
      }
      ring(c, cx, cy, in, out, 0, turn, 0);
      // Round ends: one at twelve o'clock, one where the arc has got to.
      const int mid = (in + out) / 2;
      disc(c, cx, cy - mid, band / 2);
      disc(c, static_cast<int>(std::lround(cx + std::sin(turn * 2 * kPi) * mid)),
           static_cast<int>(std::lround(cy - std::cos(turn * 2 * kPi) * mid)), band / 2);
    };
    arc(r1i, r1o, minutes);
    arc(r2i, r2o, hours);
  }
  const float boxW = static_cast<float>(r2i) * 0.3f;
  const float boxH = boxW * cc::kNumeralHeight;
  const float gap = boxW * 0.2f;
  const float colon = boxW * 0.45f;
  float x = cx - (boxW * 4 + gap * 2 + colon) / 2;
  const float y = cy - boxH / 2;
  for (int i = 0; i < 4; ++i) {
    if (t.valid)
      numeral(c, t.d[i], x, y, boxW, boxW * 0.1f);
    else
      dash(c, x, y, boxW, boxW * 0.1f);
    if (i == 1) {
      disc(c, static_cast<int>(x + boxW + colon / 2), static_cast<int>(cy - boxH * 0.2f), 4);
      disc(c, static_cast<int>(x + boxW + colon / 2), static_cast<int>(cy + boxH * 0.2f), 4);
    }
    x += boxW + (i == 1 ? colon : gap);
  }
  if (!now.valid) return;
  char date[32];
  dateLine(now, date, sizeof(date));
  if (wide) {
    // The date as two short lines in the margin the rings leave at the left.
    text(c, toybox::kUiFontId, toybox::kUiCut, 28, c.h / 2 - toybox::kUiCut.inkHeight - 8, cc::weekdayEs(now.weekday),
         -1);
    char rest[toybox::kIntChars + 8];
    std::snprintf(rest, sizeof(rest), "%d %s", now.day, cc::monthEs(now.month));
    text(c, toybox::kUiFontId, toybox::kUiCut, 28, c.h / 2 + 8, rest, -1);
  } else {
    text(c, toybox::kDisplayFontId, toybox::kDisplayCut, c.w / 2, cy + outer + (c.h - cy - outer) / 2 - 20, date);
  }
}

// ---------------------------------------------------------------- MATRIZ
// A board of lamps: lit ones full, the rest a speck, so the grid is always there.
void faceMatriz(const Canvas& c, const Moment& now) {
  const Digits t = digitsOf(now);
  const bool wide = c.wide();
  const int cols = wide ? 25 : 11;  // 5 a numeral, 1 between, 3 for the colon
  const int rows = wide ? 7 : 15;   // two rows of numerals with one between
  const int pitch = wide ? 30 : 40;
  const int lamp = pitch * 2 / 5;
  const int left = (c.w - cols * pitch) / 2 + pitch / 2;
  const int top = (c.h - rows * pitch) / 2 + pitch / 2 - (wide ? 30 : 34);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      bool on = false;
      if (t.valid) {
        if (wide) {
          // Columns: HH at 0 and 6, the colon at 12, MM at 14 and 20.
          static const int kStart[4] = {0, 6, 14, 20};
          for (int i = 0; i < 4; ++i) {
            const int k = col - kStart[i];
            if (k >= 0 && k < 5) on = (cc::dotsOf(t.d[i])[row] >> (4 - k)) & 1;
          }
          if (col == 12 && (row == 2 || row == 4)) on = true;
        } else {
          const int line = row / 8;  // 0 hours, 1 minutes
          const int r = row - line * 8;
          if (r < 7) {
            const int i = line * 2 + (col >= 6 ? 1 : 0);
            const int k = col - (col >= 6 ? 6 : 0);
            if (k < 5) on = (cc::dotsOf(t.d[i])[r] >> (4 - k)) & 1;
          }
        }
      }
      const int x = left + col * pitch;
      const int y = top + row * pitch;
      if (on)
        disc(c, x, y, lamp);
      else
        disc(c, x, y, 2);
    }
  }
  if (now.valid) {
    char date[32];
    dateLine(now, date, sizeof(date));
    text(c, toybox::kDisplayFontId, toybox::kDisplayCut, c.w / 2, top + rows * pitch - pitch / 2 + (wide ? 44 : 36),
         date);
  }
}

// ---------------------------------------------------------------- SEGMENTOS
// A liquid-crystal readout, leaning, with the unlit segments faintly there.
void faceSegmentos(const Canvas& c, const Moment& now) {
  const Digits t = digitsOf(now);
  const bool wide = c.wide();
  const float dw = wide ? 132 : 158;  // a numeral's width
  const float dh = dw * 1.9f;
  const float thick = dw * 0.17f;
  const float lean = 0.12f;
  auto segment = [&](const float x, const float y, const int which, const bool lit) {
    // Each segment a six-sided bar, horizontal or vertical, in the numeral's box.
    const float half = dh / 2;
    const float t2 = thick / 2;
    const float g = thick * 0.14f;  // the gap where segments meet
    float ax, ay, bx, by;           // the bar's two ends, on its centre line
    switch (which) {
      case 0:
        ax = t2, ay = t2, bx = dw - t2, by = t2;
        break;
      case 1:
        ax = dw - t2, ay = t2, bx = dw - t2, by = half;
        break;
      case 2:
        ax = dw - t2, ay = half, bx = dw - t2, by = dh - t2;
        break;
      case 3:
        ax = t2, ay = dh - t2, bx = dw - t2, by = dh - t2;
        break;
      case 4:
        ax = t2, ay = half, bx = t2, by = dh - t2;
        break;
      case 5:
        ax = t2, ay = t2, bx = t2, by = half;
        break;
      default:
        ax = t2, ay = half, bx = dw - t2, by = half;
        break;
    }
    const bool horizontal = ay == by;
    float px[6];
    float py[6];
    if (horizontal) {
      const float l = ax + g, r = bx - g;
      const float pts[6][2] = {{l, ay}, {l + t2, ay - t2}, {r - t2, ay - t2},
                               {r, ay}, {r - t2, ay + t2}, {l + t2, ay + t2}};
      for (int i = 0; i < 6; ++i) px[i] = pts[i][0], py[i] = pts[i][1];
    } else {
      const float u = ay + g, d = by - g;
      const float pts[6][2] = {{ax, u}, {ax + t2, u + t2}, {ax + t2, d - t2},
                               {ax, d}, {ax - t2, d - t2}, {ax - t2, u + t2}};
      for (int i = 0; i < 6; ++i) px[i] = pts[i][0], py[i] = pts[i][1];
    }
    int xs[6];
    int ys[6];
    for (int i = 0; i < 6; ++i) {
      xs[i] = static_cast<int>(std::lround(x + px[i] + (dh - py[i]) * lean));
      ys[i] = static_cast<int>(std::lround(y + py[i]));
    }
    if (lit) {
      c.r.fillPolygon(xs, ys, 6, true);
      return;
    }
    // Unlit: its outline in specks.
    for (int i = 0; i < 6; ++i) {
      const int j = (i + 1) % 6;
      const int n =
          static_cast<int>(std::hypot(static_cast<float>(xs[j] - xs[i]), static_cast<float>(ys[j] - ys[i])) / 5) + 1;
      for (int k = 0; k < n; ++k) c.r.drawPixel(xs[i] + (xs[j] - xs[i]) * k / n, ys[i] + (ys[j] - ys[i]) * k / n, true);
    }
  };
  const float gap = dw * 0.22f;
  const float colon = dw * 0.4f;
  const float shift = dh * lean / 2;
  for (int i = 0; i < 4; ++i) {
    float x;
    float y;
    if (wide) {
      const float total = dw * 4 + gap * 2 + colon;
      x = (c.w - total) / 2 - shift + i * (dw + gap) + (i >= 2 ? colon - gap : 0);
      y = (c.h - dh) / 2 - 24;
    } else {
      x = (c.w - dw * 2 - gap) / 2 - shift + (i % 2) * (dw + gap);
      y = 40 + (i / 2) * (dh + 34);
    }
    const uint8_t mask = t.valid ? cc::segmentsOf(t.d[i]) : 0x40;
    for (int sgm = 0; sgm < 7; ++sgm) segment(x, y, sgm, (mask >> sgm) & 1);
  }
  if (wide) {
    const float cx = (c.w - (dw * 4 + gap * 2 + colon)) / 2 - shift + 2 * (dw + gap) - gap + colon / 2;
    const float y = (c.h - dh) / 2 - 24;
    for (const float f : {0.3f, 0.7f}) {
      const int px = static_cast<int>(cx + (dh - dh * f) * lean);
      c.r.fillRect(px - static_cast<int>(thick / 2), static_cast<int>(y + dh * f - thick / 2), static_cast<int>(thick),
                   static_cast<int>(thick), true);
    }
  }
  if (now.valid) {
    char date[32];
    dateLine(now, date, sizeof(date));
    text(c, toybox::kDisplayFontId, toybox::kDisplayCut, c.w / 2,
         wide ? static_cast<int>((c.h + dh) / 2) + 12 : c.h - 70, date);
  }
}

// ---------------------------------------------------------------- PALABRAS
// The time as it is said, in capitals as large as the longest word allows.
void facePalabras(const Canvas& c, const Moment& now) {
  if (!now.valid) {
    text(c, toybox::kLargeFontId, toybox::kLargeCut, 40, c.h / 2 - 40, "SIN HORA", -1);
    return;
  }
  const cc::Words w = cc::wordsEs(now.hour, now.minute);
  // The tail may be two words; on a narrow panel it takes two lines.
  char tailA[20];
  char tailB[20] = "";
  std::snprintf(tailA, sizeof(tailA), "%s", w.tail);
  const int margin = c.wide() ? 48 : 34;
  const int room = c.w - margin * 2;
  struct Cut {
    int font;
    const toybox::CutMetrics* cut;
  };
  const Cut cuts[3] = {{toybox::kHugeFontId, &toybox::kHugeCut},
                       {toybox::kLargeFontId, &toybox::kLargeCut},
                       {toybox::kDisplayFontId, &toybox::kDisplayCut}};
  auto fits = [&](const Cut& cut) {
    return c.r.getTextWidth(cut.font, w.hour) <= room && c.r.getTextWidth(cut.font, tailA) <= room &&
           c.r.getTextWidth(cut.font, tailB) <= room;
  };
  int pick = 0;
  while (pick < 2 && !fits(cuts[pick])) ++pick;
  if (pick > 0 && !c.wide()) {
    // Before going smaller on a tall panel, try breaking the tail in two.
    char* space = std::strchr(tailA, ' ');
    if (space != nullptr) {
      std::snprintf(tailB, sizeof(tailB), "%s", space + 1);
      *space = 0;
      pick = 0;
      while (pick < 2 && !fits(cuts[pick])) ++pick;
    }
  }
  const Cut& big = cuts[pick];
  const int lead = big.cut->inkHeight + big.cut->inkHeight * 2 / 5;
  const int lines = 2 + (tailB[0] ? 1 : 0);
  const int blockH = toybox::kDisplayCut.inkHeight + 30 + lines * lead - (lead - big.cut->inkHeight);
  int y = (c.h - blockH) / 2 - 20;
  text(c, toybox::kDisplayFontId, toybox::kDisplayCut, margin, y, w.lead, -1);
  y += toybox::kDisplayCut.inkHeight + 30;
  // The hour inverted on a bar across the page; the rest in plain ink.
  c.r.fillRect(0, y - big.cut->inkHeight / 4, c.w, big.cut->inkHeight * 3 / 2, true);
  text(c, big.font, *big.cut, margin, y, w.hour, -1, false);
  y += lead + big.cut->inkHeight / 4;
  text(c, big.font, *big.cut, margin, y, tailA, -1);
  if (tailB[0]) text(c, big.font, *big.cut, margin, y + lead, tailB, -1);
  // The exact time and the date, small, in the corners.
  char exact[2 * toybox::kIntChars + 2];
  std::snprintf(exact, sizeof(exact), "%02d:%02d", now.hour % 100, now.minute % 100);
  char date[32];
  dateLine(now, date, sizeof(date));
  text(c, toybox::kUiFontId, toybox::kUiCut, margin, c.h - 70, date, -1);
  text(c, toybox::kUiFontId, toybox::kUiCut, c.w - margin, c.h - 70, exact, 1);
}

}  // namespace

void draw(GfxRenderer& renderer, const int face, const Moment& now) {
  const Canvas c{renderer, renderer.getScreenWidth(), renderer.getScreenHeight()};
  switch (face) {
    case 0:
      faceTrazo(c, now);
      break;
    case 1:
      facePaletas(c, now);
      break;
    case 2:
      faceEsfera(c, now);
      break;
    case 3:
      faceAnillos(c, now);
      break;
    case 4:
      faceMatriz(c, now);
      break;
    case 5:
      faceSegmentos(c, now);
      break;
    default:
      facePalabras(c, now);
      break;
  }
}

void drawNumeral(GfxRenderer& renderer, const int digit, const float x, const float y, const float boxW,
                 const float pen, const bool ink) {
  const Canvas c{renderer, renderer.getScreenWidth(), renderer.getScreenHeight()};
  numeral(c, digit, x, y, boxW, pen, ink);
}

void drawHint(GfxRenderer& renderer, const char* message) {
  const Canvas c{renderer, renderer.getScreenWidth(), renderer.getScreenHeight()};
  const int h = toybox::kTileCut.inkHeight + 18;
  renderer.fillRect(0, c.h - h, c.w, h, true);
  text(c, toybox::kTileFontId, toybox::kTileCut, c.w / 2, c.h - h + 9, message, 0, false);
}

}  // namespace clockfaces
