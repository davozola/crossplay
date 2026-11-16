#include "WeatherScreens.h"

#include <cstdio>
#include <cstring>

#include "../ui/ToyboxFormat.h"

namespace weatherui {

namespace {

namespace wx = weather;

constexpr int16_t kBottomReserve = 40;  // the button hints

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void box(toybox::Screen& screen, const int x, const int y, const int w, const int h, const fui::Paint& paint) {
  if (w <= 0 || h <= 0) return;
  screen.target().fill(rect(x, y, w, h), paint);
}

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

void degree(toybox::Screen& screen, const int x, const int y, const int r, const int w) {
  toybox::ring(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r),
               static_cast<int16_t>(w), fui::Color::Black, fui::Color::White);
}

// --- sky glyphs ---------------------------------------------------------------

void sun(toybox::Screen& screen, const int cx, const int cy, const int s) {
  const int r = s * 20 / 100;
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), fui::Color::Black);
  const int t = s / 16 > 2 ? s / 16 : 2;
  const int in = r + s / 14;
  const int len = s * 11 / 100;
  box(screen, cx - t / 2, cy - in - len, t, len, kInk);
  box(screen, cx - t / 2, cy + in, t, len, kInk);
  box(screen, cx - in - len, cy - t / 2, len, t, kInk);
  box(screen, cx + in, cy - t / 2, len, t, kInk);
  // Diagonals as runs of squares stepping along 45 degrees.
  const int d0 = (in * 70) / 100;
  const int d1 = ((in + len) * 70) / 100;
  for (int k = d0; k <= d1; ++k) {
    box(screen, cx + k - t / 2, cy + k - t / 2, t, t, kInk);
    box(screen, cx - k - t / 2, cy + k - t / 2, t, t, kInk);
    box(screen, cx + k - t / 2, cy - k - t / 2, t, t, kInk);
    box(screen, cx - k - t / 2, cy - k - t / 2, t, t, kInk);
  }
}

void moon(toybox::Screen& screen, const int cx, const int cy, const int s) {
  const int r = s * 26 / 100;
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(cx + r * 55 / 100), static_cast<int16_t>(cy - r * 35 / 100),
               static_cast<int16_t>(r * 85 / 100), fui::Color::White);
}

// A cloud whose lower edge is flat at `base`, `w` wide. Drawn as a black
// silhouette, then (when outlined) the same shape inset in paper, so it reads
// as a line drawing that can stand in front of the sun.
void cloudShape(toybox::Screen& screen, const int cx, const int base, const int w, const int inset,
                const fui::Color colour) {
  const int r1 = w * 21 / 100 - inset;
  const int r2 = w * 29 / 100 - inset;
  const int r3 = w * 19 / 100 - inset;
  const int left = cx - w / 2 + inset;
  const int right = cx + w / 2 - inset;
  const int16_t baseY = static_cast<int16_t>(base - inset);
  auto d = [&](const int x, const int y, const int r) {
    if (r > 0) toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r), colour);
  };
  d(left + r1 + inset / 2, baseY - r1, r1);
  d(cx - w / 20, baseY - w * 30 / 100, r2);
  d(right - r3 - inset / 2, baseY - r3, r3);
  const int bodyTop = baseY - w * 19 / 100;
  box(screen, left + r1, bodyTop, (right - r3) - (left + r1), baseY - bodyTop, fui::Paint::solid(colour));
}

void cloud(toybox::Screen& screen, const int cx, const int base, const int w, const bool solid) {
  cloudShape(screen, cx, base, w, 0, fui::Color::Black);
  const int line = w / 16 > 2 ? w / 16 : 2;
  if (!solid) cloudShape(screen, cx, base, w, line, fui::Color::White);
}

void drops(toybox::Screen& screen, const int cx, const int top, const int w, const int s, const int count,
           const int length) {
  const int t = s / 18 > 2 ? s / 18 : 2;
  const int step = w / count;
  for (int i = 0; i < count; ++i) {
    const int x0 = cx - w / 2 + step / 2 + i * step;
    for (int k = 0; k < length; k += t) box(screen, x0 - k / 2, top + k, t, t, kInk);
  }
}

