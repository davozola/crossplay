#include "SynthFigures.h"

#include <cmath>

namespace synthfig {

namespace {

constexpr float kPi = 3.14159265f;
constexpr int kLabelGap = 14;  // from a drawing to the label under it

// --- Pieces every figure is made of. ---

void arrow(Canvas& c, const int x0, const int y0, const int x1, const int y1, const int width = 2) {
  c.line(x0, y0, x1, y1, width);
  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0f) return;
  const float ux = dx / len;
  const float uy = dy / len;
  // A solid head, as lines fanned between its two barbs.
  for (int k = 0; k <= 6; ++k) {
    const float spread = (static_cast<float>(k) - 3.0f) * 1.6f;
    const int bx = static_cast<int>(static_cast<float>(x1) - ux * 9.0f - uy * spread);
    const int by = static_cast<int>(static_cast<float>(y1) - uy * 9.0f + ux * spread);
    c.line(bx, by, x1, y1, 1);
  }
}

void dashed(Canvas& c, const int x0, const int y0, const int x1, const int y1, const int width = 2) {
  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  const float len = std::sqrt(dx * dx + dy * dy);
  const int dashes = static_cast<int>(len / 8.0f);
  for (int i = 0; i < dashes; i += 2) {
    const float a = static_cast<float>(i) / static_cast<float>(dashes);
    const float b = static_cast<float>(i + 1) / static_cast<float>(dashes);
    c.line(x0 + static_cast<int>(dx * a), y0 + static_cast<int>(dy * a), x0 + static_cast<int>(dx * b),
           y0 + static_cast<int>(dy * b), width);
  }
}

void box(Canvas& c, const int x, const int y, const int w, const int h, const char* label, const bool solid = false) {
  if (solid) {
    c.fill(x, y, w, h, true);
  } else {
    c.fill(x, y, w, h, false);
    c.frame(x, y, w, h, 2);
  }
  c.text(x + w / 2, y + h / 2, label, Align::Centre, !solid);
}

// A curve through `n` + 1 samples of f(t), t from 0 to 1, f from 0 (bottom)
// to 1 (top) of the box.
template <typename F>
void plot(Canvas& c, const int x, const int y, const int w, const int h, F f, const int n = 60, const int width = 2) {
  int px = x;
  int py = y + h - static_cast<int>(f(0.0f) * static_cast<float>(h));
  for (int i = 1; i <= n; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(n);
    const int qx = x + static_cast<int>(t * static_cast<float>(w));
    const int qy = y + h - static_cast<int>(f(t) * static_cast<float>(h));
    c.line(px, py, qx, qy, width);
    px = qx;
    py = qy;
  }
}

// Axes: a baseline and a left edge.
void axes(Canvas& c, const int x, const int y, const int w, const int h) {
  c.line(x, y + h, x + w, y + h, 1);
  c.line(x, y, x, y + h, 1);
}

// Panels in a row, `count` of them, each its drawing over its label.
struct Cells {
  int x, y, w, gap, cellW;
  Cells(const int x0, const int y0, const int width, const int count, const int g = 10)
      : x(x0), y(y0), w(width), gap(g), cellW((width - g * (count - 1)) / count) {}
  int at(const int i) const { return x + i * (cellW + gap); }
};

float saw(const float t) { return t - std::floor(t); }
float tri(const float t) {
  const float p = saw(t);
  return p < 0.5f ? p * 2.0f : 2.0f - p * 2.0f;
}

// --- The figures. ---

void signalFlow(Canvas& c, const int x, const int y, const int w) {
  const int bh = 28;
  const int srcW = w * 18 / 100;
  const int col = (w - srcW) / 4;
  const int boxW = col - 22;
  const char* src[4] = {"OSC A", "OSC B", "NOISE", "SUB"};
  const int top = y + 4;
  for (int i = 0; i < 4; ++i) box(c, x, top + i * (bh + 10), srcW, bh, src[i]);
  const int stackH = 4 * bh + 3 * 10;
  const int midY = top + stackH / 2;
  const int fx = x + srcW + 22;
  box(c, fx, top, boxW, stackH, "FILTER");
  for (int i = 0; i < 4; ++i) {
    const int cy = top + i * (bh + 10) + bh / 2;
    arrow(c, x + srcW + 2, cy, fx - 2, cy);
  }
  const int ex = fx + boxW + 22;
  box(c, ex, midY - 30, boxW, 60, "ENV 1");
  c.text(ex + boxW / 2, midY + 44, "AMP", Align::Centre, true);
  arrow(c, fx + boxW + 2, midY, ex - 2, midY);
  const int gx = ex + boxW + 22;
  box(c, gx, midY - 30, boxW, 60, "FX X7");
  arrow(c, ex + boxW + 2, midY, gx - 2, midY);
  const int ox = gx + boxW + 22;
  const int ow = x + w - ox;
  box(c, ox, midY - 30, ow, 60, "OUT", true);
  arrow(c, gx + boxW + 2, midY, ox - 2, midY);
  // DIRECT: the sub around the filter and the effects.
  const int subY = top + 3 * (bh + 10) + bh / 2;
  const int low = top + stackH + 26;
  dashed(c, x + srcW / 2, top + stackH, x + srcW / 2, low);
  dashed(c, x + srcW / 2, low, ox + ow / 2, low);
  arrow(c, ox + ow / 2, low, ox + ow / 2, midY + 32);
  c.text(x + srcW + 30, low - 10, "DIRECT", Align::Left, true);
  (void)subY;
}

void waveforms(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 5);
  const int h = 70;
  const char* names[5] = {"SINE", "TRIANGLE", "SAW", "SQUARE", "PULSE"};
  for (int i = 0; i < 5; ++i) {
    const int cx = cells.at(i);
    c.line(cx, y + h / 2, cx + cells.cellW, y + h / 2, 1);
    switch (i) {
      case 0:
        plot(c, cx, y, cells.cellW, h, [](const float t) { return 0.5f + 0.45f * std::sin(t * 4.0f * kPi); }, 64);
        break;
      case 1:
        plot(c, cx, y, cells.cellW, h, [](const float t) { return 0.05f + 0.9f * tri(t * 2.0f + 0.25f); }, 64);
        break;
      case 2:
        plot(c, cx, y, cells.cellW, h, [](const float t) { return 0.05f + 0.9f * saw(t * 2.0f + 0.5f); }, 200);
        break;
      default: {
        const float duty = i == 3 ? 0.5f : 0.2f;
        plot(c, cx, y, cells.cellW, h, [duty](const float t) { return saw(t * 2.0f) < duty ? 0.95f : 0.05f; }, 200);
        break;
      }
    }
    c.text(cx + cells.cellW / 2, y + h + kLabelGap, names[i], Align::Centre, true);
  }
}

