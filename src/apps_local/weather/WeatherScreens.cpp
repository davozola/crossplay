#include "WeatherScreens.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFormat.h"

namespace weatherui {

namespace {

namespace wx = weather;

constexpr int kBottomReserve = 40;  // the button hints

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void box(toybox::Screen& screen, const int x, const int y, const int w, const int h, const fui::Paint& paint,
         const int radius = 0) {
  if (w <= 0 || h <= 0) return;
  screen.target().fill(rect(x, y, w, h), paint, static_cast<uint8_t>(radius));
}

// Text whose INK (cap height) is centred in `r` vertically.
void label(toybox::Screen& screen, const fui::Rect& r, const char* text, const fui::FontId font,
           const toybox::CutMetrics& cut, const fui::TextAlign align = fui::TextAlign::Center,
           const fui::Color color = fui::Color::Black) {
  fui::TextStyle s;
  s.font = font;
  s.align = align;
  s.color = color;
  s.maxLines = 1;
  screen.target().text(toybox::inkCentred(r, cut), text, s);
}

int textWidth(toybox::Screen& screen, const char* text, const fui::FontId font) {
  fui::TextStyle s;
  s.font = font;
  return screen.target().measureText(font, text, s).width;
}

void disc(toybox::Screen& screen, const int cx, const int cy, const int r, const fui::Color c) {
  if (r > 0) toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), c);
}

// A ring drawn as a degree sign: the font has no U+00B0.
void degree(toybox::Screen& screen, const int x, const int y, const int r, const int w) {
  disc(screen, x, y, r, fui::Color::Black);
  disc(screen, x, y, r - w, fui::Color::White);
}

// --- polygons ------------------------------------------------------------------

// Even-odd scanline fill, in whole pixels. Enough for the bolt, the rays and the
// raindrops, which is every diagonal on these screens.
void polygon(toybox::Screen& screen, const float* xs, const float* ys, const int n, const fui::Paint& paint) {
  float top = ys[0];
  float bottom = ys[0];
  for (int i = 1; i < n; ++i) {
    if (ys[i] < top) top = ys[i];
    if (ys[i] > bottom) bottom = ys[i];
  }
  float cross[8];
  for (int y = static_cast<int>(std::floor(top)); y <= static_cast<int>(std::ceil(bottom)); ++y) {
    const float sy = static_cast<float>(y) + 0.5f;
    int count = 0;
    for (int i = 0, j = n - 1; i < n && count < 8; j = i++) {
      if ((ys[i] <= sy && ys[j] > sy) || (ys[j] <= sy && ys[i] > sy)) {
        cross[count++] = xs[i] + (sy - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
      }
    }
    for (int a = 1; a < count; ++a) {
      for (int b = a; b > 0 && cross[b - 1] > cross[b]; --b) {
        const float t = cross[b];
        cross[b] = cross[b - 1];
        cross[b - 1] = t;
      }
    }
    for (int k = 0; k + 1 < count; k += 2) {
      const int x0 = static_cast<int>(std::lround(cross[k]));
      const int x1 = static_cast<int>(std::lround(cross[k + 1]));
      if (x1 > x0) box(screen, x0, y, x1 - x0, 1, paint);
    }
  }
}

// A straight stroke `w` thick from (x0, y0) to (x1, y1), square ends.
void stroke(toybox::Screen& screen, const float x0, const float y0, const float x1, const float y1, const float w,
            const fui::Paint& paint = kInk) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0) return;
  const float nx = -dy / len * w / 2;
  const float ny = dx / len * w / 2;
  const float xs[4] = {x0 + nx, x1 + nx, x1 - nx, x0 - nx};
  const float ys[4] = {y0 + ny, y1 + ny, y1 - ny, y0 - ny};
  polygon(screen, xs, ys, 4, paint);
}

// --- sky glyphs ----------------------------------------------------------------
//
// One family: every outline is the same weight for a given size, the sun and
// moon are solid, and a cloud in front of something occludes it.

int weightFor(const int s) { return s * 7 / 100 > 2 ? s * 7 / 100 : 2; }

void sun(toybox::Screen& screen, const float cx, const float cy, const int s) {
  const int r = s * 19 / 100;
  disc(screen, static_cast<int>(cx), static_cast<int>(cy), r, fui::Color::Black);
  const float in = static_cast<float>(r) + static_cast<float>(s) * 0.08f;
  const float out = in + static_cast<float>(s) * 0.13f;
  const int ray = s * 5 / 100 > 2 ? s * 5 / 100 : 2;
  for (int k = 0; k < 8; ++k) {
    const float a = static_cast<float>(k) * 0.785398f;
    const float ux = std::cos(a);
    const float uy = std::sin(a);
    stroke(screen, cx + ux * in, cy + uy * in, cx + ux * out, cy + uy * out, static_cast<float>(ray));
  }
}

void moon(toybox::Screen& screen, const int cx, const int cy, const int s) {
  const int r = s * 27 / 100;
  disc(screen, cx, cy, r, fui::Color::Black);
  disc(screen, cx + r * 50 / 100, cy - r * 36 / 100, r * 80 / 100, fui::Color::White);
}

void sunOrMoon(toybox::Screen& screen, const bool day, const int cx, const int cy, const int s) {
  if (day) {
    sun(screen, static_cast<float>(cx), static_cast<float>(cy), s);
  } else {
    moon(screen, cx, cy, s);
  }
}

// Three lobes on a flat base, `cw` wide, base at `base`. `inset` shrinks the
// whole silhouette, which is how the outline is cut out of the solid.
void cloudShape(toybox::Screen& screen, const int cx, const int base, const int cw, const int inset,
                const fui::Color colour) {
  const int r1 = cw * 22 / 100 - inset;
  const int r2 = cw * 30 / 100 - inset;
  const int r3 = cw * 20 / 100 - inset;
  const int left = cx - cw / 2;
  const int right = cx + cw / 2;
  const int baseY = base - inset;
  const int c1x = left + cw * 22 / 100;
  const int c3x = right - cw * 20 / 100;
  disc(screen, c1x, base - cw * 22 / 100, r1, colour);
  disc(screen, cx - cw / 25, base - cw * 34 / 100, r2, colour);
  disc(screen, c3x, base - cw * 20 / 100, r3, colour);
  const int bodyTop = base - cw * 22 / 100;
  box(screen, c1x, bodyTop, c3x - c1x, baseY - bodyTop, fui::Paint::solid(colour));
}

void cloud(toybox::Screen& screen, const int cx, const int base, const int cw, const int w) {
  cloudShape(screen, cx, base, cw, 0, fui::Color::Black);
  cloudShape(screen, cx, base, cw, w, fui::Color::White);
}

void drop(toybox::Screen& screen, const float x, const float y, const float len, const int w) {
  stroke(screen, x, y, x - len * 0.38f, y + len, static_cast<float>(w));
}

void flake(toybox::Screen& screen, const float x, const float y, const float arm, const int w) {
  for (int k = 0; k < 3; ++k) {
    const float a = static_cast<float>(k) * 1.0472f + 0.5236f;
    const float ux = std::cos(a) * arm;
    const float uy = std::sin(a) * arm;
    stroke(screen, x - ux, y - uy, x + ux, y + uy, static_cast<float>(w));
  }
}

void bolt(toybox::Screen& screen, const int cx, const int top, const int s) {
  const float u = static_cast<float>(s);
  const float px[6] = {0.10f, -0.13f, 0.00f, -0.08f, 0.17f, 0.04f};
  const float py[6] = {0.00f, 0.30f, 0.30f, 0.52f, 0.20f, 0.20f};
  float xs[6];
  float ys[6];
  // A paper halo first, so the bolt stays separate where it crosses the cloud.
  for (int i = 0; i < 6; ++i) {
    xs[i] = static_cast<float>(cx) + px[i] * u * 1.35f;
    ys[i] = static_cast<float>(top) + (py[i] - 0.04f) * u * 1.2f;
  }
  polygon(screen, xs, ys, 6, kPaper);
  for (int i = 0; i < 6; ++i) {
    xs[i] = static_cast<float>(cx) + px[i] * u;
    ys[i] = static_cast<float>(top) + py[i] * u;
  }
  polygon(screen, xs, ys, 6, kInk);
}

}  // namespace

