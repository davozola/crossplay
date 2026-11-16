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

bool daylight(const int hour, const wx::Day& today) {
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

  int y = inkTop + cut.inkHeight + 18;
  label(screen, rect(left, y, colW, toybox::kUiCut.inkHeight), wx::labelOf(f.current.code), toybox::kUiFont,
        toybox::kUiCut);
  y += toybox::kUiCut.inkHeight + 14;
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
  const int capY = top + 12;
  const int valY = capY + toybox::kTileCut.inkHeight + 12;
  for (int i = 0; i < 4; ++i) {
    const int x = toybox::kMargin + i * cellW;
    if (i > 0) box(screen, x, top + 10, toybox::kHairline, height - 20, kInk);
    caption(screen, x, capY, cellW, captions[i], fui::TextAlign::Center);
    if (i == 0) {
      smallTemp(screen, x + cellW / 2, valY + toybox::kUiCut.inkHeight / 2, f.current.feels, fahr);
    } else {
      label(screen, rect(x, valY, cellW, toybox::kUiCut.inkHeight), values[i], toybox::kUiFont, toybox::kUiCut);
    }
  }
}

// Eight columns three hours apart from now: hour, sky, temperature, and the
// chance of rain on a row of its own where it is worth saying.
void hours(toybox::Screen& screen, const wx::Forecast& f, const bool fahr, const int top, const int width) {
  const int cols = 8;
  const int inner = width - toybox::kMargin * 2;
  const int colW = inner / cols;
  const int icon = 40;
  const int hourY = top;
  const int iconCy = hourY + toybox::kTileCut.inkHeight + 10 + icon / 2;
  const int tempCy = iconCy + icon / 2 + 8 + toybox::kUiCut.inkHeight / 2;
  const int rainY = tempCy + toybox::kUiCut.inkHeight / 2 + 10;
  for (int c = 0; c < cols; ++c) {
    const int i = c * 3;
    if (i >= f.hourCount) break;
    const wx::Hour& h = f.hours[i];
    const int x = toybox::kMargin + c * colW;
    char hh[toybox::kIntChars + 1];
    if (c == 0) {
      std::snprintf(hh, sizeof(hh), "NOW");
    } else {
      std::snprintf(hh, sizeof(hh), "%02d", h.hour);
    }
    caption(screen, x, hourY, colW, hh, fui::TextAlign::Center);
    skyIcon(screen, wx::skyOf(h.code), daylight(h.hour, f.days[0]), x + colW / 2, iconCy, icon);
    smallTemp(screen, x + colW / 2, tempCy, h.temp, fahr);
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

// A row per day: name, sky, chance of rain, then the low and high with the
// day's span drawn against the whole week's. Today's row marks where the
// temperature is now.
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
  const int nameW = 92;
  const int icon = 32;
  const int iconCx = x0 + nameW + icon / 2;
  const int rainX = x0 + nameW + icon + 10;
  const int rainW = 48;
  const int numW = 42;
  const int loX = rainX + rainW;
  const int hiX = x0 + inner - numW;
  const int trackL = loX + numW + 12;
  const int trackR = hiX - 12;
  const int trackW = trackR - trackL;
  for (int i = 0; i < f.dayCount; ++i) {
    const wx::Day& d = f.days[i];
    const int y = top + i * rowH;
    const int cy = y + rowH / 2;
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
  }
}

void footer(toybox::Screen& screen, const wx::Forecast* f, const int top, const int height, const int width) {
  char when[48] = "NOT FETCHED YET";
  if (f && f->current.time[0]) {
    const wx::Current& c = f->current;
    int y = 0;
    int m = 0;
    int d = 0;
    std::sscanf(c.time, "%d-%d-%d", &y, &m, &d);
    std::snprintf(when, sizeof(when), "UPDATED %s %d %s, %.5s", wx::weekdayName(wx::weekday(y, m, d)), d,
                  wx::monthName(m), c.time + 11);
  }
  const int bw = 132;
  const int bh = 44;
  const int by = top + (height - bh) / 2;
  caption(screen, toybox::kMargin, top + (height - toybox::kTileCut.inkHeight) / 2,
          width - toybox::kMargin * 2 - bw - toybox::kGutter, when);
  fui::ButtonProps b;
  b.label = "REFRESH";
  b.action = ActionRefresh;
  b.styles = toybox::rowStyles();
  b.text = toybox::buttonText(screen.theme());
  b.radius = toybox::kPillRadius / 2;
  screen.button(b, rect(width - toybox::kMargin - bw, by, bw, bh));
}

}  // namespace

void buildForecast(toybox::Screen& screen, const ForecastModel& model) {
  chrome(screen, model.place ? model.place->name : "WEATHER", "CITIES", ActionCities);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int footH = 52;
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
    footer(screen, nullptr, footTop, footH, width);
    return;
  }

  const wx::Forecast& f = *model.forecast;
  int y = toybox::kChromeHeight + 14;
  const int heroH = 160;
  hero(screen, model, y, heroH, width);
  y += heroH + 10;
  hairline(screen, toybox::kMargin, y, inner);
  const int statsH = 66;
  stats(screen, f, model.fahrenheit, y, statsH, width);
  y += statsH;
  hairline(screen, toybox::kMargin, y, inner);
  y += 14;
  const int hoursH = 132;
  hours(screen, f, model.fahrenheit, y, width);
  y += hoursH;
  hairline(screen, toybox::kMargin, y, inner);
  const int rowH = (footTop - y) / (f.dayCount > 0 ? f.dayCount : 1);
  days(screen, f, model.fahrenheit, y, rowH, width);
  hairline(screen, toybox::kMargin, footTop, inner);
  footer(screen, &f, footTop, footH, width);
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
    const fui::Rect r = rect(toybox::kMargin, y, inner - xW - gap, rowH);
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

}  // namespace weatherui