void wavetable(Canvas& c, const int x, const int y, const int w) {
  // Ten frames, each drawn a little further back and to the right, from a
  // sine to a saw: the 3D view the editor and the FM-1 show.
  const int frames = 10;
  const int fw = w * 52 / 100;
  const int fh = 64;
  const int stepX = (w - fw - 8) / (frames - 1);
  const int stepY = 9;
  const int chosen = 4;
  for (int f = frames - 1; f >= 0; --f) {
    const int fx = x + f * stepX;
    const int fy = y + 6 + (frames - 1 - f) * stepY;
    const float m = static_cast<float>(f) / static_cast<float>(frames - 1);
    plot(
        c, fx, fy, fw, fh,
        [m](const float t) {
          const float sine = 0.5f + 0.45f * std::sin(t * 2.0f * kPi);
          const float s = 0.05f + 0.9f * saw(t + 0.5f);
          return sine * (1.0f - m) + s * m;
        },
        48, f == chosen ? 4 : 1);
  }
  const int baseY = y + 6 + (frames - 1) * stepY + fh + 22;
  arrow(c, x + fw / 2, baseY, x + (frames - 1) * stepX + fw / 2, baseY);
  c.text(x + fw / 2 - 30, baseY + 16, "FRAME 1", Align::Left, true);
  c.text(x + (frames - 1) * stepX + fw / 2 + 20, baseY + 16, "ULTIMO", Align::Right, true);
  const int chosenX = x + chosen * stepX + fw / 2;
  c.line(chosenX, baseY - 7, chosenX, baseY + 7, 4);
  c.text(chosenX, baseY - 18, "WT POS", Align::Centre, true);
}