void flakes(toybox::Screen& screen, const int cx, const int top, const int w, const int s) {
  const int arm = s / 14 > 3 ? s / 14 : 3;
  const int t = s / 28 > 1 ? s / 28 : 1;
  const int step = w / 3;
  for (int i = 0; i < 3; ++i) {
    const int x = cx - w / 2 + step / 2 + i * step;
    const int y = top + arm + (i % 2) * arm;
    box(screen, x - arm, y - t / 2, arm * 2 + 1, t + 1, kInk);
    box(screen, x - t / 2, y - arm, t + 1, arm * 2 + 1, kInk);
    for (int k = -arm * 7 / 10; k <= arm * 7 / 10; ++k) {
      box(screen, x + k, y + k, t + 1, t + 1, kInk);
      box(screen, x + k, y - k, t + 1, t + 1, kInk);
    }
  }
}

void bolt(toybox::Screen& screen, const int cx, const int top, const int s) {
  // A zig: down-left, a step right, down-left again.
  const int h = s * 34 / 100;
  const int t = s / 10 > 3 ? s / 10 : 3;
  for (int y = 0; y < h; ++y) {
    const int half = h / 2;
    const int x = y < half ? cx + s / 10 - y * 3 / 5 : cx + s / 10 - (y - half) * 3 / 5;
    box(screen, x - t / 2, top + y, t, 1, kInk);
  }
  box(screen, cx + s / 10 - (h / 2) * 3 / 5 - t / 2, top + h / 2 - t / 3, t + s / 10, t * 2 / 3 + 1, kInk);
}

}  // namespace

void skyIcon(toybox::Screen& screen, const wx::Sky sky, const bool day, const int cx, const int cy, const int size) {
  const int s = size;
  switch (sky) {
    case wx::Sky::Clear:
      if (day) {
        sun(screen, cx, cy, s);
      } else {
        moon(screen, cx, cy, s);
      }
      return;
    case wx::Sky::PartCloud:
      if (day) {
        sun(screen, cx + s * 14 / 100, cy - s * 14 / 100, s * 75 / 100);
      } else {
        moon(screen, cx + s * 16 / 100, cy - s * 16 / 100, s * 75 / 100);
      }
      cloud(screen, cx - s * 6 / 100, cy + s * 30 / 100, s * 74 / 100, false);
      return;
    case wx::Sky::Cloud:
      cloud(screen, cx, cy + s * 22 / 100, s * 86 / 100, true);
      return;
    case wx::Sky::Fog: {
      cloud(screen, cx, cy + s * 4 / 100, s * 74 / 100, false);
      const int t = s / 16 > 2 ? s / 16 : 2;
      box(screen, cx - s * 36 / 100, cy + s * 14 / 100, s * 72 / 100, t, kInk);
      box(screen, cx - s * 28 / 100, cy + s * 26 / 100, s * 64 / 100, t, kInk);
      box(screen, cx - s * 36 / 100, cy + s * 38 / 100, s * 60 / 100, t, kInk);
      return;
    }
    case wx::Sky::Drizzle:
    case wx::Sky::Rain:
    case wx::Sky::Showers: {
      const bool showers = sky == wx::Sky::Showers;
      if (showers) {
        if (day) {
          sun(screen, cx + s * 18 / 100, cy - s * 22 / 100, s * 60 / 100);
        } else {
          moon(screen, cx + s * 18 / 100, cy - s * 22 / 100, s * 60 / 100);
        }
      }
      cloud(screen, cx - (showers ? s * 6 / 100 : 0), cy + s * 8 / 100, s * 80 / 100, showers);
      const int len = sky == wx::Sky::Drizzle ? s / 12 : s * 26 / 100;
      drops(screen, cx, cy + s * 16 / 100, s * 60 / 100, s, sky == wx::Sky::Drizzle ? 4 : 3, len);
      if (sky == wx::Sky::Drizzle) drops(screen, cx + s / 12, cy + s * 32 / 100, s * 60 / 100, s, 4, s / 12);
      return;
    }
    case wx::Sky::Snow:
      cloud(screen, cx, cy + s * 4 / 100, s * 80 / 100, false);
      flakes(screen, cx, cy + s * 12 / 100, s * 70 / 100, s);
      return;
    case wx::Sky::Thunder:
      cloud(screen, cx, cy + s * 4 / 100, s * 80 / 100, true);
      bolt(screen, cx, cy + s * 10 / 100, s);
      return;
  }
}