void skyIcon(toybox::Screen& screen, const wx::Sky sky, const bool day, const int cx, const int cy, const int size) {
  const int s = size;
  const int w = weightFor(s);
  switch (sky) {
    case wx::Sky::Clear:
      sunOrMoon(screen, day, cx, cy, day ? s : s * 115 / 100);
      return;
    case wx::Sky::PartCloud:
      sunOrMoon(screen, day, cx + s * 15 / 100, cy - s * 13 / 100, s * 72 / 100);
      cloud(screen, cx - s * 7 / 100, cy + s * 33 / 100, s * 76 / 100, w);
      return;
    case wx::Sky::Cloud:
      cloud(screen, cx + s * 14 / 100, cy + s * 4 / 100, s * 58 / 100, w);
      cloud(screen, cx - s * 6 / 100, cy + s * 30 / 100, s * 78 / 100, w);
      return;
    case wx::Sky::Fog: {
      cloud(screen, cx, cy + s * 6 / 100, s * 70 / 100, w);
      const int barH = w;
      box(screen, cx - s * 38 / 100, cy + s * 16 / 100, s * 76 / 100, barH, kInk, barH / 2);
      box(screen, cx - s * 30 / 100, cy + s * 28 / 100, s * 60 / 100, barH, kInk, barH / 2);
      box(screen, cx - s * 38 / 100, cy + s * 40 / 100, s * 48 / 100, barH, kInk, barH / 2);
      return;
    }
    case wx::Sky::Drizzle:
    case wx::Sky::Rain:
    case wx::Sky::Showers: {
      const bool showers = sky == wx::Sky::Showers;
      if (showers) sunOrMoon(screen, day, cx + s * 18 / 100, cy - s * 20 / 100, s * 58 / 100);
      const int base = cy + s * 10 / 100;
      cloud(screen, cx - (showers ? s * 5 / 100 : 0), base, s * 80 / 100, w);
      const float top = static_cast<float>(base + w + s * 4 / 100);
      if (sky == wx::Sky::Drizzle) {
        for (int i = 0; i < 3; ++i) {
          const float x = static_cast<float>(cx - s * 18 / 100 + i * s * 18 / 100);
          drop(screen, x, top, static_cast<float>(s) * 0.08f, w);
          drop(screen, x - static_cast<float>(s) * 0.07f, top + static_cast<float>(s) * 0.17f,
               static_cast<float>(s) * 0.08f, w);
        }
      } else {
        for (int i = 0; i < 3; ++i) {
          drop(screen, static_cast<float>(cx - s * 14 / 100 + i * s * 18 / 100), top, static_cast<float>(s) * 0.26f, w);
        }
      }
      return;
    }
    case wx::Sky::Snow: {
      const int base = cy + s * 8 / 100;
      cloud(screen, cx, base, s * 80 / 100, w);
      const float y = static_cast<float>(base + s * 16 / 100);
      const float arm = static_cast<float>(s) * 0.08f;
      const int fw = w > 2 ? w - 1 : 2;
      flake(screen, static_cast<float>(cx - s * 22 / 100), y, arm, fw);
      flake(screen, static_cast<float>(cx), y + static_cast<float>(s) * 0.12f, arm, fw);
      flake(screen, static_cast<float>(cx + s * 22 / 100), y, arm, fw);
      return;
    }
    case wx::Sky::Thunder:
      cloud(screen, cx, cy + s * 10 / 100, s * 82 / 100, w);
      bolt(screen, cx, cy - s * 2 / 100, s);
      return;
  }
}

namespace {

// --- chrome and shared pieces --------------------------------------------------

void chrome(toybox::Screen& screen, const char* title, const char* trailing, const fui::ActionId action) {
  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  if (trailing) {
    header.trailingLabel = trailing;
    header.trailingAction = action;
    header.trailingStyles = toybox::bandOutlineStyles();
    header.trailingText = screen.theme().smallText;
    header.trailingText.font = toybox::kSmallFont;
    header.trailingText.color = fui::Color::White;
    header.trailingRadius = toybox::kPillRadius / 2;
  }
  toybox::headerBand(screen, header);
}

void tempText(char* out, const size_t cap, const float c, const bool f) {
  std::snprintf(out, cap, "%d", wx::displayTemp(c, f));
}

// A number in the 20px cut with its degree ring, ink centred on (cx, cy).
void smallTemp(toybox::Screen& screen, const int cx, const int cy, const float c, const bool f) {
  char t[toybox::kIntChars + 1];
  tempText(t, sizeof(t), c, f);
  const int tw = textWidth(screen, t, toybox::kUiFont);
  const int ring = 4;
  // The cut's advance carries trailing air; the ring tucks into it.
  const int total = tw + ring * 2;
  const int x = cx - total / 2;
  const int inkTop = cy - toybox::kUiCut.inkHeight / 2;
  label(screen, rect(x, inkTop, tw + 2, toybox::kUiCut.inkHeight), t, toybox::kUiFont, toybox::kUiCut,
        fui::TextAlign::Left);
  degree(screen, x + tw + ring - 1, inkTop + ring, ring, 2);
}

void hairline(toybox::Screen& screen, const int x, const int y, const int w) {
  box(screen, x, y, w, toybox::kHairline, kInk);
}

// The small caption cut, ink top at `y`.
void caption(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
             const fui::TextAlign align = fui::TextAlign::Left) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align);
}

// A short wrapped paragraph in the caption cut, centred.
void paragraph(toybox::Screen& screen, const int x, const int y, const int w, const char* text) {
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Center;
  s.maxLines = 4;
  screen.target().text(rect(x, y, w, toybox::kTileCut.lineHeight * 4), text, s);
}

bool isDaylight(const int hour, const wx::Day& today) {
  int rise = 7;
  int set = 19;
  if (today.sunrise[0]) rise = (today.sunrise[0] - '0') * 10 + (today.sunrise[1] - '0');
  if (today.sunset[0]) set = (today.sunset[0] - '0') * 10 + (today.sunset[1] - '0');
  return hour >= rise && hour <= set;
}