void harmonics(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 2, 24);
  const int h = 90;
  const char* names[2] = {"SAW: TODOS", "SQUARE: IMPARES"};
  for (int s = 0; s < 2; ++s) {
    const int cx = cells.at(s);
    axes(c, cx, y, cells.cellW, h);
    const int bars = 10;
    const int bw = (cells.cellW - 8) / bars;
    for (int n = 1; n <= bars; ++n) {
      if (s == 1 && n % 2 == 0) continue;
      const int bh = static_cast<int>(static_cast<float>(h - 4) / static_cast<float>(n));
      c.fill(cx + 4 + (n - 1) * bw + 2, y + h - bh, bw - 4, bh, true);
    }
    c.text(cx + 4 + bw / 2, y + h + kLabelGap, "1", Align::Centre, true);
    c.text(cx + 4 + bw * 9 + bw / 2, y + h + kLabelGap, "10", Align::Centre, true);
    c.text(cx + cells.cellW / 2, y + h + kLabelGap + 20, names[s], Align::Centre, true);
  }
}

void unison(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 2, 24);
  const int h = 96;
  // Left: five voices around the note in pitch. The centre one is taller:
  // BLEND sets how loud the outer ones are against it.
  int cx = cells.at(0);
  axes(c, cx, y, cells.cellW, h);
  const int mid = cx + cells.cellW / 2;
  const int spread = cells.cellW / 7;
  const int heights[5] = {48, 62, 86, 62, 48};
  for (int i = 0; i < 5; ++i) {
    const int vx = mid + (i - 2) * spread;
    c.fill(vx - 3, y + h - heights[i], 6, heights[i], i == 2 ? true : true);
    if (i != 2) c.fill(vx - 1, y + h - heights[i] + 2, 2, heights[i] - 4, false);
  }
  arrow(c, mid - 6, y + 14, mid - 2 * spread + 2, y + 14, 1);
  arrow(c, mid + 6, y + 14, mid + 2 * spread - 2, y + 14, 1);
  c.text(mid, y + h + kLabelGap, "DETUNE: SEPARA EL TONO", Align::Centre, true);
  c.text(mid, y + h + kLabelGap + 20, "BLEND: CENTRO O LADOS", Align::Centre, true);
  // Right: the same voices placed from left to right in stereo.
  cx = cells.at(1);
  c.line(cx, y + h / 2, cx + cells.cellW, y + h / 2, 1);
  c.text(cx + 2, y + h / 2 - 14, "L", Align::Left, true);
  c.text(cx + cells.cellW - 2, y + h / 2 - 14, "R", Align::Right, true);
  const int sm = cx + cells.cellW / 2;
  for (int i = 0; i < 5; ++i) {
    const int vx = sm + (i - 2) * (cells.cellW / 5);
    c.fill(vx - 6, y + h / 2 - 6, 12, 12, true);
  }
  arrow(c, sm, y + h / 2 + 24, cx + 12, y + h / 2 + 24, 1);
  arrow(c, sm, y + h / 2 + 24, cx + cells.cellW - 12, y + h / 2 + 24, 1);
  c.text(sm, y + h + kLabelGap, "WIDTH: SE ABREN", Align::Centre, true);
  c.text(sm, y + h + kLabelGap + 20, "EN ESTEREO", Align::Centre, true);
}

void warp(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 4);
  const int h = 70;
  const char* names[4] = {"OFF", "SYNC", "BEND+", "MIRROR"};
  for (int i = 0; i < 4; ++i) {
    const int cx = cells.at(i);
    c.frame(cx, y, cells.cellW, h, 1);
    const int px = cx + 4;
    const int pw = cells.cellW - 8;
    switch (i) {
      case 0:
        plot(c, px, y + 4, pw, h - 8, [](const float t) { return 0.5f + 0.45f * std::sin(t * 2.0f * kPi); });
        break;
      case 1:  // the cycle restarts before it ends: more of it fits
        plot(
            c, px, y + 4, pw, h - 8,
            [](const float t) { return 0.5f + 0.45f * std::sin(saw(t * 1.0f) * 2.6f * 2.0f * kPi); }, 120);
        break;
      case 2:  // the cycle squeezed towards its start
        plot(c, px, y + 4, pw, h - 8,
             [](const float t) { return 0.5f + 0.45f * std::sin(std::pow(t, 0.45f) * 2.0f * kPi); });
        break;
      default:  // the first half played, then played back
        plot(c, px, y + 4, pw, h - 8, [](const float t) {
          const float p = t < 0.5f ? t * 2.0f : 2.0f - t * 2.0f;
          return 0.5f + 0.45f * std::sin(p * kPi);
        });
        break;
    }
    c.text(cx + cells.cellW / 2, y + h + kLabelGap, names[i], Align::Centre, true);
  }
}