namespace {

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

void temp(char* out, const size_t cap, const float c, const bool f) {
  std::snprintf(out, cap, "%d", wx::displayTemp(c, f));
}

// Seven-segment numerals with a leading minus, centred in `area`; the same
// digits as the clocks. Returns the right edge of the ink.
constexpr uint8_t kSegments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

int segmentsWidth(const char* text, const int h) {
  const int w = h * 55 / 100;
  const int t = h / 9 > 4 ? h / 9 : 4;
  int total = 0;
  for (const char* c = text; *c; ++c) total += w + t * 2;
  return total - t * 2;
}

void segments(toybox::Screen& screen, const int x0, const int y, const char* text, const int h) {
  const int w = h * 55 / 100;
  const int t = h / 9 > 4 ? h / 9 : 4;
  const int mid = h / 2;
  const int arm = mid - t - t / 2;
  int x = x0;
  for (const char* c = text; *c; ++c) {
    if (*c == '-') {
      box(screen, x + t, y + mid - t / 2, w - 2 * t, t, kInk);
    } else if (*c >= '0' && *c <= '9') {
      const uint8_t s = kSegments[*c - '0'];
      if (s & 0x01) box(screen, x + t, y, w - 2 * t, t, kInk);
      if (s & 0x02) box(screen, x + w - t, y + t, t, arm, kInk);
      if (s & 0x04) box(screen, x + w - t, y + mid + t / 2, t, arm, kInk);
      if (s & 0x08) box(screen, x + t, y + h - t, w - 2 * t, t, kInk);
      if (s & 0x10) box(screen, x, y + mid + t / 2, t, arm, kInk);
      if (s & 0x20) box(screen, x, y + t, t, arm, kInk);
      if (s & 0x40) box(screen, x + t, y + mid - t / 2, w - 2 * t, t, kInk);
    }
    x += w + t * 2;
  }
}

// --- forecast pieces -----------------------------------------------------------

void currentBlock(toybox::Screen& screen, const wx::Forecast& f, const bool fahrenheit, const int top, const int height,
                  const int width) {
  const int iconSize = height - 10;
  skyIcon(screen, wx::skyOf(f.current.code), f.current.isDay, toybox::kMargin + iconSize / 2, top + height / 2,
          iconSize);

  const int left = toybox::kMargin + iconSize + toybox::kGutter;
  const int right = width - toybox::kMargin;
  char t[toybox::kIntChars];
  temp(t, sizeof(t), f.current.temp, fahrenheit);
  const int digitH = height * 55 / 100;
  const int tw = segmentsWidth(t, digitH);
  const int unitW = digitH * 40 / 100;
  const int x = left + ((right - left) - tw - unitW) / 2;
  segments(screen, x, top + 4, t, digitH);
  const int dr = digitH / 9 + 3;
  degree(screen, x + tw + dr + 6, top + 4 + dr, dr, digitH / 16 + 2);
  label(screen, rect(x + tw + 4, top + 4 + dr * 2 + 4, unitW, toybox::kUiCut.lineHeight), fahrenheit ? "F" : "C",
        toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Left);
  label(screen, rect(left, top + digitH + 14, right - left, toybox::kUiCut.lineHeight), wx::labelOf(f.current.code),
        toybox::kUiFont, toybox::kUiCut);
}

void detailLine(toybox::Screen& screen, const wx::Forecast& f, const bool fahrenheit, const int top, const int width) {
  char line[96];
  const wx::Day& today = f.days[0];
  std::snprintf(line, sizeof(line), "FEELS %d   HUM %d%%   WIND %d %s   SUN %s-%s",
                wx::displayTemp(f.current.feels, fahrenheit), f.current.humidity,
                wx::displayWind(f.current.wind, fahrenheit), fahrenheit ? "MPH" : "KM/H", today.sunrise, today.sunset);
  label(screen, rect(toybox::kMargin, top, width - toybox::kMargin * 2, toybox::kTileCut.lineHeight), line,
        toybox::kTileFont, toybox::kTileCut);
}

void rule(toybox::Screen& screen, const int y, const int width) {
  box(screen, toybox::kMargin, y, width - toybox::kMargin * 2, toybox::kHairline, kInk);
}

// Eight columns, three hours apart, starting at the current hour.
void hourStrip(toybox::Screen& screen, const wx::Forecast& f, const bool fahrenheit, const int top, const int height,
               const int width) {
  const int cols = 8;
  const int colW = (width - toybox::kMargin * 2) / cols;
  const int icon = height * 36 / 100;
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
    label(screen, rect(x, top, colW, toybox::kTileCut.lineHeight), hh, toybox::kTileFont, toybox::kTileCut);
    const bool day = h.hour >= 7 && h.hour < 19;
    skyIcon(screen, wx::skyOf(h.code), day, x + colW / 2, top + toybox::kTileCut.lineHeight + icon / 2 + 2, icon);
    char t[toybox::kIntChars];
    temp(t, sizeof(t), h.temp, fahrenheit);
    const int ty = top + toybox::kTileCut.lineHeight + icon + 6;
    label(screen, rect(x, ty, colW, toybox::kButtonCut.lineHeight), t, toybox::kUiFont, toybox::kUiCut);
    if (h.precip >= 20) {
      char p[toybox::kIntChars + 2];
      std::snprintf(p, sizeof(p), "%d%%", h.precip);
      label(screen, rect(x, ty + toybox::kUiCut.lineHeight - 14, colW, toybox::kTileCut.lineHeight), p,
            toybox::kTileFont, toybox::kTileCut);
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

// A row per day: name, sky, rain chance, then the day's low and high with its
// span drawn against the whole week's, so a cold snap shows as a bar that
// slides left.
void dayRows(toybox::Screen& screen, const wx::Forecast& f, const bool fahrenheit, const int top, const int rowH,
             const int width) {
  float lo = f.days[0].min;
  float hi = f.days[0].max;
  for (int i = 1; i < f.dayCount; ++i) {
    if (f.days[i].min < lo) lo = f.days[i].min;
    if (f.days[i].max > hi) hi = f.days[i].max;
  }
  const float span = hi - lo > 1 ? hi - lo : 1;
  const int x0 = toybox::kMargin;
  const int inner = width - toybox::kMargin * 2;
  for (int i = 0; i < f.dayCount; ++i) {
    const wx::Day& d = f.days[i];
    const int y = top + i * rowH;
    if (i > 0) box(screen, x0, y, inner, toybox::kHairline, kInk);
    char name[16];
    dayName(name, sizeof(name), d, i);
    label(screen, rect(x0, y, 104, rowH), name, toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left);
    const int icon = rowH - 8;
    skyIcon(screen, wx::skyOf(d.code), true, x0 + 104 + icon / 2, y + rowH / 2, icon);
    char p[toybox::kIntChars + 2] = "";
    if (d.precip >= 20) std::snprintf(p, sizeof(p), "%d%%", d.precip);
    label(screen, rect(x0 + 104 + icon + 4, y, 52, rowH), p, toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left);
    char low[toybox::kIntChars];
    char high[toybox::kIntChars];
    temp(low, sizeof(low), d.min, fahrenheit);
    temp(high, sizeof(high), d.max, fahrenheit);
    const int numW = 40;
    const int midL = x0 + 104 + icon + 60;
    const int midR = x0 + inner;
    label(screen, rect(midL, y, numW, rowH), low, toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Right);
    label(screen, rect(midR - numW, y, numW, rowH), high, toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Left);
    const int trackL = midL + numW + 10;
    const int trackR = midR - numW - 10;
    const int trackW = trackR - trackL;
    const int cy = y + rowH / 2;
    box(screen, trackL, cy - 1, trackW, 2, fui::Paint::dither(fui::Color::DarkGray));
    const int a = trackL + static_cast<int>((d.min - lo) / span * trackW);
    const int b = trackL + static_cast<int>((d.max - lo) / span * trackW);
    screen.target().fill(rect(a, cy - 5, b - a > 10 ? b - a : 10, 10), kInk, 5);
  }
}

void footer(toybox::Screen& screen, const wx::Forecast* f, const int top, const int width) {
  char when[40] = "NOT FETCHED YET";
  if (f && f->current.time[0]) {
    const wx::Current& c = f->current;
    int y = 0;
    int m = 0;
    int d = 0;
    std::sscanf(c.time, "%d-%d-%d", &y, &m, &d);
    std::snprintf(when, sizeof(when), "UPDATED %s %d %s %.5s", wx::weekdayName(wx::weekday(y, m, d)), d,
                  wx::monthName(m), c.time + 11);
  }
  const int bw = 150;
  label(screen, rect(toybox::kMargin, top, width - toybox::kMargin * 2 - bw - toybox::kGutter, toybox::kPillHeight),
        when, toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left);
  fui::ButtonProps b;
  b.label = "REFRESH";
  b.action = ActionRefresh;
  b.styles = toybox::rowStyles();
  screen.button(b, rect(width - toybox::kMargin - bw, top, bw, toybox::kPillHeight));
}

}  // namespace

void buildForecast(toybox::Screen& screen, const ForecastModel& model) {
  chrome(screen, model.place ? model.place->name : "WEATHER", "CITIES", ActionCities);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int bottom = device.height - kBottomReserve - toybox::kMargin;
  const int footTop = bottom - toybox::kPillHeight;

  if (!model.forecast) {
    label(screen,
          rect(toybox::kMargin, toybox::kBodyTop + 120, width - toybox::kMargin * 2, toybox::kDisplayCut.lineHeight),
          "NO FORECAST YET", toybox::kDisplayFont, toybox::kDisplayCut);
    fui::TextStyle s;
    s.font = toybox::kBodyFont;
    s.align = fui::TextAlign::Center;
    s.maxLines = 3;
    screen.target().text(rect(toybox::kMargin * 2, toybox::kBodyTop + 200, width - toybox::kMargin * 4, 130),
                         "TAP REFRESH TO JOIN WI-FI AND FETCH IT. IT STAYS ON THE CARD FOR WHEN YOU ARE OFFLINE.", s);
    footer(screen, nullptr, footTop, width);
    return;
  }

  const wx::Forecast& f = *model.forecast;
  int y = toybox::kBodyTop - 18;
  const int curH = 150;
  currentBlock(screen, f, model.fahrenheit, y, curH, width);
  y += curH + 4;
  detailLine(screen, f, model.fahrenheit, y, width);
  y += toybox::kTileCut.lineHeight + 6;
  rule(screen, y, width);
  y += 8;
  const int hourH = 112;
  hourStrip(screen, f, model.fahrenheit, y, hourH, width);
  y += hourH;
  rule(screen, y, width);
  y += 6;
  const int rowH = (footTop - 8 - y) / (f.dayCount > 0 ? f.dayCount : 1);
  dayRows(screen, f, model.fahrenheit, y, rowH, width);
  footer(screen, &f, footTop, width);
}

void buildCities(toybox::Screen& screen, const CitiesModel& model) {
  chrome(screen, "CITIES", nullptr, 0);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  int y = toybox::kBodyTop;
  const int rowH = 64;
  const int xW = 64;
  for (int i = 0; i < model.count; ++i) {
    const wx::Place& p = model.places[i];
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionSelect;
    row.value = static_cast<int16_t>(i);
    row.styles = toybox::rowStyles();
    row.state = i == model.selected ? fui::StateSelected : fui::StateNormal;
    const fui::Rect r = rect(toybox::kMargin, y, inner - xW - toybox::kGutter, rowH);
    screen.button(row, r);
    const fui::Color ink = i == model.selected ? fui::Color::White : fui::Color::Black;
    label(screen, rect(r.x + 14, y + 2, r.width - 28, toybox::kUiCut.lineHeight - 6), p.name, toybox::kUiFont,
          toybox::kUiCut, fui::TextAlign::Left, ink);
    label(screen, rect(r.x + 14, y + rowH - toybox::kTileCut.lineHeight - 4, r.width - 28, toybox::kTileCut.lineHeight),
          p.region, toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left, ink);
    fui::ButtonProps x;
    x.label = "X";
    x.action = ActionRemove;
    x.value = static_cast<int16_t>(i);
    x.styles = toybox::rowStyles();
    screen.button(x, rect(width - toybox::kMargin - xW, y, xW, rowH));
    y += rowH + toybox::kGutter / 2;
  }
  y += toybox::kGutter;
  if (model.count < wx::kMaxPlaces) {
    fui::ButtonProps add;
    add.label = "+ ADD A CITY";
    add.action = ActionAdd;
    add.styles = toybox::rowStyles();
    screen.button(add, rect(toybox::kMargin, y, inner, toybox::kPillHeight));
    y += toybox::kPillHeight + toybox::kGutter;
  } else {
    label(screen, rect(toybox::kMargin, y, inner, toybox::kPillHeight), "SIX CITIES: REMOVE ONE TO ADD ANOTHER",
          toybox::kTileFont, toybox::kTileCut);
    y += toybox::kPillHeight + toybox::kGutter;
  }
  fui::ButtonProps units;
  units.label = model.fahrenheit ? "UNITS: FAHRENHEIT, MPH" : "UNITS: CELSIUS, KM/H";
  units.action = ActionUnits;
  units.styles = toybox::rowStyles();
  screen.button(units, rect(toybox::kMargin, y, inner, toybox::kPillHeight));
}

void buildResults(toybox::Screen& screen, const ResultsModel& model) {
  chrome(screen, "PICK A CITY", "SEARCH", ActionSearch);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  int y = toybox::kBodyTop;
  if (model.count == 0) {
    char line[64];
    std::snprintf(line, sizeof(line), "NOTHING FOUND FOR \"%.30s\"", model.query);
    label(screen, rect(toybox::kMargin, y + 80, inner, toybox::kUiCut.lineHeight), line, toybox::kUiFont,
          toybox::kUiCut);
    label(screen, rect(toybox::kMargin, y + 130, inner, toybox::kTileCut.lineHeight),
          "TRY THE CITY ALONE, WITHOUT THE COUNTRY", toybox::kTileFont, toybox::kTileCut);
    return;
  }
  const int rowH = 72;
  for (int i = 0; i < model.count; ++i) {
    const wx::Place& p = model.results[i];
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionPick;
    row.value = static_cast<int16_t>(i);
    row.styles = toybox::rowStyles();
    const fui::Rect r = rect(toybox::kMargin, y, inner, rowH);
    screen.button(row, r);
    label(screen, rect(r.x + 14, y + 4, r.width - 28, toybox::kUiCut.lineHeight - 6), p.name, toybox::kUiFont,
          toybox::kUiCut, fui::TextAlign::Left);
    char sub[wx::kRegionChars + 24];
    std::snprintf(sub, sizeof(sub), "%s  %.1f, %.1f", p.region, static_cast<double>(p.lat), static_cast<double>(p.lon));
    label(screen, rect(r.x + 14, y + rowH - toybox::kTileCut.lineHeight - 6, r.width - 28, toybox::kTileCut.lineHeight),
          sub, toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left);
    y += rowH + toybox::kGutter / 2;
  }
}

void buildStatus(toybox::Screen& screen, const StatusModel& model) {
  chrome(screen, model.title, nullptr, 0);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  label(screen,
        rect(toybox::kMargin, device.height / 2 - 100, width - toybox::kMargin * 2, toybox::kDisplayCut.lineHeight),
        model.headline, toybox::kDisplayFont, toybox::kDisplayCut);
  fui::TextStyle s;
  s.font = toybox::kBodyFont;
  s.align = fui::TextAlign::Center;
  s.maxLines = 3;
  screen.target().text(rect(toybox::kMargin * 2, device.height / 2 - 20, width - toybox::kMargin * 4, 120),
                       model.detail, s);
  if (model.dismissable) {
    fui::ButtonProps ok;
    ok.label = "OK";
    ok.action = ActionDismiss;
    ok.styles = toybox::rowStyles();
    screen.button(ok, rect(width / 2 - 90, device.height / 2 + 120, 180, toybox::kPillHeight));
  }
}

}  // namespace weatherui