int minutesOf(const char* hhmm) {
  if (!hhmm[0] || !hhmm[1] || !hhmm[2] || !hhmm[3] || !hhmm[4]) return -1;
  return ((hhmm[0] - '0') * 10 + (hhmm[1] - '0')) * 60 + (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
}

// --- forecast ------------------------------------------------------------------

void hero(toybox::Screen& screen, const ForecastModel& model, const int top, const int height, const int width) {
  const wx::Forecast& f = *model.forecast;
  const bool fahr = model.fahrenheit;
  const int iconSize = height - 8;
  const int iconCx = toybox::kMargin + iconSize / 2 + 2;
  skyIcon(screen, wx::skyOf(f.current.code), f.current.isDay, iconCx, top + height / 2, iconSize);

  const int left = toybox::kMargin + iconSize + toybox::kGutter;
  const int right = width - toybox::kMargin;
  const int colW = right - left;

  // The temperature, in the huge cut when the activity lent the swap.
  char t[toybox::kIntChars + 1];
  tempText(t, sizeof(t), f.current.temp, fahr);
  const bool huge = model.faces.bindTitle != nullptr;
  const toybox::CutMetrics& cut = huge ? toybox::kHugeCut : toybox::kDisplayCut;
  if (huge) model.faces.bindTitle(model.faces.ctx, true);
  const int tw = textWidth(screen, t, toybox::kDisplayFont);
  const int ringR = cut.inkHeight / 7;
  const int ringW = ringR * 2 / 5 > 3 ? ringR * 2 / 5 : 3;
  const int total = tw + ringR * 2 + 6;
  const int x = left + (colW - total) / 2;
  const int inkTop = top + 6;
  label(screen, rect(x, inkTop, tw + 4, cut.inkHeight), t, toybox::kDisplayFont, cut, fui::TextAlign::Left);
  if (huge) model.faces.bindTitle(model.faces.ctx, false);
  degree(screen, x + tw + 6 + ringR, inkTop + ringR, ringR, ringW);

  int y = inkTop + cut.inkHeight + 14;
  label(screen, rect(left, y, colW, toybox::kUiCut.inkHeight), wx::labelOf(f.current.code), toybox::kUiFont,
        toybox::kUiCut);
  y += toybox::kUiCut.inkHeight + 10;
  if (f.dayCount > 0) {
    char hl[2 * toybox::kIntChars + 16];
    std::snprintf(hl, sizeof(hl), "HIGH %d   LOW %d", wx::displayTemp(f.days[0].max, fahr),
                  wx::displayTemp(f.days[0].min, fahr));
    caption(screen, left, y, colW, hl, fui::TextAlign::Center);
  }
}

// Four cells: a caption over a value, split by hairlines.
void stats(toybox::Screen& screen, const wx::Forecast& f, const bool fahr, const int top, const int height,
           const int width) {
  const int inner = width - toybox::kMargin * 2;
  const int cellW = inner / 4;
  char hum[toybox::kIntChars + 2];
  std::snprintf(hum, sizeof(hum), "%d%%", f.current.humidity);
  char wind[toybox::kIntChars + 1];
  std::snprintf(wind, sizeof(wind), "%d", wx::displayWind(f.current.wind, fahr));

  // The next sun event: sunrise before dawn, sunset during the day, and
  // tomorrow's sunrise after dark.
  const char* sunCaption = "SUNSET";
  const char* sunValue = f.days[0].sunset;
  const int now = f.current.time[0] ? minutesOf(f.current.time + 11) : -1;
  if (now >= 0 && now < minutesOf(f.days[0].sunrise)) {
    sunCaption = "SUNRISE";
    sunValue = f.days[0].sunrise;
  } else if (now >= 0 && now >= minutesOf(f.days[0].sunset) && f.dayCount > 1) {
    sunCaption = "SUNRISE";
    sunValue = f.days[1].sunrise;
  }

  const char* captions[4] = {"FEELS LIKE", "HUMIDITY", fahr ? "WIND MPH" : "WIND KM/H", sunCaption};
  const char* values[4] = {"", hum, wind, sunValue};
  const int capY = top + 9;
  const int valY = capY + toybox::kTileCut.inkHeight + 9;
  for (int i = 0; i < 4; ++i) {
    const int x = toybox::kMargin + i * cellW;
    if (i > 0) box(screen, x, top + 8, toybox::kHairline, height - 16, kInk);
    caption(screen, x, capY, cellW, captions[i], fui::TextAlign::Center);
    if (i == 0) {
      smallTemp(screen, x + cellW / 2, valY + toybox::kUiCut.inkHeight / 2, f.current.feels, fahr);
    } else {
      label(screen, rect(x, valY, cellW, toybox::kUiCut.inkHeight), values[i], toybox::kUiFont, toybox::kUiCut);
    }
  }
}

// The next day as a curve. Eight columns three hours apart from now: hour,
// sky, then the temperature riding on a line drawn through every hour between
// them, and the chance of rain where it is worth saying.
void hours(toybox::Screen& screen, const wx::Forecast& f, const bool fahr, const int top, const int width) {
  const int cols = 8;
  const int inner = width - toybox::kMargin * 2;
  const int colW = inner / cols;
  const int icon = 36;
  const int first = wx::nowIndex(f);
  int n = f.hourCount - first;
  if (n > (cols - 1) * 3 + 1) n = (cols - 1) * 3 + 1;
  if (n < 1) return;

  const int hourY = top;
  const int iconCy = hourY + toybox::kTileCut.inkHeight + 8 + icon / 2;
  const int zoneTop = iconCy + icon / 2 + 6;
  const int numberH = toybox::kUiCut.inkHeight;
  const int curveTop = zoneTop + numberH + 10;  // the highest a point may sit
  const int curveH = 34;
  const int zoneBottom = curveTop + curveH + 6;
  const int rainY = zoneBottom + 8;

  float lo = f.hours[first].temp;
  float hi = lo;
  for (int i = 1; i < n; ++i) {
    const float t = f.hours[first + i].temp;
    if (t < lo) lo = t;
    if (t > hi) hi = t;
  }
  const float span = hi - lo > 0.5f ? hi - lo : 0.5f;
  const float x0 = static_cast<float>(toybox::kMargin + colW / 2);
  const float step = static_cast<float>(colW) / 3.0f;
  auto px = [&](const int i) { return x0 + step * static_cast<float>(i); };
  auto py = [&](const int i) {
    return static_cast<float>(curveTop) + static_cast<float>(curveH) * (hi - f.hours[first + i].temp) / span;
  };

  // Shade under the line, then the line, then a dot at each labelled hour.
  const fui::Paint shade = fui::Paint::dither(fui::Color::LightGray);
  for (int x = static_cast<int>(px(0)); x <= static_cast<int>(px(n - 1)); ++x) {
    const float t = (static_cast<float>(x) - x0) / step;
    int i = static_cast<int>(t);
    if (i >= n - 1) i = n - 2;
    if (i < 0) break;
    const float fr = t - static_cast<float>(i);
    const int y = static_cast<int>(py(i) + (py(i + 1) - py(i)) * fr);
    box(screen, x, y, 1, zoneBottom - y, shade);
  }
  for (int i = 0; i + 1 < n; ++i) {
    stroke(screen, px(i), py(i), px(i + 1), py(i + 1), 3);
    disc(screen, static_cast<int>(px(i + 1)), static_cast<int>(py(i + 1)), 1, fui::Color::Black);
  }
  box(screen, toybox::kMargin, zoneBottom, inner, toybox::kHairline, kInk);

  for (int c = 0; c < cols; ++c) {
    const int i = c * 3;
    if (i >= n) break;
    const wx::Hour& h = f.hours[first + i];
    const int x = toybox::kMargin + c * colW;
    const int cx = x + colW / 2;
    char hh[toybox::kIntChars + 1];
    if (c == 0) {
      std::snprintf(hh, sizeof(hh), "NOW");
    } else {
      std::snprintf(hh, sizeof(hh), "%02d", h.hour);
    }
    caption(screen, x, hourY, colW, hh, fui::TextAlign::Center);
    const wx::Day& sunDay = f.days[h.day >= 0 ? h.day : 0];
    skyIcon(screen, wx::skyOf(h.code), isDaylight(h.hour, sunDay), cx, iconCy, icon);
    const int y = static_cast<int>(py(i));
    disc(screen, cx, y, 6, fui::Color::White);
    disc(screen, cx, y, 5, fui::Color::Black);
    disc(screen, cx, y, 2, fui::Color::White);
    smallTemp(screen, cx, y - 10 - numberH / 2, h.temp, fahr);
    if (h.precip >= 20) {
      char p[toybox::kIntChars + 2];
      std::snprintf(p, sizeof(p), "%d%%", h.precip);
      caption(screen, x, rainY, colW, p, fui::TextAlign::Center);
    }
  }
}

void dayName(char* out, const size_t cap, const wx::Day& d, const int index) {
  if (index == 0) {
    std::snprintf(out, cap, "TODAY");
  } else {
    std::snprintf(out, cap, "%s %d", wx::weekdayName(wx::weekday(d.year, d.month, d.day)), d.day);
  }
}

// A row with nothing drawn but its hit rect: the content is laid over it.
fui::StyleSet bareStyles() {
  fui::StyleSet styles = toybox::rowStyles();
  styles.normal.borderWidth = 0;
  styles.selected = styles.normal;
  styles.focused = styles.normal;
  styles.active = styles.normal;
  styles.disabled = styles.normal;
  return styles;
}

// A small chevron pointing right: "this opens".
void chevronRight(toybox::Screen& screen, const int cx, const int cy) {
  for (int i = 0; i <= 5; ++i) {
    box(screen, cx - 3 + i, cy - 5 + i, 2, 1, kInk);
    box(screen, cx - 3 + i, cy + 5 - i, 2, 1, kInk);
  }
}

// A row per day: name, sky, chance of rain, then the low and high with the
// day's span drawn against the whole week's. Today's row marks where the
// temperature is now. Each row opens its day.
void days(toybox::Screen& screen, const wx::Forecast& f, const bool fahr, const int top, const int rowH,
          const int width) {
  float lo = f.days[0].min;
  float hi = f.days[0].max;
  for (int i = 1; i < f.dayCount; ++i) {
    if (f.days[i].min < lo) lo = f.days[i].min;
    if (f.days[i].max > hi) hi = f.days[i].max;
  }
  if (f.current.temp < lo) lo = f.current.temp;
  if (f.current.temp > hi) hi = f.current.temp;
  const float span = hi - lo > 1 ? hi - lo : 1;
  const int x0 = toybox::kMargin;
  const int inner = width - toybox::kMargin * 2;
  const int nameW = 84;
  const int icon = rowH - 4 < 30 ? rowH - 4 : 30;
  const int iconCx = x0 + nameW + icon / 2;
  const int rainX = x0 + nameW + icon + 8;
  const int rainW = 46;
  const int numW = 40;
  const int chevronW = 18;
  const int loX = rainX + rainW;
  const int hiX = x0 + inner - chevronW - numW;
  const int trackL = loX + numW + 12;
  const int trackR = hiX - 12;
  const int trackW = trackR - trackL;
  for (int i = 0; i < f.dayCount; ++i) {
    const wx::Day& d = f.days[i];
    const int y = top + i * rowH;
    const int cy = y + rowH / 2;
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionDay;
    row.value = static_cast<int16_t>(i);
    row.styles = bareStyles();
    // Rows are shorter than a fingertip and stacked: each takes exactly its
    // own band rather than growing into its neighbours.
    row.minTouchSize = 0;
    screen.button(row, rect(x0, y + 1, inner, rowH - 1));
    if (i > 0) hairline(screen, x0, y, inner);
    char name[16];
    dayName(name, sizeof(name), d, i);
    caption(screen, x0, cy - toybox::kTileCut.inkHeight / 2, nameW, name);
    skyIcon(screen, wx::skyOf(d.code), true, iconCx, cy, icon);
    if (d.precip >= 20) {
      char p[toybox::kIntChars + 2];
      std::snprintf(p, sizeof(p), "%d%%", d.precip);
      caption(screen, rainX, cy - toybox::kTileCut.inkHeight / 2, rainW, p);
    }
    char low[toybox::kIntChars + 1];
    char high[toybox::kIntChars + 1];
    tempText(low, sizeof(low), d.min, fahr);
    tempText(high, sizeof(high), d.max, fahr);
    label(screen, rect(loX, cy - toybox::kUiCut.inkHeight / 2, numW, toybox::kUiCut.inkHeight), low, toybox::kUiFont,
          toybox::kUiCut, fui::TextAlign::Right);
    label(screen, rect(hiX, cy - toybox::kUiCut.inkHeight / 2, numW, toybox::kUiCut.inkHeight), high, toybox::kUiFont,
          toybox::kUiCut, fui::TextAlign::Right);
    box(screen, trackL, cy, trackW, toybox::kHairline, fui::Paint::dither(fui::Color::DarkGray));
    const int a = trackL + static_cast<int>((d.min - lo) / span * static_cast<float>(trackW));
    const int b = trackL + static_cast<int>((d.max - lo) / span * static_cast<float>(trackW));
    const int barH = 10;
    box(screen, a, cy - barH / 2, b - a > barH ? b - a : barH, barH, kInk, barH / 2);
    if (i == 0) {
      const int nx = trackL + static_cast<int>((f.current.temp - lo) / span * static_cast<float>(trackW));
      disc(screen, nx, cy, 8, fui::Color::White);
      disc(screen, nx, cy, 6, fui::Color::Black);
      disc(screen, nx, cy, 3, fui::Color::White);
    }
    chevronRight(screen, x0 + inner - 6, cy);
  }
}

void footer(toybox::Screen& screen, const wx::Forecast* f, const int top, const int height, const int width,
            const int autoMinutes, const char* note = nullptr) {
  char when[48] = "NOT FETCHED YET";
  if (note != nullptr) {
    std::snprintf(when, sizeof(when), "%s", note);
  } else if (f && f->current.time[0]) {
    const wx::Current& c = f->current;
    int y = 0;
    int m = 0;
    int d = 0;
    std::sscanf(c.time, "%d-%d-%d", &y, &m, &d);
    std::snprintf(when, sizeof(when), "%s %d %s, %.5s", wx::weekdayName(wx::weekday(y, m, d)), d % 100,
                  wx::monthName(m), c.time + 11);
  }
  char autoLabel[toybox::kIntChars + 12];
  if (autoMinutes <= 0) {
    std::snprintf(autoLabel, sizeof(autoLabel), "AUTO OFF");
  } else if (autoMinutes >= 60) {
    std::snprintf(autoLabel, sizeof(autoLabel), "AUTO %d H", autoMinutes / 60);
  } else {
    std::snprintf(autoLabel, sizeof(autoLabel), "AUTO %d M", autoMinutes);
  }
  const int bw = 112;
  const int gap = 8;
  const int bh = height - 8 < 44 ? height - 8 : 44;
  const int by = top + (height - bh) / 2 + 1;
  caption(screen, toybox::kMargin, top + (height - toybox::kTileCut.inkHeight) / 2,
          width - toybox::kMargin * 2 - bw * 2 - gap - toybox::kGutter, when);
  fui::ButtonProps a;
  a.label = autoLabel;
  a.action = ActionAuto;
  a.styles = toybox::rowStyles();
  a.text = toybox::buttonText(screen.theme());
  a.radius = toybox::kPillRadius / 2;
  a.state = autoMinutes > 0 ? fui::StateSelected : fui::StateNormal;
  screen.button(a, rect(width - toybox::kMargin - bw * 2 - gap, by, bw, bh));
  fui::ButtonProps b;
  b.label = "REFRESH";
  b.action = ActionRefresh;
  b.styles = toybox::rowStyles();
  b.text = toybox::buttonText(screen.theme());
  b.radius = toybox::kPillRadius / 2;
  screen.button(b, rect(width - toybox::kMargin - bw, by, bw, bh));
}

// --- one day ---------------------------------------------------------------------

void statCell(toybox::Screen& screen, const int x, const int y, const int w, const char* name, const char* value) {
  caption(screen, x, y, w, name, fui::TextAlign::Center);
  label(screen, rect(x, y + toybox::kTileCut.inkHeight + 8, w, toybox::kUiCut.inkHeight), value, toybox::kUiFont,
        toybox::kUiCut);
}

// Four captioned cells across, hairlines between.
void statRow(toybox::Screen& screen, const int top, const int width, const char* const names[4],
             const char* const values[4]) {
  const int inner = width - toybox::kMargin * 2;
  const int cellW = inner / 4;
  for (int i = 0; i < 4; ++i) {
    const int x = toybox::kMargin + i * cellW;
    if (i > 0) box(screen, x, top + 8, toybox::kHairline, 44, kInk);
    statCell(screen, x, top + 9, cellW, names[i], values[i]);
  }
}

// The day's temperature through its hours: night shaded, the line across it,
// the low and the high marked and named, and where now is when it is today.
void dayCurve(toybox::Screen& screen, const fui::Rect& plot, const wx::Forecast& f, const int d, const bool fahr) {
  int first = 0;
  const int n = wx::hoursOfDay(f, d, first);
  const wx::Day& day = f.days[d];
  auto xOfHour = [&](const float hour) {
    return static_cast<float>(plot.x) + static_cast<float>(plot.width) * hour / 24.0f;
  };

  // Night: before sunrise and after sunset.
  const float rise = wx::clockHours(day.sunrise);
  const float set = wx::clockHours(day.sunset);
  const fui::Paint night = fui::Paint::dither(fui::Color::LightGray);
  if (rise > 0 && set > rise) {
    box(screen, plot.x, plot.y, static_cast<int>(xOfHour(rise)) - plot.x, plot.height, night);
    const int sx = static_cast<int>(xOfHour(set));
    box(screen, sx, plot.y, plot.x + plot.width - sx, plot.height, night);
  }
  box(screen, plot.x, plot.y + plot.height, plot.width, toybox::kHairline, kInk);
  if (n < 2) {
    label(screen, rect(plot.x, plot.y + plot.height / 2 - 10, plot.width, toybox::kTileCut.inkHeight),
          "NO HOURLY DATA FOR THIS DAY. REFRESH TO GET IT.", toybox::kTileFont, toybox::kTileCut);
    return;
  }

  int loAt = 0;
  int hiAt = 0;
  for (int i = 1; i < n; ++i) {
    if (f.hours[first + i].temp < f.hours[first + loAt].temp) loAt = i;
    if (f.hours[first + i].temp > f.hours[first + hiAt].temp) hiAt = i;
  }
  const float lo = f.hours[first + loAt].temp;
  const float hi = f.hours[first + hiAt].temp;
  const float span = hi - lo > 0.5f ? hi - lo : 0.5f;
  const int padTop = toybox::kUiCut.inkHeight + 16;  // room for the high's label
  const int padBottom = 14;
  auto px = [&](const int i) { return xOfHour(static_cast<float>(f.hours[first + i].hour) + 0.5f); };
  auto py = [&](const int i) {
    return static_cast<float>(plot.y + padTop) +
           static_cast<float>(plot.height - padTop - padBottom) * (hi - f.hours[first + i].temp) / span;
  };
  for (int i = 0; i + 1 < n; ++i) {
    stroke(screen, px(i), py(i), px(i + 1), py(i + 1), 4);
    disc(screen, static_cast<int>(px(i + 1)), static_cast<int>(py(i + 1)), 2, fui::Color::Black);
  }

  // The extremes, each with its figure: the high above its point, the low
  // above too (below would run off the plot), nudged in from the edges.
  auto mark = [&](const int i) {
    const int cx = static_cast<int>(px(i));
    const int cy = static_cast<int>(py(i));
    disc(screen, cx, cy, 8, fui::Color::White);
    disc(screen, cx, cy, 7, fui::Color::Black);
    disc(screen, cx, cy, 3, fui::Color::White);
    int lx = cx;
    if (lx < plot.x + 24) lx = plot.x + 24;
    if (lx > plot.x + plot.width - 24) lx = plot.x + plot.width - 24;
    const int ly = cy - 12 - toybox::kUiCut.inkHeight / 2;
    // A paper plate under the figure so it reads over the night shading.
    box(screen, lx - 22, ly - toybox::kUiCut.inkHeight / 2 - 3, 44, toybox::kUiCut.inkHeight + 6, kPaper, 6);
    smallTemp(screen, lx, ly, f.hours[first + i].temp, fahr);
  };
  mark(hiAt);
  if (loAt != hiAt) mark(loAt);

  // Now, on today's sheet: a tick down from the line to the axis.
  if (d == 0) {
    const int now = wx::nowIndex(f) - first;
    if (now >= 0 && now < n) {
      const int cx = static_cast<int>(px(now));
      const int cy = static_cast<int>(py(now));
      for (int y = cy + 8; y < plot.y + plot.height; y += 5) box(screen, cx - 1, y, 2, 3, kInk);
      disc(screen, cx, cy, 5, fui::Color::Black);
    }
  }
}

// The chance of rain through the day, a bar an hour.
void rainBars(toybox::Screen& screen, const fui::Rect& plot, const wx::Forecast& f, const int d) {
  int first = 0;
  const int n = wx::hoursOfDay(f, d, first);
  box(screen, plot.x, plot.y + plot.height, plot.width, toybox::kHairline, kInk);
  int peak = 0;
  for (int i = 0; i < n; ++i) {
    if (f.hours[first + i].precip > peak) peak = f.hours[first + i].precip;
  }
  if (n == 0 || peak < 5) {
    label(screen, rect(plot.x, plot.y + plot.height / 2 - 8, plot.width, toybox::kTileCut.inkHeight),
          n == 0 ? "NO HOURLY DATA" : "NO RAIN EXPECTED", toybox::kTileFont, toybox::kTileCut);
    return;
  }
  // The scale is always 0 to 100, so two days can be compared at a glance; a
  // dotted line marks the half.
  for (int x = plot.x; x < plot.x + plot.width; x += 6) box(screen, x, plot.y + plot.height / 2, 3, 1, kInk);
  const float slot = static_cast<float>(plot.width) / 24.0f;
  const int barW = static_cast<int>(slot) - 4;
  for (int i = 0; i < n; ++i) {
    const wx::Hour& h = f.hours[first + i];
    if (h.precip <= 0) continue;
    const int bh = plot.height * h.precip / 100;
    const int x = plot.x + static_cast<int>(slot * static_cast<float>(h.hour)) + 2;
    box(screen, x, plot.y + plot.height - (bh > 2 ? bh : 2), barW, bh > 2 ? bh : 2, kInk, 2);
  }
}

}  // namespace