// A filter's response on a log-frequency axis, 0 to 1 across the box:
// `kind` 0 low-pass, 1 high-pass, 2 band-pass, 3 notch; `res` 0 to 1; `order`
// in poles of 6 dB.
float response(const int kind, const float t, const float res, const int order) {
  const float f = std::pow(10.0f, (t - 0.5f) * 3.0f);  // frequency over cutoff, 1/30 to 30
  const float q = 0.7f + res * 6.0f;
  float g = 1.0f;
  switch (kind) {
    case 0:
      g = 1.0f / std::sqrt(std::pow(1.0f - f * f, 2.0f) + std::pow(f / q, 2.0f));
      if (order == 1) g = 1.0f / std::sqrt(1.0f + f * f);
      if (order == 4) g *= 1.0f / std::sqrt(std::pow(1.0f - f * f, 2.0f) + std::pow(f / 0.7f, 2.0f));
      break;
    case 1:
      g = (f * f) / std::sqrt(std::pow(1.0f - f * f, 2.0f) + std::pow(f / q, 2.0f));
      break;
    case 2:
      g = (f / q) / std::sqrt(std::pow(1.0f - f * f, 2.0f) + std::pow(f / q, 2.0f));
      break;
    default:
      g = std::fabs(1.0f - f * f) / std::sqrt(std::pow(1.0f - f * f, 2.0f) + std::pow(f / 0.7f, 2.0f));
      break;
  }
  // As decibels, -48 dB at the bottom to +12 dB at the top.
  const float db = 20.0f * std::log10(g < 1e-4f ? 1e-4f : g);
  float v = (db + 48.0f) / 60.0f;
  return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

void filterTypes(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 4);
  const int h = 70;
  const char* names[4] = {"LP", "HP", "BP", "NOTCH"};
  for (int i = 0; i < 4; ++i) {
    const int cx = cells.at(i);
    axes(c, cx, y, cells.cellW, h);
    dashed(c, cx + cells.cellW / 2, y, cx + cells.cellW / 2, y + h, 1);
    plot(c, cx, y, cells.cellW, h, [i](const float t) { return response(i, t, 0.0f, 2); }, 80);
    c.text(cx + cells.cellW / 2, y + h + kLabelGap, names[i], Align::Centre, true);
  }
}

void cutoffRes(Canvas& c, const int x, const int y, const int w) {
  const int h = 110;
  const int pw = w - 20;
  axes(c, x, y, pw, h);
  const int cut = x + pw / 2;
  dashed(c, cut, y, cut, y + h, 1);
  plot(c, x, y, pw, h, [](const float t) { return response(0, t, 0.0f, 2); }, 100, 2);
  plot(c, x, y, pw, h, [](const float t) { return response(0, t, 0.85f, 2); }, 100, 3);
  c.text(cut, y + h + kLabelGap, "CUTOFF", Align::Centre, true);
  c.text(cut + 24, y + 6, "RES ALTA", Align::Left, true);
  c.text(x + 8, y + h + kLabelGap, "GRAVES", Align::Left, true);
  c.text(x + pw, y + h + kLabelGap, "AGUDOS", Align::Right, true);
}

void slopes(Canvas& c, const int x, const int y, const int w) {
  const int h = 110;
  const int pw = w - 40;
  axes(c, x, y, pw, h);
  dashed(c, x + pw / 2, y, x + pw / 2, y + h, 1);
  const int orders[3] = {1, 2, 4};
  const char* names[3] = {"6 DB", "12 DB", "24 DB"};
  for (int i = 0; i < 3; ++i) {
    const int order = orders[i];
    plot(c, x, y, pw, h, [order](const float t) { return response(0, t, 0.0f, order); }, 100, i == 2 ? 3 : 2);
    // Each slope's name where it reaches the floor, under the axis; the
    // gentlest never does, so its name goes at its end.
    float floorAt = 1.0f;
    for (int k = 50; k <= 100; ++k) {
      const float t = static_cast<float>(k) / 100.0f;
      if (response(0, t, 0.0f, order) < 0.02f) {
        floorAt = t;
        break;
      }
    }
    if (floorAt < 1.0f) {
      const int fx = x + static_cast<int>(floorAt * static_cast<float>(pw));
      c.line(fx, y + h, fx, y + h + 5, 2);
      c.text(fx, y + h + kLabelGap + 2, names[i], Align::Centre, true);
    } else {
      const int ly = y + h - static_cast<int>(response(0, 1.0f, 0.0f, order) * static_cast<float>(h));
      c.text(x + pw + 4, ly, names[i], Align::Left, true);
    }
  }
  c.text(x + pw / 2 - 20, y + h + kLabelGap + 2, "CUTOFF", Align::Centre, true);
}

