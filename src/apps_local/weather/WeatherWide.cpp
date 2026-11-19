#include "WeatherWide.h"

#include <GfxRenderer.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "WeatherType.h"

namespace weatherwide {

namespace {

namespace wx = weather;
using weathertype::Font;
using weathertype::Glyph;

constexpr float kPi = 3.14159265f;

// Where a face draws: the panel, and which of the two inks is the figure.
struct Page {
  GfxRenderer& r;
  int w;
  int h;
  bool ink;  // true: black on paper
};

const Glyph* glyphOf(const Font& font, const uint16_t codepoint) {
  int lo = 0;
  int hi = font.count - 1;
  while (lo <= hi) {
    const int mid = (lo + hi) / 2;
    if (font.glyphs[mid].codepoint == codepoint) return &font.glyphs[mid];
    if (font.glyphs[mid].codepoint < codepoint) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return nullptr;
}

// The next character of a UTF-8 string; one and two byte forms are all the
// pages use.
uint16_t nextChar(const char*& s) {
  const uint8_t a = static_cast<uint8_t>(*s++);
  if (a < 0x80 || *s == '\0') return a;
  const uint8_t b = static_cast<uint8_t>(*s++);
  return static_cast<uint16_t>(((a & 0x1F) << 6) | (b & 0x3F));
}

int widthOf(const Font& font, const char* s) {
  int width = 0;
  while (*s != '\0') {
    const Glyph* g = glyphOf(font, nextChar(s));
    if (g != nullptr) width += g->advance;
  }
  return width;
}

// A string from its pen position on the baseline. align: -1 from x, 0 centred
// on x, 1 ending at x. Returns where the pen ended.
int type(const Page& p, const Font& font, int x, const int baseline, const char* s, const int align = -1,
         const bool figure = true) {
  if (align >= 0) {
    const int width = widthOf(font, s);
    x = align == 0 ? x - width / 2 : x - width;
  }
  const bool state = figure ? p.ink : !p.ink;
  while (*s != '\0') {
    const Glyph* g = glyphOf(font, nextChar(s));
    if (g == nullptr) continue;
    const uint8_t* runs = font.runs + g->offset;
    const int left = x + g->left;
    const int top = baseline - g->top;
    for (int row = 0; row < g->rows; ++row) {
      const int count = *runs++;
      for (int i = 0; i < count; ++i) {
        p.r.fillRect(left + runs[0], top + row, runs[1], 1, state);
        runs += 2;
      }
    }
    x += g->advance;
  }
  return x;
}

void bar(const Page& p, const int x, const int y, const int w, const int h, const bool figure = true) {
  if (w > 0 && h > 0) p.r.fillRect(x, y, w, h, figure ? p.ink : !p.ink);
}

void disc(const Page& p, const int cx, const int cy, const int radius, const bool figure = true) {
  for (int dy = -radius; dy <= radius; ++dy) {
    const int half = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
    bar(p, cx - half, cy + dy, half * 2 + 1, 1, figure);
  }
}

void ring(const Page& p, const int cx, const int cy, const int radius, const int weight) {
  disc(p, cx, cy, radius, true);
  disc(p, cx, cy, radius - weight, false);
}

// A straight stroke `weight` thick, square ends.
void stroke(const Page& p, const float x0, const float y0, const float x1, const float y1, const float weight,
            const bool figure = true) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0) return;
  const float nx = -dy / len * weight / 2;
  const float ny = dx / len * weight / 2;
  const int xs[4] = {static_cast<int>(std::lround(x0 + nx)), static_cast<int>(std::lround(x1 + nx)),
                     static_cast<int>(std::lround(x1 - nx)), static_cast<int>(std::lround(x0 - nx))};
  const int ys[4] = {static_cast<int>(std::lround(y0 + ny)), static_cast<int>(std::lround(y1 + ny)),
                     static_cast<int>(std::lround(y1 - ny)), static_cast<int>(std::lround(y0 - ny))};
  p.r.fillPolygon(xs, ys, 4, figure ? p.ink : !p.ink);
}

int minutesOf(const char* hhmm) {
  if (std::strlen(hhmm) < 5) return -1;
  return ((hhmm[0] - '0') * 10 + (hhmm[1] - '0')) * 60 + (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
}

// Sixteen points of the compass for a bearing in degrees, or "" for none.
const char* compassPoint(const int degrees) {
  static const char* const kPoints[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                          "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  if (degrees < 0) return "";
  return kPoints[((degrees % 360) * 2 + 22) / 45 % 16];
}

void updatedLine(const weatherui::ForecastModel& m, char* out, const size_t cap) {
  const wx::Forecast& f = *m.forecast;
  char autoPart[24] = "";
  if (m.autoMinutes >= 60) {
    std::snprintf(autoPart, sizeof(autoPart), "  ·  AUTO %d H", m.autoMinutes / 60);
  } else if (m.autoMinutes > 0) {
    std::snprintf(autoPart, sizeof(autoPart), "  ·  AUTO %d M", m.autoMinutes);
  }
  std::snprintf(out, cap, "UPDATED %.5s%s", f.current.time[0] ? f.current.time + 11 : "--:--", autoPart);
}

// --- editorial -------------------------------------------------------------------

// "PARTLY CLOUDY" as a sentence would have it: "Partly cloudy".
void sentenceCase(const char* upper, char* out, const size_t cap) {
  size_t i = 0;
  for (; upper[i] != '\0' && i + 1 < cap; ++i) {
    const char c = upper[i];
    out[i] = (i > 0 && c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }
  out[i] = '\0';
}

const char* partOfDay(const int hour) {
  if (hour >= 6 && hour < 12) return "the morning";
  if (hour >= 12 && hour < 15) return "midday";
  if (hour >= 15 && hour < 19) return "late afternoon";
  if (hour >= 19 && hour < 23) return "the evening";
  return "tonight";
}

bool wet(const int code) {
  const wx::Sky sky = wx::skyOf(code);
  return sky == wx::Sky::Drizzle || sky == wx::Sky::Rain || sky == wx::Sky::Showers || sky == wx::Sky::Thunder;
}

// What the next day holds, in a few words, from the hours already fetched.
void outlook(const wx::Forecast& f, char* out, const size_t cap) {
  const int first = wx::nowIndex(f);
  int last = first + 24;
  if (last > f.hourCount) last = f.hourCount;
  if (first < 0 || last - first < 3) {
    out[0] = '\0';
    return;
  }
  if (wet(f.current.code)) {
    for (int i = first + 1; i < last; ++i) {
      if (f.hours[i].precip < 20) {
        std::snprintf(out, cap, "easing by %s.", partOfDay(f.hours[i].hour));
        return;
      }
    }
    std::snprintf(out, cap, "rain through the day.");
    return;
  }
  int peak = 0;
  for (int i = first + 1; i < last; ++i) {
    if (f.hours[i].precip >= 50) {
      std::snprintf(out, cap, "rain by %s.", partOfDay(f.hours[i].hour));
      return;
    }
    if (f.hours[i].precip > peak) peak = f.hours[i].precip;
  }
  std::snprintf(out, cap, "%s", peak >= 25 ? "showers possible later." : "dry through tomorrow.");
}

void editorial(const Page& p, const weatherui::ForecastModel& m) {
  using namespace weathertype;
  const wx::Forecast& f = *m.forecast;
  const bool fahr = m.fahrenheit;
  const int margin = 28;
  const int right = p.w - margin;

  // The masthead: the place, and the day this is from.
  type(p, kMonoBold, margin, 34, m.place->name);
  char dateline[48] = "";
  if (f.current.time[0]) {
    int y = 0, mo = 0, d = 0;
    std::sscanf(f.current.time, "%d-%d-%d", &y, &mo, &d);
    std::snprintf(dateline, sizeof(dateline), "%s %d %s  ·  %.5s", wx::weekdayLong(wx::weekday(y, mo, d)), d % 100,
                  wx::monthName(mo), f.current.time + 11);
  }
  type(p, kMono, right, 34, dateline, 1);
  bar(p, margin, 48, p.w - margin * 2, 2);

  // The figure, with air under it before the rule.
  const int figureBase = 262;
  char temp[8];
  std::snprintf(temp, sizeof(temp), "%d", wx::displayTemp(f.current.temp, fahr));
  const int figureEnd = type(p, kSerifHuge, margin - 8, figureBase, temp);
  ring(p, figureEnd + 26, figureBase - kSerifHuge.capTop + 22, 20, 7);

  // Beside it: the day in a sentence, then the figures large enough to read
  // from across a desk.
  int textX = figureEnd + 76;
  if (textX < 392) textX = 392;
  const int room = right - textX;
  char now[40];
  char lead[48];
  sentenceCase(wx::labelOf(f.current.code), now, sizeof(now));
  std::snprintf(lead, sizeof(lead), "%s now,", now);
  char next[48];
  outlook(f, next, sizeof(next));
  if (next[0] == '\0') std::snprintf(lead, sizeof(lead), "%s.", now);
  const bool small = widthOf(kSerifPhrase, lead) > room || widthOf(kSerifPhrase, next) > room;
  const Font& phrase = small ? kSerifPhraseSmall : kSerifPhrase;
  type(p, phrase, textX, 104, lead);
  type(p, phrase, textX, small ? 140 : 148, next);

  char line[3][64];
  std::snprintf(line[0], sizeof(line[0]), "FEELS %d°  HIGH %d°  LOW %d°", wx::displayTemp(f.current.feels, fahr),
                wx::displayTemp(f.days[0].max, fahr), wx::displayTemp(f.days[0].min, fahr));
  std::snprintf(line[1], sizeof(line[1]), "WIND %d %s %s", wx::displayWind(f.current.wind, fahr),
                fahr ? "MPH" : "KM/H", compassPoint(f.current.windDir));
  std::snprintf(line[2], sizeof(line[2]), "HUMIDITY %d%%  SUN %s > %s", f.current.humidity, f.days[0].sunrise,
                f.days[0].sunset);
  bool tight = false;
  for (const auto& l : line) tight = tight || widthOf(kMonoBig, l) > room;
  const Font& stat = tight ? kMonoBold : kMonoBig;
  for (int i = 0; i < 3; ++i) type(p, stat, textX, 198 + i * 30, line[i]);

  bar(p, margin, 300, p.w - margin * 2, 1);

  // The week: each day's high and low, and where they fall on one scale.
  const int n = f.dayCount > 7 ? 7 : f.dayCount;
  if (n > 0) {
    float lo = f.days[0].min;
    float hi = f.days[0].max;
    for (int i = 1; i < n; ++i) {
      if (f.days[i].min < lo) lo = f.days[i].min;
      if (f.days[i].max > hi) hi = f.days[i].max;
    }
    const float span = hi - lo > 1.0f ? hi - lo : 1.0f;
    const int colW = (p.w - margin * 2) / n;
    for (int i = 0; i < n; ++i) {
      const wx::Day& d = f.days[i];
      const int x = margin + colW * i;
      if (i > 0) bar(p, x - 7, 312, 1, 108);
      type(p, i == 0 ? kMonoBold : kMono, x + 2, 326,
           i == 0 ? "TODAY" : wx::weekdayName(wx::weekday(d.year, d.month, d.day)));
      char figure[8];
      std::snprintf(figure, sizeof(figure), "%d", wx::displayTemp(d.max, fahr));
      const int after = type(p, kSerifFigure, x + 2, 368, figure);
      std::snprintf(figure, sizeof(figure), "%d", wx::displayTemp(d.min, fahr));
      type(p, kSerifFigureItalic, after + 14, 368, figure);
      const int x0 = x + 2;
      const int x1 = x + colW - 18;
      bar(p, x0, 388, x1 - x0, 1);
      const int a = x0 + static_cast<int>(static_cast<float>(x1 - x0) * (d.min - lo) / span);
      int b = x0 + static_cast<int>(static_cast<float>(x1 - x0) * (d.max - lo) / span);
      if (b - a < 8) b = a + 8;
      bar(p, a, 384, b - a, 9);
      if (d.precip >= 20) {
        char rain[16];
        std::snprintf(rain, sizeof(rain), "RAIN %d%%", d.precip % 1000);
        type(p, kMono, x + 2, 416, rain);
      }
    }
  }

  bar(p, margin, p.h - kFootHeight, p.w - margin * 2, 2);
  char updated[48];
  updatedLine(m, updated, sizeof(updated));
  type(p, kMonoBold, margin, p.h - 18, "CITIES");
  type(p, kMono, p.w / 2, p.h - 18, updated, 0);
  type(p, kMonoBold, right, p.h - 18, "REFRESH", 1);
}

// --- instruments -----------------------------------------------------------------

void gauge(const Page& p, const int x, const int y, const int w, const char* name, const int percent) {
  using namespace weathertype;
  type(p, kMono, x, y + 12, name);
  char value[12];
  std::snprintf(value, sizeof(value), "%d %%", percent % 1000);
  type(p, kMonoBold, x + w, y + 12, value, 1);
  const int cells = w / 10;
  const int lit = cells * (percent < 0 ? 0 : (percent > 100 ? 100 : percent)) / 100;
  for (int i = 0; i < cells; ++i) {
    if (i < lit) {
      bar(p, x + i * 10, y + 22, 6, 16);
    } else {
      bar(p, x + i * 10 + 2, y + 28, 2, 4);
    }
  }
}

void instruments(const Page& p, const weatherui::ForecastModel& m) {
  using namespace weathertype;
  const wx::Forecast& f = *m.forecast;
  const bool fahr = m.fahrenheit;
  const int margin = 24;
  const int right = p.w - margin;

  // The header: where, and when this was read.
  char where[64];
  std::snprintf(where, sizeof(where), "%s  %.2f%c %.2f%c", m.place->name,
                static_cast<double>(std::fabs(m.place->lat)), m.place->lat < 0 ? 'S' : 'N',
                static_cast<double>(std::fabs(m.place->lon)), m.place->lon < 0 ? 'W' : 'E');
  if (widthOf(kMonoBold, where) > 440) std::snprintf(where, sizeof(where), "%s", m.place->name);
  type(p, kMonoBold, margin, 28, where);
  char updated[48];
  updatedLine(m, updated, sizeof(updated));
  type(p, kMono, right, 28, updated, 1);
  bar(p, margin, 40, p.w - margin * 2, 1);

  // Now.
  char temp[8];
  std::snprintf(temp, sizeof(temp), "%d", wx::displayTemp(f.current.temp, fahr));
  const int after = type(p, kMonoHuge, margin - 4, 158, temp);
  type(p, kMonoUnit, after + 8, 158 - kMonoHuge.capTop + kMonoUnit.capTop, fahr ? "°F" : "°C");
  type(p, kMonoBold, margin + 2, 198, wx::labelOf(f.current.code));
  char day[48];
  std::snprintf(day, sizeof(day), "FEELS %d  HI %d  LO %d", wx::displayTemp(f.current.feels, fahr),
                wx::displayTemp(f.days[0].max, fahr), wx::displayTemp(f.days[0].min, fahr));
  type(p, kMono, margin + 2, 222, day);

  // The wind: a card with its needle pointing where it blows from.
  const int cx = 384;
  const int cy = 124;
  const int r = 74;
  ring(p, cx, cy, r, 3);
  for (int k = 0; k < 36; ++k) {
    const float a = static_cast<float>(k) * kPi / 18.0f;
    const float len = k % 9 == 0 ? 12.0f : (k % 3 == 0 ? 7.0f : 4.0f);
    stroke(p, cx + std::cos(a) * (r - len), cy + std::sin(a) * (r - len), cx + std::cos(a) * (r - 1),
           cy + std::sin(a) * (r - 1), 2);
  }
  type(p, kMonoBold, cx, cy - r + 30, "N", 0);
  type(p, kMonoBold, cx, cy + r - 20, "S", 0);
  type(p, kMonoBold, cx + r - 26, cy + 6, "E", 0);
  type(p, kMonoBold, cx - r + 26, cy + 6, "W", 0);
  if (f.current.windDir >= 0) {
    const float a = (static_cast<float>(f.current.windDir) - 90.0f) * kPi / 180.0f;
    const float tx = cx + std::cos(a) * (r - 34);
    const float ty = cy + std::sin(a) * (r - 34);
    stroke(p, cx - std::cos(a) * (r - 46), cy - std::sin(a) * (r - 46), tx, ty, 5);
    const int xs[3] = {static_cast<int>(std::lround(cx + std::cos(a) * (r - 16))),
                       static_cast<int>(std::lround(tx - std::cos(a - kPi / 2) * 10)),
                       static_cast<int>(std::lround(tx - std::cos(a + kPi / 2) * 10))};
    const int ys[3] = {static_cast<int>(std::lround(cy + std::sin(a) * (r - 16))),
                       static_cast<int>(std::lround(ty - std::sin(a - kPi / 2) * 10)),
                       static_cast<int>(std::lround(ty - std::sin(a + kPi / 2) * 10))};
    p.r.fillPolygon(xs, ys, 3, p.ink);
  }
  disc(p, cx, cy, 7);
  char wind[40];
  std::snprintf(wind, sizeof(wind), "WIND %d %s %s", wx::displayWind(f.current.wind, fahr), fahr ? "MPH" : "KM/H",
                compassPoint(f.current.windDir));
  type(p, kMonoBold, cx, cy + r + 24, wind, 0);

  // Three gauges and the sun's hours.
  const int gx = 500;
  const int gw = right - gx;
  gauge(p, gx, 52, gw, "HUMIDITY", f.current.humidity);
  gauge(p, gx, 104, gw, "RAIN TODAY", f.days[0].precip);
  if (f.current.cloud >= 0) gauge(p, gx, 156, gw, "CLOUD", f.current.cloud);
  char sun[32];
  std::snprintf(sun, sizeof(sun), "SUN %s > %s", f.days[0].sunrise, f.days[0].sunset);
  type(p, kMono, gx, 226, sun);
  bar(p, margin, 244, p.w - margin * 2, 1);

  // The next day: temperature on a ruled plot, rain as bars along its floor.
  const int first = wx::nowIndex(f);
  int n = f.hourCount - first;
  if (n > 25) n = 25;
  const int px0 = 64;
  const int px1 = right;
  const int py0 = 266;
  const int py1 = 398;
  if (first >= 0 && n >= 2) {
    float lo = f.hours[first].temp;
    float hi = lo;
    for (int i = 1; i < n; ++i) {
      const float t = f.hours[first + i].temp;
      if (t < lo) lo = t;
      if (t > hi) hi = t;
    }
    // Four equal steps between whole figures, in the unit shown.
    const int shownLo = static_cast<int>(std::floor(fahr ? lo * 9.0f / 5.0f + 32.0f : lo));
    const int shownHi = static_cast<int>(std::ceil(fahr ? hi * 9.0f / 5.0f + 32.0f : hi));
    int step = (shownHi - shownLo + 3) / 4;
    if (step < 1) step = 1;
    const int top = shownLo + step * 4;
    for (int g = 0; g <= 4; ++g) {
      const int y = py0 + (py1 - py0) * g / 4;
      for (int x = px0; x < px1; x += 8) bar(p, x, y, 1, 1);
      char mark[8];
      std::snprintf(mark, sizeof(mark), "%d", top - step * g);
      type(p, kMono, px0 - 10, y + 5, mark, 1);
    }
    const auto xOf = [&](const int i) { return static_cast<float>(px0) + static_cast<float>(px1 - px0) * i / 24.0f; };
    const auto yOf = [&](const int i) {
      const float shown = fahr ? f.hours[first + i].temp * 9.0f / 5.0f + 32.0f : f.hours[first + i].temp;
      return static_cast<float>(py0) +
             static_cast<float>(py1 - py0) * (static_cast<float>(top) - shown) / static_cast<float>(step * 4);
    };
    for (int i = 0; i < n; ++i) {
      const int rain = f.hours[first + i].precip;
      if (rain < 10) continue;
      const int height = (py1 - py0) * rain / 200;
      bar(p, static_cast<int>(xOf(i)) - 5, py1 - height, 11, height);
    }
    for (int i = 0; i + 1 < n; ++i) {
      stroke(p, xOf(i), yOf(i), xOf(i + 1), yOf(i + 1), 4);
      disc(p, static_cast<int>(xOf(i + 1)), static_cast<int>(yOf(i + 1)), 2);
    }
    for (int i = 0; i < n; i += 3) {
      const int x = static_cast<int>(xOf(i));
      const int y = static_cast<int>(yOf(i));
      disc(p, x, y, 7);
      disc(p, x, y, 4, false);
      char hour[8];
      std::snprintf(hour, sizeof(hour), "%02d", f.hours[first + i].hour % 100);
      type(p, kMono, x, py1 + 22, hour, 0);
    }
  } else {
    type(p, kMono, p.w / 2, (py0 + py1) / 2, "NO HOURLY DATA. REFRESH.", 0);
  }

  bar(p, margin, p.h - kFootHeight, p.w - margin * 2, 1);
  type(p, kMonoBold, margin, p.h - 18, "[ CITIES ]");
  type(p, kMono, p.w / 2, p.h - 18, fahr ? "TEMP °F / RAIN %  NEXT 24 H" : "TEMP °C / RAIN %  NEXT 24 H", 0);
  type(p, kMonoBold, right, p.h - 18, "[ REFRESH ]", 1);
}

}  // namespace

void draw(GfxRenderer& renderer, const weatherui::ForecastModel& model, const int face) {
  if (model.forecast == nullptr || model.place == nullptr) return;
  const int which = ((face % kFaces) + kFaces) % kFaces;
  const bool onPaper = which == 0 || which == 3;
  const Page page{renderer, renderer.getScreenWidth(), renderer.getScreenHeight(), onPaper};
  renderer.fillRect(0, 0, page.w, page.h, !onPaper);
  if (which <= 1) {
    editorial(page, model);
  } else {
    instruments(page, model);
  }
}

}  // namespace weatherwide