void buildForecast(toybox::Screen& screen, const ForecastModel& model) {
  chrome(screen, model.place ? model.place->name : "WEATHER", "CITIES", ActionCities);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int footH = 48;
  const int footTop = device.height - kBottomReserve - footH;

  if (!model.forecast) {
    const int cy = toybox::kBodyTop + 150;
    skyIcon(screen, wx::Sky::PartCloud, true, width / 2, cy, 150);
    label(screen, rect(toybox::kMargin, cy + 110, inner, toybox::kDisplayCut.inkHeight), "NO FORECAST YET",
          toybox::kDisplayFont, toybox::kDisplayCut);
    paragraph(screen, toybox::kMargin * 3, cy + 110 + toybox::kDisplayCut.inkHeight + 24, width - toybox::kMargin * 6,
              "REFRESH JOINS WI-FI AND FETCHES THE FORECAST. IT IS KEPT ON THE CARD, SO IT STILL READS WITH NO "
              "NETWORK.");
    hairline(screen, toybox::kMargin, footTop, inner);
    footer(screen, nullptr, footTop, footH, width, model.autoMinutes);
    return;
  }

  const wx::Forecast& f = *model.forecast;
  int y = toybox::kChromeHeight + 10;
  const int heroH = 156;
  hero(screen, model, y, heroH, width);
  y += heroH + 4;
  hairline(screen, toybox::kMargin, y, inner);
  const int statsH = 62;
  stats(screen, f, model.fahrenheit, y, statsH, width);
  y += statsH;
  hairline(screen, toybox::kMargin, y, inner);
  y += 12;
  const int hoursH = 164;
  hours(screen, f, model.fahrenheit, y, width);
  y += hoursH;
  hairline(screen, toybox::kMargin, y, inner);
  const int rowH = (footTop - y) / (f.dayCount > 0 ? f.dayCount : 1);
  days(screen, f, model.fahrenheit, y, rowH, width);
  hairline(screen, toybox::kMargin, footTop, inner);
  footer(screen, &f, footTop, footH, width, model.autoMinutes);
}