void adsr(Canvas& c, const int x, const int y, const int w) {
  const int h = 100;
  const int pw = w;
  const int a = pw * 14 / 100;
  const int d = pw * 16 / 100;
  const int s = pw * 36 / 100;
  const int r = pw * 24 / 100;
  const int top = y;
  const int base = y + h;
  const int sus = y + h * 45 / 100;
  c.line(x, base, x + a, top, 3);
  // Decay and release as curves, the way Serum's are.
  plot(c, x + a, top, d, base - top, [](const float t) { return 0.55f + 0.45f * std::exp(-4.0f * t); }, 30, 3);
  c.line(x + a + d, sus, x + a + d + s, sus, 3);
  plot(
      c, x + a + d + s, top, r, base - top, [](const float t) { return 0.55f * std::exp(-4.0f * t) - 0.01f * t; }, 30,
      3);
  c.line(x, base, x + pw, base, 1);
  const char* names[4] = {"A", "D", "S", "R"};
  const int starts[5] = {x, x + a, x + a + d, x + a + d + s, x + a + d + s + r};
  for (int i = 0; i < 4; ++i) {
    c.text((starts[i] + starts[i + 1]) / 2, base + kLabelGap, names[i], Align::Centre, true);
    if (i > 0) dashed(c, starts[i], top, starts[i], base, 1);
  }
  // The key under it: held through A, D and S, let go at R.
  const int gy = base + 30;
  c.fill(x, gy, a + d + s, 12, true);
  c.text(x + 4, gy + 24, "TECLA PULSADA", Align::Left, true);
  c.text(starts[3] + 4, gy + 24, "SUELTAS", Align::Left, true);
}

void lfoShapes(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 4);
  const int h = 60;
  const char* names[4] = {"TRIANGLE", "SAW DOWN", "SAW UP", "S&H"};
  for (int i = 0; i < 4; ++i) {
    const int cx = cells.at(i);
    c.line(cx, y + h, cx + cells.cellW, y + h, 1);
    switch (i) {
      case 0:
        plot(c, cx, y, cells.cellW, h, [](const float t) { return tri(t * 2.0f); }, 64);
        break;
      case 1:
        plot(c, cx, y, cells.cellW, h, [](const float t) { return 1.0f - saw(t * 2.0f); }, 200);
        break;
      case 2:
        plot(c, cx, y, cells.cellW, h, [](const float t) { return saw(t * 2.0f); }, 200);
        break;
      default: {
        // Held steps of a fixed made-up series: random, but the same every time.
        static constexpr float kSteps[8] = {0.7f, 0.2f, 0.9f, 0.45f, 0.1f, 0.6f, 0.8f, 0.3f};
        plot(c, cx, y, cells.cellW, h, [](const float t) { return kSteps[static_cast<int>(t * 7.999f)]; }, 200);
        break;
      }
    }
    c.text(cx + cells.cellW / 2, y + h + kLabelGap, names[i], Align::Centre, true);
  }
}

void bipolar(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 2, 24);
  const int h = 90;
  const char* names[2] = {"UNIPOLAR: SOLO SUBE", "BIPOLAR: SUBE Y BAJA"};
  for (int i = 0; i < 2; ++i) {
    const int cx = cells.at(i);
    const int knobY = i == 0 ? y + h * 3 / 4 : y + h / 2;
    const int lw = 56;  // the label's room at the left
    dashed(c, cx + lw - 6, knobY, cx + cells.cellW, knobY, 1);
    c.text(cx, knobY, "MANDO", Align::Left, true);
    if (i == 0) {
      plot(
          c, cx + lw, y, cells.cellW - lw, h * 3 / 4,
          [](const float t) { return 0.5f - 0.5f * std::cos(t * 4.0f * kPi); }, 64, 3);
    } else {
      plot(
          c, cx + lw, y, cells.cellW - lw, h, [](const float t) { return 0.5f + 0.45f * std::sin(t * 4.0f * kPi); }, 64,
          3);
    }
    c.text(cx + cells.cellW / 2, y + h + kLabelGap, names[i], Align::Centre, true);
  }
}

void matrix(Canvas& c, const int x, const int y, const int w) {
  const int bw = w * 30 / 100;
  const int bh = 40;
  const int row = y + 6;
  box(c, x, row, bw, bh, "SOURCE");
  c.text(x + bw / 2, row + bh + kLabelGap, "LFO 1", Align::Centre, true);
  const int mid = x + w / 2;
  // AMOUNT as a dial: how much, and which way.
  c.fill(mid - 22, row - 2, 44, 44, true);
  c.fill(mid - 18, row + 2, 36, 36, false);
  c.text(mid, row + bh / 2, "X", Align::Centre, true);
  c.text(mid, row + bh + kLabelGap, "AMOUNT", Align::Centre, true);
  box(c, x + w - bw, row, bw, bh, "DEST");
  c.text(x + w - bw / 2, row + bh + kLabelGap, "CUTOFF", Align::Centre, true);
  arrow(c, x + bw + 2, row + bh / 2, mid - 24, row + bh / 2);
  arrow(c, mid + 24, row + bh / 2, x + w - bw - 2, row + bh / 2);
  // AUX, under it, scaling the amount.
  const int aux = row + bh + 46;
  box(c, mid - bw / 2, aux, bw, bh, "AUX");
  c.text(mid, aux + bh + kLabelGap, "MOD WHEEL", Align::Centre, true);
  arrow(c, mid, aux - 2, mid, row + bh + 26);
}

void glide(Canvas& c, const int x, const int y, const int w) {
  const int h = 80;
  axes(c, x, y, w, h);
  const int lowY = y + h - 16;
  const int highY = y + 16;
  const int third = w / 3;
  // Without glide the pitch jumps; with it, it slides for the GLIDE time.
  dashed(c, x + third, lowY, x + third, highY, 1);
  c.line(x, lowY, x + third, lowY, 3);
  c.line(x + third, lowY, x + third + third / 2, highY, 3);
  c.line(x + third + third / 2, highY, x + w, highY, 3);
  c.text(x + third + third / 2 + 10, (lowY + highY) / 2 + 8, "GLIDE", Align::Left, true);
  c.text(x + 4, lowY - 14, "NOTA 1", Align::Left, true);
  c.text(x + w - 4, highY + 16, "NOTA 2", Align::Right, true);
  // The keys: LEGATO glides only when the second is pressed before the first is let go.
  const int ky = y + h + 14;
  c.fill(x, ky, third + 30, 10, true);
  c.fill(x + third, ky + 16, w - third, 10, true);
  c.text(x + third + 34, ky + 4, "SE PISAN: LEGATO", Align::Left, true);
}

void arp(Canvas& c, const int x, const int y, const int w) {
  const int keysW = w * 22 / 100;
  const int h = 96;
  // The chord held: three notes at once.
  for (int i = 0; i < 3; ++i) c.fill(x + 10, y + h - 24 - i * 28, keysW - 20, 16, true);
  c.text(x + keysW / 2, y + h + kLabelGap, "ACORDE", Align::Centre, true);
  arrow(c, x + keysW + 4, y + h / 2, x + keysW + 30, y + h / 2);
  // The same notes one at a time, up, over two octaves.
  const int gx = x + keysW + 40;
  const int gw = w - (gx - x);
  const int steps = 12;
  const int sw = gw / steps;
  for (int s = 0; s < steps; ++s) {
    const int degree = s % 6;  // three notes, then the same three an octave up
    const int ny = y + h - 18 - degree * 14;
    c.fill(gx + s * sw + 1, ny, sw - 3, 10, true);
  }
  c.line(gx, y + h, gx + gw, y + h, 1);
  c.text(gx + gw / 2, y + h + kLabelGap, "UP  OCTAVES 2  RATE 1/16", Align::Centre, true);
}