void buildDay(toybox::Screen& screen, const DayModel& model) {
  const wx::Forecast& f = *model.forecast;
  const int d = model.day >= 0 && model.day < f.dayCount ? model.day : 0;
  const wx::Day& day = f.days[d];
  const bool fahr = model.fahrenheit;
  char title[24];
  std::snprintf(title, sizeof(title), "%s %d", wx::weekdayLong(wx::weekday(day.year, day.month, day.day)), day.day);
  chrome(screen, title, nullptr, 0);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int x = toybox::kMargin;

  int y = toybox::kChromeHeight + 14;
  char where[64];
  std::snprintf(where, sizeof(where), "%s, %d %s", model.place ? model.place->name : "", day.day,
                wx::monthName(day.month));
  caption(screen, x, y, inner, where);
  caption(screen, x, y, inner, d == 0 ? "TODAY" : (d == 1 ? "TOMORROW" : ""), fui::TextAlign::Right);
  y += toybox::kTileCut.inkHeight + 10;

  // The sky, the high in the huge cut, the low beside it, the day in words.
  const int heroH = 124;
  const int iconSize = heroH - 4;
  skyIcon(screen, wx::skyOf(day.code), true, x + iconSize / 2 + 2, y + heroH / 2, iconSize);
  const int left = x + iconSize + toybox::kGutter;
  const int colW = x + inner - left;
  char t[toybox::kIntChars + 1];
  tempText(t, sizeof(t), day.max, fahr);
  const bool huge = model.faces.bindTitle != nullptr;
  const toybox::CutMetrics& cut = huge ? toybox::kHugeCut : toybox::kDisplayCut;
  if (huge) model.faces.bindTitle(model.faces.ctx, true);
  const int tw = textWidth(screen, t, toybox::kDisplayFont);
  const int ringR = cut.inkHeight / 7;
  const int ringW = ringR * 2 / 5 > 3 ? ringR * 2 / 5 : 3;
  const int lowW = 86;
  const int total = tw + ringR * 2 + 6 + 14 + lowW;
  const int tx = left + (colW - total) / 2;
  label(screen, rect(tx, y + 2, tw + 4, cut.inkHeight), t, toybox::kDisplayFont, cut, fui::TextAlign::Left);
  if (huge) model.faces.bindTitle(model.faces.ctx, false);
  degree(screen, tx + tw + 6 + ringR, y + 2 + ringR, ringR, ringW);
  const int lowX = tx + tw + ringR * 2 + 6 + 14;
  caption(screen, lowX, y + 2 + cut.inkHeight - toybox::kUiCut.inkHeight - toybox::kTileCut.inkHeight - 8, lowW, "LOW",
          fui::TextAlign::Center);
  smallTemp(screen, lowX + lowW / 2, y + 2 + cut.inkHeight - toybox::kUiCut.inkHeight / 2, day.min, fahr);
  label(screen, rect(left, y + 2 + cut.inkHeight + 14, colW, toybox::kUiCut.inkHeight), wx::labelOf(day.code),
        toybox::kUiFont, toybox::kUiCut);
  y += heroH + 8;

  // Eight figures in two rows.
  hairline(screen, x, y, inner);
  char daylight[2 * toybox::kIntChars + 4] = "-";
  const float rise = wx::clockHours(day.sunrise);
  const float set = wx::clockHours(day.sunset);
  if (rise >= 0 && set > rise) {
    const int minutes = static_cast<int>((set - rise) * 60.0f + 0.5f);
    std::snprintf(daylight, sizeof(daylight), "%dH %02d", minutes / 60, minutes % 60);
  }
  char uv[16] = "-";
  if (day.uvMax >= 0) std::snprintf(uv, sizeof(uv), "%d", static_cast<int>(day.uvMax + 0.5f));
  const char* const names1[4] = {"SUNRISE", "SUNSET", "DAYLIGHT", "UV INDEX"};
  const char* const values1[4] = {day.sunrise[0] ? day.sunrise : "-", day.sunset[0] ? day.sunset : "-", daylight, uv};
  statRow(screen, y, width, names1, values1);
  y += 60;
  hairline(screen, x, y, inner);
  char rain[16] = "-";
  if (day.rainMm >= 0) {
    if (fahr) {
      std::snprintf(rain, sizeof(rain), "%.2f", static_cast<double>(day.rainMm / 25.4f));
    } else {
      std::snprintf(rain, sizeof(rain), "%.1f", static_cast<double>(day.rainMm));
    }
  }
  char chance[toybox::kIntChars + 2];
  std::snprintf(chance, sizeof(chance), "%d%%", day.precip);
  char wind[toybox::kIntChars + 1] = "-";
  if (day.windMax >= 0) std::snprintf(wind, sizeof(wind), "%d", wx::displayWind(day.windMax, fahr));
  char feels[2 * toybox::kIntChars + 4] = "-";
  if (day.hasFeels) {
    std::snprintf(feels, sizeof(feels), "%d / %d", wx::displayTemp(day.feelsMin, fahr),
                  wx::displayTemp(day.feelsMax, fahr));
  }
  const char* const names2[4] = {fahr ? "RAIN IN" : "RAIN MM", "CHANCE", fahr ? "WIND MPH" : "WIND KM/H", "FEELS LIKE"};
  const char* const values2[4] = {rain, chance, wind, feels};
  statRow(screen, y, width, names2, values2);
  y += 60;
  hairline(screen, x, y, inner);
  y += 12;

  // Temperature, then the skies under it on the same hours, then rain.
  const int navH = 46;
  const int navY = device.height - kBottomReserve - navH - 2;
  const int rainH = 62;
  const int axisH = toybox::kTileCut.inkHeight + 8;
  const int iconsH = 34;
  const int captionH = toybox::kTileCut.inkHeight + 8;
  const int rainTop = navY - 12 - axisH - rainH;
  const int curveTop = y + captionH;
  const int curveH = rainTop - captionH - 8 - iconsH - 6 - curveTop;
  caption(screen, x, y, inner, "TEMPERATURE");
  caption(screen, x, y, inner, "SHADED: NIGHT", fui::TextAlign::Right);
  dayCurve(screen, rect(x, curveTop, inner, curveH), f, d, fahr);
  // A sky every three hours, centred on its hours.
  int first = 0;
  const int n = wx::hoursOfDay(f, d, first);
  const int iconCy = curveTop + curveH + 6 + iconsH / 2;
  for (int i = 0; i < n; ++i) {
    const wx::Hour& h = f.hours[first + i];
    if (h.hour % 3 != 1) continue;
    const int cx = x + inner * (h.hour * 2 + 1) / 48;
    skyIcon(screen, wx::skyOf(h.code), isDaylight(h.hour, day), cx, iconCy, 30);
  }
  caption(screen, x, rainTop - captionH, inner, "CHANCE OF RAIN OR SNOW, BY HOUR");
  rainBars(screen, rect(x, rainTop, inner, rainH), f, d);
  // One hour axis under both.
  const int axisY = rainTop + rainH + 7;
  static const char* const kTicks[5] = {"00", "06", "12", "18", "24"};
  for (int i = 0; i < 5; ++i) {
    const int tx2 = x + inner * i / 4;
    const fui::TextAlign align =
        i == 0 ? fui::TextAlign::Left : (i == 4 ? fui::TextAlign::Right : fui::TextAlign::Center);
    const int lx = i == 0 ? tx2 : (i == 4 ? tx2 - 40 : tx2 - 20);
    caption(screen, lx, axisY, 40, kTicks[i], align);
    if (i > 0 && i < 4) box(screen, tx2, rainTop + rainH, 1, 5, kInk);
  }

  // The days either side.
  const int gap = 10;
  const int half = (inner - gap) / 2;
  char prevText[24] = "";
  char nextText[24] = "";
  if (d > 0) {
    const wx::Day& p = f.days[d - 1];
    std::snprintf(prevText, sizeof(prevText), "< %s %d", wx::weekdayName(wx::weekday(p.year, p.month, p.day)), p.day);
  }
  if (d + 1 < f.dayCount) {
    const wx::Day& nx = f.days[d + 1];
    std::snprintf(nextText, sizeof(nextText), "%s %d >", wx::weekdayName(wx::weekday(nx.year, nx.month, nx.day)),
                  nx.day);
  }
  fui::ButtonProps prev;
  prev.label = prevText;
  prev.action = ActionDay;
  prev.value = static_cast<int16_t>(d - 1);
  prev.enabled = d > 0;
  prev.styles = toybox::rowStyles();
  prev.text = toybox::buttonText(screen.theme());
  prev.radius = toybox::kPillRadius / 2;
  if (d > 0) screen.button(prev, rect(x, navY, half, navH));
  fui::ButtonProps next = prev;
  next.label = nextText;
  next.value = static_cast<int16_t>(d + 1);
  next.enabled = true;
  if (d + 1 < f.dayCount) screen.button(next, rect(x + half + gap, navY, half, navH));
}