void stepSeq(Canvas& c, const int x, const int y, const int w) {
  const int h = 80;
  const int steps = 16;
  const int sw = w / steps;
  // Notes and rests; the gate is how much of its step a note lasts.
  static constexpr int8_t kNotes[16] = {0, -1, 3, -1, 5, 5, -1, 2, 0, -1, 7, -1, 5, 3, 2, -1};
  static constexpr int8_t kGate[16] = {9, 0, 5, 0, 9, 9, 0, 3, 9, 0, 5, 0, 9, 9, 3, 0};
  for (int s = 0; s < steps; ++s) {
    c.frame(x + s * sw, y, sw, h, 1);
    if (s % 4 == 0) c.fill(x + s * sw, y, sw, 3, true);
    if (kNotes[s] < 0) continue;
    const int ny = y + h - 10 - kNotes[s] * 8;
    c.fill(x + s * sw + 2, ny, (sw - 4) * kGate[s] / 9, 7, true);
  }
  c.text(x, y + h + kLabelGap, "PASO 1", Align::Left, true);
  c.text(x + w / 2, y + h + kLabelGap, "STEP 1/16", Align::Centre, true);
  c.text(x + w, y + h + kLabelGap, "16", Align::Right, true);
  c.text(x + w / 2, y + h + kLabelGap + 20, "LARGO DE CADA BARRA: GATE", Align::Centre, true);
}

void automation(Canvas& c, const int x, const int y, const int w) {
  const int h = 80;
  const int bars = 4;
  const int bw = w / bars;
  for (int b = 0; b <= bars; ++b) c.line(x + b * bw, y, x + b * bw, y + h, 1);
  c.line(x, y + h, x + w, y + h, 1);
  // Points recorded from the knob, joined as the lane plays them.
  static constexpr float kPts[7][2] = {{0.0f, 0.2f},  {0.18f, 0.25f}, {0.32f, 0.8f}, {0.5f, 0.6f},
                                       {0.68f, 0.9f}, {0.85f, 0.3f},  {1.0f, 0.2f}};
  for (int i = 0; i < 7; ++i) {
    const int px = x + static_cast<int>(kPts[i][0] * static_cast<float>(w));
    const int py = y + h - static_cast<int>(kPts[i][1] * static_cast<float>(h));
    c.fill(px - 4, py - 4, 9, 9, true);
    if (i > 0) {
      const int qx = x + static_cast<int>(kPts[i - 1][0] * static_cast<float>(w));
      const int qy = y + h - static_cast<int>(kPts[i - 1][1] * static_cast<float>(h));
      c.line(qx, qy, px, py, 2);
    }
  }
  const char* names[4] = {"1", "2", "3", "4"};
  for (int b = 0; b < bars; ++b) c.text(x + b * bw + bw / 2, y + h + kLabelGap, names[b], Align::Centre, true);
  c.text(x + w / 2, y + h + kLabelGap + 20, "COMPASES: Y VUELTA A EMPEZAR", Align::Centre, true);
}

void polyrhythm(Canvas& c, const int x, const int y, const int w) {
  const int bars = 12;
  const int bw = w / bars;
  const int rowH = 30;
  const int lens[2] = {3, 4};
  const char* names[2] = {"PISTA DE 3", "PISTA DE 4"};
  for (int r = 0; r < 2; ++r) {
    const int ry = y + 22 + r * (rowH + 22);
    c.text(x + 12, ry - 12, names[r], Align::Left, true);
    for (int b = 0; b < bars; ++b) {
      const bool first = b % lens[r] == 0;
      if (first) {
        c.fill(x + b * bw + 1, ry, bw * lens[r] - 3, rowH, false);
        c.frame(x + b * bw + 1, ry, bw * lens[r] - 3, rowH, 2);
        c.fill(x + b * bw + 1, ry, 8, rowH, true);
      }
    }
  }
  const int end = y + 22 + 2 * (rowH + 22) - 14;
  c.line(x, y + 6, x, end, 3);
  c.line(x + bars * bw - 1, y + 6, x + bars * bw - 1, end, 3);
  c.text(x, end + kLabelGap, "1", Align::Left, true);
  c.text(x + bars * bw, end + kLabelGap, "13: COINCIDEN", Align::Right, true);
}