void buildCities(toybox::Screen& screen, const CitiesModel& model) {
  chrome(screen, "CITIES", nullptr, 0);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  int y = toybox::kBodyTop;
  const int rowH = 68;
  const int xW = 60;
  const int gap = 8;
  for (int i = 0; i < model.count; ++i) {
    const wx::Place& p = model.places[i];
    const bool on = i == model.selected;
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionSelect;
    row.value = static_cast<int16_t>(i);
    row.styles = toybox::rowStyles();
    row.state = on ? fui::StateSelected : fui::StateNormal;
    row.radius = toybox::kPillRadius / 2;
    // A city, then the arrow that moves it up the list, then the cross that removes it.
    const int upW = 52;
    const fui::Rect r = rect(toybox::kMargin, y, inner - xW - gap - upW - gap, rowH);
    screen.button(row, r);
    const fui::Color ink = on ? fui::Color::White : fui::Color::Black;
    // The last forecast on the card, at the row's right: sky and temperature.
    int textW = r.width - 32;
    const CityGlance* g = model.glance ? &model.glance[i] : nullptr;
    if (g && g->have) {
      const int gw = 104;
      textW -= gw;
      const int gx = r.x + r.width - gw - 8;
      // On the selected row the glance sits on a paper chip so the glyph keeps
      // its outline-on-paper reading.
      if (on) box(screen, gx, y + 10, gw, rowH - 20, kPaper, 8);
      skyIcon(screen, wx::skyOf(g->code), g->day, gx + 26, y + rowH / 2, 36);
      char t[toybox::kIntChars + 1];
      tempText(t, sizeof(t), g->temp, model.fahrenheit);
      const int tw = textWidth(screen, t, toybox::kUiFont);
      const int tx = gx + 52 + (gw - 52 - tw - 8) / 2;
      const int inkTop = y + (rowH - toybox::kUiCut.inkHeight) / 2;
      label(screen, rect(tx, inkTop, tw + 2, toybox::kUiCut.inkHeight), t, toybox::kUiFont, toybox::kUiCut,
            fui::TextAlign::Left);
      degree(screen, tx + tw + 3, inkTop + 4, 4, 2);
    }
    const int nameY = y + 14;
    label(screen, rect(r.x + 16, nameY, textW, toybox::kUiCut.inkHeight), p.name, toybox::kUiFont, toybox::kUiCut,
          fui::TextAlign::Left, ink);
    label(screen, rect(r.x + 16, nameY + toybox::kUiCut.inkHeight + 10, textW, toybox::kTileCut.inkHeight), p.region,
          toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left, ink);
    if (i > 0) {
      fui::ButtonProps up;
      up.label = "";
      up.action = ActionMoveUp;
      up.value = static_cast<int16_t>(i);
      up.styles = toybox::rowStyles();
      up.radius = toybox::kPillRadius / 2;
      const fui::Rect ur = rect(r.x + r.width + gap, y, upW, rowH);
      screen.button(up, ur);
      const float ux = static_cast<float>(ur.x + upW / 2);
      const float uy = static_cast<float>(y + rowH / 2);
      stroke(screen, ux - 10, uy + 5, ux, uy - 6, 3);
      stroke(screen, ux, uy - 6, ux + 10, uy + 5, 3);
    }
    fui::ButtonProps x;
    x.label = "";
    x.action = ActionRemove;
    x.value = static_cast<int16_t>(i);
    x.styles = toybox::rowStyles();
    x.radius = toybox::kPillRadius / 2;
    const fui::Rect xr = rect(width - toybox::kMargin - xW, y, xW, rowH);
    screen.button(x, xr);
    const float xc = static_cast<float>(xr.x + xW / 2);
    const float yc = static_cast<float>(y + rowH / 2);
    const float arm = 9;
    stroke(screen, xc - arm, yc - arm, xc + arm, yc + arm, 3);
    stroke(screen, xc - arm, yc + arm, xc + arm, yc - arm, 3);
    y += rowH + gap;
  }
  if (model.count == 0) {
    const int cy = y + 90;
    skyIcon(screen, wx::Sky::PartCloud, true, width / 2, cy, 130);
    label(screen, rect(toybox::kMargin, cy + 96, inner, toybox::kUiCut.inkHeight), "NO CITIES YET", toybox::kUiFont,
          toybox::kUiCut);
    y = cy + 150;
  }
  y += toybox::kGutter;
  if (model.count < wx::kMaxPlaces) {
    fui::ButtonProps add;
    add.label = "+ ADD A CITY";
    add.action = ActionAdd;
    add.styles = toybox::rowStyles();
    add.radius = toybox::kPillRadius / 2;
    screen.button(add, rect(toybox::kMargin, y, inner, toybox::kPillHeight));
  } else {
    caption(screen, toybox::kMargin, y + (toybox::kPillHeight - toybox::kTileCut.inkHeight) / 2, inner,
            "SIX CITIES. REMOVE ONE TO ADD ANOTHER.", fui::TextAlign::Center);
  }

  // Units sit at the foot, away from the list they do not belong to.
  const int unitsY = device.height - kBottomReserve - toybox::kPillHeight - toybox::kGutter;
  caption(screen, toybox::kMargin, unitsY - toybox::kTileCut.inkHeight - 12, inner, "UNITS");
  const int half = (inner - gap) / 2;
  fui::ButtonProps c;
  c.label = "CELSIUS, KM/H";
  c.action = ActionUnits;
  c.value = 0;
  c.styles = toybox::rowStyles();
  c.text = toybox::buttonText(screen.theme());
  c.radius = toybox::kPillRadius / 2;
  c.state = model.fahrenheit ? fui::StateNormal : fui::StateSelected;
  screen.button(c, rect(toybox::kMargin, unitsY, half, toybox::kPillHeight));
  fui::ButtonProps fb = c;
  fb.label = "FAHRENHEIT, MPH";
  fb.value = 1;
  fb.state = model.fahrenheit ? fui::StateSelected : fui::StateNormal;
  screen.button(fb, rect(toybox::kMargin + half + gap, unitsY, half, toybox::kPillHeight));
}