void fxChain(Canvas& c, const int x, const int y, const int w) {
  const char* names[7] = {"DIST", "COMP", "CHOR", "DLY", "REV", "EQ", "PUMP"};
  const int gap = 12;
  const int bw = (w - gap * 6) / 7;
  const int bh = 40;
  for (int i = 0; i < 7; ++i) {
    const int bx = x + i * (bw + gap);
    box(c, bx, y + 18, bw, bh, names[i]);
    char n[2] = {static_cast<char>('1' + i), 0};
    c.text(bx + bw / 2, y + 6, n, Align::Centre, true);
    if (i < 6) arrow(c, bx + bw + 1, y + 18 + bh / 2, bx + bw + gap - 1, y + 18 + bh / 2, 1);
  }
  c.text(x + w / 2, y + 18 + bh + kLabelGap, "EL SONIDO PASA DEL 1 AL 7", Align::Centre, true);
}

void voicePool(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 2, 24);
  const int cell = (cells.cellW - 14) / 8;
  const char* names[2] = {"UNISON 1: MAS NOTAS", "UNISON ALTO: MENOS"};
  for (int s = 0; s < 2; ++s) {
    const int cx = cells.at(s);
    for (int i = 0; i < 16; ++i) {
      const int vx = cx + (i % 8) * (cell + 2);
      const int vy = y + (i / 8) * (cell + 2);
      // Left: each note one voice. Right: each note four, so fewer fit.
      const bool used = s == 0 ? i < 8 : i < 12;
      if (used) {
        c.fill(vx, vy, cell, cell, true);
        if (s == 1 && i % 4 == 0 && i % 8 != 0) c.fill(vx - 2, vy - 2, 2, cell + 4, false);
      } else {
        c.frame(vx, vy, cell, cell, 1);
      }
    }
    c.text(cx + cells.cellW / 2, y + 2 * (cell + 2) + kLabelGap, names[s], Align::Centre, true);
  }
}

void kick(Canvas& c, const int x, const int y, const int w) {
  const Cells cells(x, y, w, 2, 24);
  const int h = 80;
  axes(c, cells.at(0), y, cells.cellW, h);
  plot(c, cells.at(0), y, cells.cellW, h, [](const float t) { return 0.05f + 0.9f * std::exp(-14.0f * t); }, 60, 3);
  c.text(cells.at(0) + cells.cellW / 2, y + h + kLabelGap, "TONO: CAE MUY RAPIDO", Align::Centre, true);
  axes(c, cells.at(1), y, cells.cellW, h);
  plot(c, cells.at(1), y, cells.cellW, h, [](const float t) { return 0.95f * std::exp(-4.5f * t); }, 60, 3);
  c.text(cells.at(1) + cells.cellW / 2, y + h + kLabelGap, "VOLUMEN: SE APAGA", Align::Centre, true);
}

struct Entry {
  const char* name;
  int height;
  void (*draw)(Canvas&, int, int, int);
};

constexpr Entry kEntries[kFigures] = {
    {"SIGNAL FLOW", 210, signalFlow},
    {"WAVEFORMS", 100, waveforms},
    {"WAVETABLE", 200, wavetable},
    {"HARMONICS", 140, harmonics},
    {"UNISON", 150, unison},
    {"WARP", 100, warp},
    {"FILTER TYPES", 100, filterTypes},
    {"CUTOFF RES", 140, cutoffRes},
    {"SLOPES", 140, slopes},
    {"ADSR", 176, adsr},
    {"LFO SHAPES", 90, lfoShapes},
    {"BIPOLAR", 120, bipolar},
    {"MATRIX", 170, matrix},
    {"GLIDE", 130, glide},
    {"ARP", 126, arp},
    {"STEP SEQ", 132, stepSeq},
    {"AUTOMATION", 132, automation},
    {"POLYRHYTHM", 150, polyrhythm},
    {"FX CHAIN", 90, fxChain},
    {"VOICE POOL", 110, voicePool},
    {"KICK", 110, kick},
};

}  // namespace

int height(const int figure, const int width) {
  (void)width;
  return figure >= 0 && figure < kFigures ? kEntries[figure].height : 0;
}

void draw(Canvas& canvas, const int figure, const int x, const int y, const int width) {
  if (figure < 0 || figure >= kFigures) return;
  kEntries[figure].draw(canvas, x, y, width);
}

const char* name(const int figure) { return figure >= 0 && figure < kFigures ? kEntries[figure].name : ""; }

}  // namespace synthfig