void buildResults(toybox::Screen& screen, const ResultsModel& model) {
  chrome(screen, "PICK A CITY", "SEARCH", ActionSearch);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  int y = toybox::kBodyTop;
  char found[64];
  if (model.count == 0) {
    std::snprintf(found, sizeof(found), "NOTHING FOUND FOR \"%.24s\"", model.query);
    label(screen, rect(toybox::kMargin, y + 90, inner, toybox::kUiCut.inkHeight), found, toybox::kUiFont,
          toybox::kUiCut);
    caption(screen, toybox::kMargin, y + 140, inner, "TRY THE CITY ALONE, WITHOUT THE COUNTRY", fui::TextAlign::Center);
    return;
  }
  std::snprintf(found, sizeof(found), "%d %s FOR \"%.24s\"", model.count, model.count == 1 ? "MATCH" : "MATCHES",
                model.query);
  caption(screen, toybox::kMargin, y - 8, inner, found);
  y += toybox::kTileCut.inkHeight + 8;
  const int rowH = 68;
  const int gap = 8;
  for (int i = 0; i < model.count; ++i) {
    const wx::Place& p = model.results[i];
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionPick;
    row.value = static_cast<int16_t>(i);
    row.styles = toybox::rowStyles();
    row.radius = toybox::kPillRadius / 2;
    const fui::Rect r = rect(toybox::kMargin, y, inner, rowH);
    screen.button(row, r);
    const int nameY = y + 14;
    label(screen, rect(r.x + 16, nameY, r.width - 32, toybox::kUiCut.inkHeight), p.name, toybox::kUiFont,
          toybox::kUiCut, fui::TextAlign::Left);
    label(screen, rect(r.x + 16, nameY + toybox::kUiCut.inkHeight + 10, r.width - 32, toybox::kTileCut.inkHeight),
          p.region, toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left);
    y += rowH + gap;
  }
}

void buildStatus(toybox::Screen& screen, const StatusModel& model) {
  chrome(screen, "WEATHER", nullptr, 0);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int cy = device.height / 2 - 120;
  skyIcon(screen, model.kind == StatusKind::Working ? wx::Sky::PartCloud : wx::Sky::Thunder, true, width / 2, cy, 140);
  const int headY = cy + 104;
  label(screen, rect(toybox::kMargin, headY, inner, toybox::kDisplayCut.inkHeight), model.headline,
        toybox::kDisplayFont, toybox::kDisplayCut);
  paragraph(screen, toybox::kMargin * 3, headY + toybox::kDisplayCut.inkHeight + 24, width - toybox::kMargin * 6,
            model.detail);
  if (model.dismissable) {
    fui::ButtonProps ok;
    ok.label = "OK";
    ok.action = ActionDismiss;
    ok.styles = toybox::rowStyles();
    ok.radius = toybox::kPillRadius / 2;
    screen.button(ok, rect(width / 2 - 90, headY + 170, 180, toybox::kPillHeight));
  }
}

void buildAutoPicker(toybox::Screen& screen, const int currentMinutes) {
  chrome(screen, "AUTO REFRESH", nullptr, 0);
  const fui::DeviceContext device = screen.device();
  const int inner = device.width - toybox::kMargin * 2;
  int y = toybox::kBodyTop - 12;
  caption(screen, toybox::kMargin, y, inner, "HOW OFTEN SHOULD IT FETCH BY ITSELF?");
  y += toybox::kTileCut.inkHeight + 16;
  for (int i = 0; i < kAutoChoiceCount; ++i) {
    const int minutes = kAutoChoices[i];
    char text[toybox::kIntChars + 16];
    if (minutes == 0) {
      std::snprintf(text, sizeof(text), "OFF");
    } else if (minutes == 60) {
      std::snprintf(text, sizeof(text), "EVERY HOUR");
    } else if (minutes > 60) {
      std::snprintf(text, sizeof(text), "EVERY %d HOURS", minutes / 60);
    } else {
      std::snprintf(text, sizeof(text), "EVERY %d MINUTES", minutes);
    }
    fui::ButtonProps b;
    b.label = text;
    b.action = ActionChooseAuto;
    b.value = static_cast<int16_t>(minutes);
    b.styles = toybox::rowStyles();
    b.radius = toybox::kPillRadius / 2;
    b.state = minutes == currentMinutes ? fui::StateSelected : fui::StateNormal;
    screen.button(b, rect(toybox::kMargin, y, inner, 60));
    y += 60 + 8;
  }
  y += 10;
  caption(screen, toybox::kMargin, y, inner, "WHILE IT IS ON THE PAGE STAYS ON THE SCREEN.");
  caption(screen, toybox::kMargin, y + toybox::kTileCut.inkHeight + 8, inner, "THE WI-FI COMES UP ONLY TO FETCH.");
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  chrome(screen, "WEATHER", "CITIES", ActionCities);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int footH = 48;
  const int footTop = device.height - kBottomReserve - footH;
  const int top = toybox::kChromeHeight + 8;
  const int gap = 8;
  const int size = model.size;
  const int tileH = (footTop - 8 - top - gap * (size - 1)) / size;
  const bool tall = size <= 3;
  const bool huge = tall && model.faces.bindTitle != nullptr;
  const toybox::CutMetrics& cut = huge ? toybox::kHugeCut : toybox::kDisplayCut;

  for (int i = 0; i < size; ++i) {
    const int y = top + (tileH + gap) * i;
    const int x = toybox::kMargin;
    fui::ButtonProps tile;
    tile.label = "";
    tile.styles = toybox::rowStyles();
    tile.radius = toybox::kPillRadius / 2;
    if (i >= model.count) {
      // An empty place on the board leads to where a city is added.
      tile.action = ActionCities;
      screen.button(tile, rect(x, y, inner, tileH));
      caption(screen, x, y + (tileH - toybox::kTileCut.inkHeight) / 2, inner, "+ ADD A CITY", fui::TextAlign::Center);
      continue;
    }
    tile.action = ActionBoardOpen;
    tile.value = static_cast<int16_t>(i);
    screen.button(tile, rect(x, y, inner, tileH));
    const wx::Place& p = model.places[i];
    const BoardCard& c = model.cards[i];
    const int pad = 16;
    label(screen, rect(x + pad, y + 12, inner - pad * 2 - 70, toybox::kUiCut.inkHeight), p.name, toybox::kUiFont,
          toybox::kUiCut, fui::TextAlign::Left);
    if (!c.have) {
      caption(screen, x + pad, y + 12 + toybox::kUiCut.inkHeight + 10, inner - pad * 2,
              "NOT FETCHED YET. TAP REFRESH.");
      continue;
    }
    caption(screen, x + pad, y + 12 + (toybox::kUiCut.inkHeight - toybox::kTileCut.inkHeight) / 2, inner - pad * 2,
            c.time, fui::TextAlign::Right);

    // Under the name: the sky, the temperature large, and the day's figures.
    const int bodyTop = y + 12 + toybox::kUiCut.inkHeight + 8;
    const int bodyH = y + tileH - 10 - bodyTop;
    const int iconSize = bodyH < 110 ? bodyH : 110;
    const int iconCx = x + pad + iconSize / 2;
    skyIcon(screen, wx::skyOf(c.code), c.day, iconCx, bodyTop + bodyH / 2, iconSize);

    char t[toybox::kIntChars + 1];
    tempText(t, sizeof(t), c.temp, model.fahrenheit);
    if (huge) model.faces.bindTitle(model.faces.ctx, true);
    const int tw = textWidth(screen, t, toybox::kDisplayFont);
    const int ringR = cut.inkHeight / 7 > 4 ? cut.inkHeight / 7 : 4;
    const int ringW = ringR * 2 / 5 > 2 ? ringR * 2 / 5 : 2;
    const int tx = x + pad + iconSize + 18;
    const int stackH = tall ? cut.inkHeight + 12 + toybox::kTileCut.inkHeight : cut.inkHeight;
    const int ty = bodyTop + (bodyH - stackH) / 2;
    label(screen, rect(tx, ty, tw + 4, cut.inkHeight), t, toybox::kDisplayFont, cut, fui::TextAlign::Left);
    if (huge) model.faces.bindTitle(model.faces.ctx, false);
    degree(screen, tx + tw + 6 + ringR, ty + ringR, ringR, ringW);

    const int colW = tall ? 118 : 92;
    const int colX = x + inner - pad - colW;
    if (tall) {
      caption(screen, tx, ty + cut.inkHeight + 12, colX - tx - 8, wx::labelOf(c.code));
    } else {
      const int lx = tx + tw + 6 + ringR * 2 + 14;
      caption(screen, lx, ty + (cut.inkHeight - toybox::kTileCut.inkHeight) / 2, colX - 18 - lx, wx::labelOf(c.code));
    }

    // The day's high, low and chance of rain, down the right edge.
    if (c.hasDay) {
      const char* names[3] = {"HIGH", "LOW", "RAIN"};
      char values[3][toybox::kIntChars + 2];
      tempText(values[0], sizeof(values[0]), c.max, model.fahrenheit);
      tempText(values[1], sizeof(values[1]), c.min, model.fahrenheit);
      std::snprintf(values[2], sizeof(values[2]), "%d%%", static_cast<int>(c.precip) % 1000);
      const int rowH = bodyH / 3;
      for (int k = 0; k < 3; ++k) {
        const int ry = bodyTop + rowH * k + (rowH - toybox::kUiCut.inkHeight) / 2;
        caption(screen, colX, ry + (toybox::kUiCut.inkHeight - toybox::kTileCut.inkHeight) / 2, 50, names[k]);
        if (tall) {
          label(screen, rect(colX + 50, ry, colW - 50, toybox::kUiCut.inkHeight), values[k], toybox::kUiFont,
                toybox::kUiCut, fui::TextAlign::Right);
        } else {
          caption(screen, colX + 50, ry + (toybox::kUiCut.inkHeight - toybox::kTileCut.inkHeight) / 2, colW - 50,
                  values[k], fui::TextAlign::Right);
        }
      }
      box(screen, colX - 10, bodyTop + 4, toybox::kHairline, bodyH - 8, kInk);
    }
  }
  hairline(screen, toybox::kMargin, footTop, inner);
  footer(screen, nullptr, footTop, footH, width, model.autoMinutes, "SWIPE: 1 / 3 / 5 CITIES");
}

}  // namespace weatherui
