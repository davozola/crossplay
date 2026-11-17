#include "TodayStyles.h"

#include <GfxRenderer.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "../clock/ClockCore.h"
#include "../clock/ClockFaces.h"
#include "../history/HistoryCore.h"
#include "../markets/MarketsCore.h"
#include "../naval/NavalCore.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxSerif.h"
#include "../ui/ToyboxTheme.h"
#include "../weather/WeatherCore.h"
#include "../weather/WeatherScreens.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace wx = weather;
namespace nv = naval;
namespace mk = markets;

namespace todaystyles {

namespace {

constexpr int kFootH = 34;
constexpr float kPi = 3.14159265f;
constexpr EpdFontFamily::Style kBold = EpdFontFamily::BOLD;
constexpr EpdFontFamily::Style kRegular = EpdFontFamily::REGULAR;

int contentBottom(const toybox::Screen& screen) { return screen.device().height - lab::kBottomReserve - 6 - kFootH; }

// A small ring: the degree sign the display face does not have.
void degree(toybox::Screen& screen, const int x, const int y, const int r) {
  toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r), fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r - 3),
               fui::Color::White);
}

void rule(toybox::Screen& screen, const int y) {
  lab::ink(screen, toybox::kMargin, y, screen.device().width - toybox::kMargin * 2, toybox::kHairline);
}

// An arrowhead for a rise or a fall.
void tick(toybox::Screen& screen, const int x, const int y, const bool up) {
  for (int i = 0; i < 8; ++i) {
    const int w = up ? i * 2 + 2 : (7 - i) * 2 + 2;
    lab::ink(screen, x - w / 2, y + i, w, 1);
  }
}

// A part of the page that leads somewhere: nothing drawn, only the touch.
void zone(toybox::Screen& screen, const int x, const int y, const int w, const int h, const Target target) {
  if (w <= 0 || h <= 0) return;
  fui::ButtonProps props;
  props.label = "";
  props.action = static_cast<fui::ActionId>(kTargetAction + target);
  props.styles.explicitlySet = true;
  props.minTouchSize = 0;
  screen.button(props, lab::rect(x, y, w, h));
}

// --- Text in the reader's own faces, for the styles that leave the pixel hand. ---

void text(const GfxRenderer& r, const int font, const int x, const int y, const char* s,
          const EpdFontFamily::Style style = kRegular) {
  r.drawText(font, x, y, s, true, style);
}
void textRight(const GfxRenderer& r, const int font, const int right, const int y, const char* s,
               const EpdFontFamily::Style style = kRegular) {
  r.drawText(font, right - r.getTextWidth(font, s, style), y, s, true, style);
}
void textCentre(const GfxRenderer& r, const int font, const int cx, const int y, const char* s,
                const EpdFontFamily::Style style = kRegular) {
  r.drawText(font, cx - r.getTextWidth(font, s, style) / 2, y, s, true, style);
}

// Wrapped at spaces, each line centred or flush left; returns the height used.
int paragraph(const GfxRenderer& r, const int font, const int x, const int y, const int width, const char* s,
              const int maxLines, const bool centred) {
  std::vector<std::string> lines;
  lines.reserve(6);
  toyboxserif::wrap(r, font, s, width, maxLines, lines);
  const int lineH = r.getLineHeight(font);
  for (size_t i = 0; i < lines.size(); ++i) {
    const int w = r.getTextWidth(font, lines[i].c_str());
    r.drawText(font, centred ? x + (width - w) / 2 : x, y + static_cast<int>(i) * lineH, lines[i].c_str(), true);
  }
  return static_cast<int>(lines.size()) * lineH;
}

constexpr const char* kWeekdays[7] = {"domingo",
                                      "lunes",
                                      "martes",
                                      "mi\xC3\xA9rcoles",
                                      "jueves",
                                      "viernes",
                                      "s\xC3\xA1"
                                      "bado"};
constexpr const char* kMonths[12] = {"enero", "febrero", "marzo",      "abril",   "mayo",      "junio",
                                     "julio", "agosto",  "septiembre", "octubre", "noviembre", "diciembre"};
constexpr const char* kShortDays[7] = {"dom",
                                       "lun",
                                       "mar",
                                       "mi\xC3\xA9",
                                       "jue",
                                       "vie",
                                       "s\xC3\xA1"
                                       "b"};

const char* skyPhrase(const wx::Sky sky) {
  switch (sky) {
    case wx::Sky::Clear:
      return "Despejado";
    case wx::Sky::PartCloud:
      return "Nubes y claros";
    case wx::Sky::Cloud:
      return "Nublado";
    case wx::Sky::Fog:
      return "Niebla";
    case wx::Sky::Drizzle:
      return "Llovizna";
    case wx::Sky::Rain:
      return "Lluvia";
    case wx::Sky::Showers:
      return "Chubascos";
    case wx::Sky::Snow:
      return "Nieve";
    case wx::Sky::Thunder:
    default:
      return "Tormenta";
  }
}

// The time as four of the clock's numerals with its two dots, `boxW` each.
// Returns the width drawn; with `draw` false it only measures.
int numerals(GfxRenderer& r, const Page& page, const float x, const float y, const float boxW, const float pen,
             const bool draw = true) {
  const float gap = boxW * 0.13f;
  const float colon = boxW * 0.46f;
  const float boxH = boxW * clockcore::kNumeralHeight;
  const int digits[4] = {page.now.tm_hour / 10, page.now.tm_hour % 10, page.now.tm_min / 10, page.now.tm_min % 10};
  float nx = x;
  for (int i = 0; i < 4; ++i) {
    if (draw) {
      if (page.clockSet) {
        clockfaces::drawNumeral(r, digits[i], nx, y, boxW, pen);
      } else {
        r.fillRect(static_cast<int>(nx + boxW * 0.2f), static_cast<int>(y + boxH / 2), static_cast<int>(boxW * 0.6f),
                   static_cast<int>(pen < 2 ? 2 : pen), true);
      }
      if (i == 1) {
        const int dot = static_cast<int>(pen * 1.5f) < 4 ? 4 : static_cast<int>(pen * 1.5f);
        const int cx = static_cast<int>(nx + boxW + colon / 2);
        r.fillRoundedRect(cx - dot / 2, static_cast<int>(y + boxH * 0.32f) - dot / 2, dot, dot, dot / 2, Color::Black);
        r.fillRoundedRect(cx - dot / 2, static_cast<int>(y + boxH * 0.68f) - dot / 2, dot, dot, dot / 2, Color::Black);
      }
    }
    nx += boxW + (i == 1 ? colon : gap);
  }
  return static_cast<int>(nx - gap - x);
}

// A stretch of a circle, clockwise from north in degrees.
void arc(const GfxRenderer& r, const int cx, const int cy, const int radius, const float from, const float to,
         const int width) {
  const float step = 5.0f;
  for (float a = from; a < to; a += step) {
    const float b = a + step < to ? a + step : to;
    const float ra = (a - 90) * kPi / 180;
    const float rb = (b - 90) * kPi / 180;
    r.drawLine(cx + static_cast<int>(std::lround(radius * std::cos(ra))),
               cy + static_cast<int>(std::lround(radius * std::sin(ra))),
               cx + static_cast<int>(std::lround(radius * std::cos(rb))),
               cy + static_cast<int>(std::lround(radius * std::sin(rb))), width, true);
  }
}

// A line from the centre of a dial outwards, clockwise from north in degrees.
void spoke(const GfxRenderer& r, const int cx, const int cy, const float angle, const int from, const int to,
           const int width) {
  const float rad = (angle - 90) * kPi / 180;
  r.drawLine(cx + static_cast<int>(std::lround(from * std::cos(rad))),
             cy + static_cast<int>(std::lround(from * std::sin(rad))),
             cx + static_cast<int>(std::lround(to * std::cos(rad))),
             cy + static_cast<int>(std::lround(to * std::sin(rad))), width, true);
}

// "63,962  -2.07%" pieces for an asset; false when there is no quote.
bool quoteOf(const Page& page, const int asset, char* price, const size_t priceCap, char* change,
             const size_t changeCap, double& pct) {
  const mk::Quote& q = page.data.quotes[asset];
  if (!q.valid) return false;
  mk::formatPriceShort(q.price, price, priceCap);
  pct = mk::dayChangePercent(q);
  mk::formatPercent(pct, change, changeCap);
  return true;
}

// The sea now, or null.
const nv::Hour* seaNow(const Page& page) {
  return page.data.hasSea ? &page.data.sea.hours[nv::nowIndex(page.data.sea)] : nullptr;
}

// ---------------------------------------------------------------------------
// 1. CLASICO: the page as it first was, in the firmware's pixel hand.
// ---------------------------------------------------------------------------
void classic(toybox::Screen& screen, GfxRenderer& renderer, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  char a[32];
  char b[64];

  // The time in the clock's own numerals, and the date beside it.
  const bool clockSet = page.clockSet;
  const struct tm& parts = page.now;
  {
    const float boxW = 64;
    const float pen = 6.5f;
    const float boxH = boxW * clockcore::kNumeralHeight;
    float nx = static_cast<float>(x);
    const float ny = 26;
    const int digits[4] = {parts.tm_hour / 10, parts.tm_hour % 10, parts.tm_min / 10, parts.tm_min % 10};
    for (int i = 0; i < 4; ++i) {
      if (clockSet) {
        clockfaces::drawNumeral(renderer, digits[i], nx, ny, boxW, pen);
      } else {
        lab::ink(screen, static_cast<int>(nx) + 12, static_cast<int>(ny + boxH / 2) - 4, static_cast<int>(boxW) - 24,
                 8);
      }
      if (i == 1) {
        toybox::disc(screen, static_cast<int16_t>(nx + boxW + 17), static_cast<int16_t>(ny + boxH * 0.32f), 5,
                     fui::Color::Black);
        toybox::disc(screen, static_cast<int16_t>(nx + boxW + 17), static_cast<int16_t>(ny + boxH * 0.68f), 5,
                     fui::Color::Black);
      }
      nx += boxW + (i == 1 ? 34 : 10);
    }
    if (clockSet) {
      lab::label(screen, lab::rect(x, 30, inner, toybox::kDisplayCut.inkHeight), clockcore::weekdayEs(parts.tm_wday),
                 toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Right);
      std::snprintf(a, sizeof(a), "%d %s", parts.tm_mday, clockcore::monthEs(parts.tm_mon + 1));
      lab::value(screen, x, 30 + toybox::kDisplayCut.inkHeight + 16, inner, a, fui::TextAlign::Right);
    }
  }
  int y = 150;
  rule(screen, y);
  y += 12;

  // Weather: now on the left, the next three days on the right.
  if (d.hasForecast) {
    const wx::Forecast& f = d.forecast;
    lab::caption(screen, x, y, inner / 2, d.place.name, fui::TextAlign::Left, fui::Color::DarkGray);
    weatherui::skyIcon(screen, wx::skyOf(f.current.code), f.current.isDay, x + 52, y + 82, 92);
    std::snprintf(a, sizeof(a), "%d", wx::displayTemp(f.current.temp, d.fahrenheit));
    const int tx = x + 116;
    lab::label(screen, lab::rect(tx, y + 44, 110, toybox::kDisplayCut.inkHeight), a, toybox::kDisplayFont,
               toybox::kDisplayCut, fui::TextAlign::Left);
    degree(screen, tx + static_cast<int>(std::strlen(a)) * 31 + 10, y + 50, 7);
    if (f.dayCount > 0) {
      std::snprintf(b, sizeof(b), "%d / %d", wx::displayTemp(f.days[0].max, d.fahrenheit),
                    wx::displayTemp(f.days[0].min, d.fahrenheit));
      lab::caption(screen, tx, y + 44 + toybox::kDisplayCut.inkHeight + 12, 120, b);
      std::snprintf(b, sizeof(b), "LLUVIA %d%%", static_cast<int>(f.days[0].precip));
      lab::caption(screen, tx, y + 44 + toybox::kDisplayCut.inkHeight + 34, 120, b);
    }
    const int colW = 66;
    const int right = x + inner;
    for (int k = 1; k <= 3 && k < f.dayCount; ++k) {
      const wx::Day& day = f.days[k];
      const int cx = right - (3 - k) * colW - colW / 2;
      lab::caption(screen, cx - colW / 2, y, colW, clockcore::weekdayEs(wx::weekday(day.year, day.month, day.day)),
                   fui::TextAlign::Center, fui::Color::DarkGray);
      weatherui::skyIcon(screen, wx::skyOf(day.code), true, cx, y + 56, 44);
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(day.max, d.fahrenheit));
      lab::caption(screen, cx - colW / 2, y + 96, colW, b, fui::TextAlign::Center);
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(day.min, d.fahrenheit));
      lab::caption(screen, cx - colW / 2, y + 118, colW, b, fui::TextAlign::Center, fui::Color::DarkGray);
    }
  } else {
    lab::paragraph(screen, x, y + 30, inner,
                   d.hasPlace ? "SIN PREVISION TODAVIA. PULSA ACTUALIZAR CON WI-FI."
                              : "ELIGE UN SITIO EN WEATHER Y VUELVE: ESTA PAGINA USA EL MISMO.",
                   3);
  }
  y += 150;
  if (d.hasForecast) {
    // The hours ahead, two at a time: the hour, the temperature, the chance of rain.
    const wx::Forecast& f = d.forecast;
    const int first = wx::nowIndex(f);
    const int cols = 6;
    const int colW = inner / cols;
    lab::box(screen, x, y, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
    for (int k = 0; k < cols; ++k) {
      const int i = first + (k + 1) * 2;
      if (i >= f.hourCount) break;
      const wx::Hour& hr = f.hours[i];
      const int cx = x + k * colW;
      std::snprintf(b, sizeof(b), "%02dH", static_cast<int>(hr.hour) % 100);
      lab::caption(screen, cx, y + 12, colW, b, fui::TextAlign::Center, fui::Color::DarkGray);
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(hr.temp, d.fahrenheit));
      lab::value(screen, cx, y + 12 + toybox::kTileCut.inkHeight + 8, colW, b, fui::TextAlign::Center);
      std::snprintf(b, sizeof(b), "%d%%", static_cast<int>(hr.precip));
      lab::caption(screen, cx, y + 12 + toybox::kTileCut.inkHeight + 8 + toybox::kUiCut.inkHeight + 8, colW, b,
                   fui::TextAlign::Center, hr.precip >= 40 ? fui::Color::Black : fui::Color::DarkGray);
    }
  }
  y += 96;
  rule(screen, y);
  y += 10;

  // Markets: a row each.
  for (int i = 0; i < mk::kAssets; ++i) {
    const mk::Quote& q = d.quotes[i];
    const int rowH = 40;
    const int ty = y + (rowH - toybox::kUiCut.inkHeight) / 2;
    lab::value(screen, x, ty, 170, mk::infoOf(static_cast<mk::Asset>(i)).name);
    if (q.valid) {
      mk::formatPriceShort(q.price, a, sizeof(a));
      lab::value(screen, x + 150, ty, 170, a, fui::TextAlign::Right);
      const double pct = mk::dayChangePercent(q);
      mk::formatPercent(pct, a, sizeof(a));
      lab::caption(screen, x + inner - 96, y + (rowH - toybox::kTileCut.inkHeight) / 2, 96, a, fui::TextAlign::Right);
      tick(screen, x + 340, y + rowH / 2 - 4, pct >= 0);
    } else {
      lab::caption(screen, x + inner - 96, y + (rowH - toybox::kTileCut.inkHeight) / 2, 96, "--", fui::TextAlign::Right,
                   fui::Color::DarkGray);
    }
    y += rowH;
  }
  y += 6;
  rule(screen, y);
  y += 12;

  // The sea: wind, waves, water.
  const int cellW = inner / 3;
  if (d.hasSea) {
    const nv::Hour& h = d.sea.hours[nv::nowIndex(d.sea)];
    lab::caption(screen, x, y, cellW, "VIENTO", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::caption(screen, x + cellW, y, cellW, "OLAS", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::caption(screen, x + cellW * 2, y, cellW, "AGUA", fui::TextAlign::Left, fui::Color::DarkGray);
    const int vy = y + toybox::kTileCut.inkHeight + 10;
    const int dy = vy + toybox::kUiCut.inkHeight + 10;
    if (h.wind >= 0) {
      std::snprintf(a, sizeof(a), "%d KN", static_cast<int>(std::lround(h.wind)));
      lab::value(screen, x, vy, cellW, a);
      std::snprintf(b, sizeof(b), "F%d  %s", nv::beaufort(h.wind), nv::compass(h.windFrom));
      lab::caption(screen, x, dy, cellW, b);
    } else {
      lab::value(screen, x, vy, cellW, "--");
    }
    if (h.wave >= 0) {
      const int tenths = static_cast<int>(std::lround(h.wave * 10));
      std::snprintf(a, sizeof(a), "%d.%d M", tenths / 10, tenths % 10);
      lab::value(screen, x + cellW, vy, cellW, a);
      std::snprintf(b, sizeof(b), "%d S  %s", static_cast<int>(std::lround(h.wavePeriod < 0 ? 0 : h.wavePeriod)),
                    nv::compass(h.waveFrom));
      lab::caption(screen, x + cellW, dy, cellW, b);
    } else {
      lab::value(screen, x + cellW, vy, cellW, "--");
    }
    if (h.water > -50) {
      std::snprintf(a, sizeof(a), "%d", static_cast<int>(std::lround(h.water)));
      lab::value(screen, x + cellW * 2, vy, cellW, a);
      degree(screen, x + cellW * 2 + static_cast<int>(std::strlen(a)) * 20 + 8, vy + 5, 5);
    } else {
      lab::value(screen, x + cellW * 2, vy, cellW, "--");
    }
  } else {
    lab::caption(screen, x, y + 20, inner, d.hasPlace ? "SIN PARTE DEL MAR PARA ESTE SITIO" : "", fui::TextAlign::Left,
                 fui::Color::DarkGray);
  }

  (void)bottom;
  zone(screen, 0, 0, width, 150, Clock);
  zone(screen, 0, 150, width, 258, Weather);
  zone(screen, 0, 408, width, 150, Markets);
  zone(screen, 0, 558, width, contentBottom(screen) - 558, Sea);
}

// ---------------------------------------------------------------------------
// 2. PANEL: the pixel hand again, set larger and with the day drawn as a curve.
// ---------------------------------------------------------------------------
void panel(toybox::Screen& screen, GfxRenderer& renderer, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  char a[32];
  char b[72];

  // --- The hour, white on a black band across the top. ---
  const bool clockSet = page.clockSet;
  const struct tm& parts = page.now;
  const int heroH = 204;
  {
    const float boxW = 78;
    const float pen = 8.5f;
    const float boxH = boxW * clockcore::kNumeralHeight;
    float nx = static_cast<float>(x + 4);
    const float ny = 22;
    const int digits[4] = {parts.tm_hour / 10, parts.tm_hour % 10, parts.tm_min / 10, parts.tm_min % 10};
    for (int i = 0; i < 4; ++i) {
      if (clockSet) {
        clockfaces::drawNumeral(renderer, digits[i], nx, ny, boxW, pen);
      } else {
        renderer.fillRect(static_cast<int>(nx) + 14, static_cast<int>(ny + boxH / 2) - 5, static_cast<int>(boxW) - 28,
                          10, true);
      }
      if (i == 1) {
        const int cx = static_cast<int>(nx + boxW + 18);
        renderer.fillRoundedRect(cx - 6, static_cast<int>(ny + boxH * 0.30f) - 6, 12, 12, 6, Color::Black);
        renderer.fillRoundedRect(cx - 6, static_cast<int>(ny + boxH * 0.70f) - 6, 12, 12, 6, Color::Black);
      }
      nx += boxW + (i == 1 ? 36 : 10);
    }
    // The day under the numerals.
    if (clockSet) {
      std::snprintf(b, sizeof(b), "%s %d %s", clockcore::weekdayEs(parts.tm_wday), parts.tm_mday,
                    clockcore::monthEs(parts.tm_mon + 1));
      renderer.drawText(toybox::kUiFontId, x + 4,
                        heroH - 16 - toybox::kUiCut.inkHeight - (toybox::kUiCut.ascender - toybox::kUiCut.inkHeight), b,
                        true);
    }
  }
  rule(screen, heroH);
  int y = heroH + 12;

  // --- The weather now, large, and the three days after today. ---
  const int weatherH = 150;
  if (d.hasForecast) {
    const wx::Forecast& f = d.forecast;
    weatherui::skyIcon(screen, wx::skyOf(f.current.code), f.current.isDay, x + 58, y + 62, 112);
    std::snprintf(a, sizeof(a), "%d", wx::displayTemp(f.current.temp, d.fahrenheit));
    const int tx = x + 128;
    renderer.drawText(toybox::kLargeFontId, tx, y - 10, a, true);
    degree(screen, tx + renderer.getTextWidth(toybox::kLargeFontId, a) + 12, y + 16, 9);
    lab::caption(screen, tx, y + toybox::kLargeCut.ascender, 150, d.place.name, fui::TextAlign::Left,
                 fui::Color::DarkGray);
    if (f.dayCount > 0) {
      std::snprintf(b, sizeof(b), "%d / %d   LLUVIA %d%%", wx::displayTemp(f.days[0].max, d.fahrenheit),
                    wx::displayTemp(f.days[0].min, d.fahrenheit), static_cast<int>(f.days[0].precip));
      lab::caption(screen, tx, y + toybox::kLargeCut.ascender + toybox::kTileCut.inkHeight + 10, 190, b);
    }
    const int colW = 56;
    const int right = x + inner;
    for (int k = 1; k <= 3 && k < f.dayCount; ++k) {
      const wx::Day& day = f.days[k];
      const int cx = right - (3 - k) * colW - colW / 2;
      // The day's first three letters: the columns are narrow.
      std::snprintf(b, sizeof(b), "%.3s", clockcore::weekdayEs(wx::weekday(day.year, day.month, day.day)));
      lab::caption(screen, cx - colW / 2, y + 2, colW, b, fui::TextAlign::Center, fui::Color::DarkGray);
      weatherui::skyIcon(screen, wx::skyOf(day.code), true, cx, y + 50, 40);
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(day.max, d.fahrenheit));
      lab::caption(screen, cx - colW / 2, y + 84, colW, b, fui::TextAlign::Center);
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(day.min, d.fahrenheit));
      lab::caption(screen, cx - colW / 2, y + 104, colW, b, fui::TextAlign::Center, fui::Color::DarkGray);
    }
  } else {
    lab::paragraph(screen, x, y + 30, inner,
                   d.hasPlace ? "SIN PREVISION TODAVIA. TOCA EL ANILLO DE ABAJO CON WI-FI."
                              : "ELIGE UN SITIO EN WEATHER Y VUELVE: ESTA PAGINA USA EL MISMO.",
                   3);
  }
  y += weatherH;

  // --- The day ahead: the temperature as a line, the chance of rain as bars. ---
  const int curveH = 92;
  if (d.hasForecast) {
    const wx::Forecast& f = d.forecast;
    const int first = wx::nowIndex(f);
    int n = f.hourCount - first;
    if (n > 24) n = 24;
    if (n >= 3) {
      float lo = f.hours[first].temp;
      float hi = lo;
      for (int i = 1; i < n; ++i) {
        const float v = f.hours[first + i].temp;
        if (v < lo) lo = v;
        if (v > hi) hi = v;
      }
      const float span = hi - lo > 1 ? hi - lo : 1;
      const int plotX = x;
      const int plotW = inner - 34;
      const int top = y + 6;
      const int lineH = 44;
      const int rainTop = top + lineH + 8;
      const int rainH = 18;
      auto px = [&](const int i) { return plotX + plotW * i / (n - 1); };
      auto py = [&](const int i) { return top + static_cast<int>(lineH * (hi - f.hours[first + i].temp) / span); };
      for (int i = 0; i + 1 < n; ++i) {
        // Shade under the line down to the rain, then the line over it.
        for (int sx = px(i); sx < px(i + 1); ++sx) {
          const int sy = py(i) + (py(i + 1) - py(i)) * (sx - px(i)) / (px(i + 1) - px(i) > 0 ? px(i + 1) - px(i) : 1);
          renderer.fillRectDither(sx, sy, 1, top + lineH + 4 - sy, Color::LightGray);
        }
      }
      for (int i = 0; i + 1 < n; ++i) renderer.drawLine(px(i), py(i), px(i + 1), py(i + 1), 3, true);
      const int barW = plotW / n - 2 > 3 ? plotW / n - 2 : 3;
      for (int i = 0; i < n; ++i) {
        const int h = rainH * f.hours[first + i].precip / 100;
        renderer.fillRect(px(i) - (i == 0 ? 0 : barW / 2), rainTop + rainH - (h > 1 ? h : 1), barW, h > 1 ? h : 1,
                          true);
      }
      // The extremes at the right, and the hour under every sixth.
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(hi, d.fahrenheit));
      lab::caption(screen, x + inner - 30, top - 2, 30, b, fui::TextAlign::Right);
      std::snprintf(b, sizeof(b), "%d", wx::displayTemp(lo, d.fahrenheit));
      lab::caption(screen, x + inner - 30, top + lineH - toybox::kTileCut.inkHeight + 2, 30, b, fui::TextAlign::Right,
                   fui::Color::DarkGray);
      for (int i = 0; i < n; i += 6) {
        std::snprintf(b, sizeof(b), "%02dH", static_cast<int>(f.hours[first + i].hour) % 100);
        lab::caption(screen, px(i) - (i == 0 ? 0 : 20), rainTop + rainH + 5, 44, b,
                     i == 0 ? fui::TextAlign::Left : fui::TextAlign::Center, fui::Color::DarkGray);
      }
    }
  }
  y += curveH + 10;

  // --- Markets: a tile each, with the day's move and the month's line. ---
  const int tileGap = 8;
  const int tileW = (inner - tileGap * (mk::kAssets - 1)) / mk::kAssets;
  const int tileH = 100;
  for (int i = 0; i < mk::kAssets; ++i) {
    const mk::Quote& q = d.quotes[i];
    const int tx = x + i * (tileW + tileGap);
    renderer.drawRoundedRect(tx, y, tileW, tileH, 2, 10, true);
    lab::caption(screen, tx + 10, y + 10, tileW - 20, mk::infoOf(static_cast<mk::Asset>(i)).name);
    if (!q.valid) {
      lab::caption(screen, tx, y + tileH / 2, tileW, "--", fui::TextAlign::Center, fui::Color::DarkGray);
      continue;
    }
    mk::formatPriceShort(q.price, a, sizeof(a));
    lab::value(screen, tx + 10, y + 10 + toybox::kTileCut.inkHeight + 8, tileW - 20, a);
    const double pct = mk::dayChangePercent(q);
    mk::formatPercent(pct, a, sizeof(a));
    // A fall is a solid pill, a rise an outlined one.
    const int pillW = renderer.getTextWidth(toybox::kTileFontId, a) + 14;
    const int pillY = y + 10 + toybox::kTileCut.inkHeight + 8 + toybox::kUiCut.inkHeight + 7;
    if (pct < 0) {
      renderer.fillRoundedRect(tx + 10, pillY, pillW, 20, 8, Color::Black);
    } else {
      renderer.drawRoundedRect(tx + 10, pillY, pillW, 20, 1, 8, true);
    }
    renderer.drawText(
        toybox::kTileFontId, tx + 17,
        pillY + (20 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight), a,
        pct >= 0);
    // The last month of closes along the foot of the tile.
    int n = q.count < 22 ? q.count : 22;
    if (n >= 3) {
      const int from = q.count - n;
      float lo = q.closes[from];
      float hi = lo;
      for (int k = 1; k < n; ++k) {
        if (q.closes[from + k] < lo) lo = q.closes[from + k];
        if (q.closes[from + k] > hi) hi = q.closes[from + k];
      }
      const float span = hi - lo > 0 ? hi - lo : 1;
      const int sx0 = tx + pillW + 18;
      const int sw = tileW - pillW - 28;
      const int sy0 = pillY - 2;
      const int sh = 24;
      if (sw > 20) {
        for (int k = 0; k + 1 < n; ++k) {
          renderer.drawLine(sx0 + sw * k / (n - 1), sy0 + static_cast<int>(sh * (hi - q.closes[from + k]) / span),
                            sx0 + sw * (k + 1) / (n - 1),
                            sy0 + static_cast<int>(sh * (hi - q.closes[from + k + 1]) / span), 2, true);
        }
      }
    }
  }
  y += tileH + 12;

  // --- The sea: wind, waves, water. ---
  const int cellW = inner / 3;
  if (d.hasSea) {
    const nv::Hour& h = d.sea.hours[nv::nowIndex(d.sea)];
    const int vy = y + toybox::kTileCut.inkHeight + 8;
    lab::caption(screen, x, y, cellW, "VIENTO", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::caption(screen, x + cellW, y, cellW, "OLAS", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::caption(screen, x + cellW * 2, y, cellW, "AGUA", fui::TextAlign::Left, fui::Color::DarkGray);
    if (h.wind >= 0) {
      std::snprintf(a, sizeof(a), "%d KN", static_cast<int>(std::lround(h.wind)));
      lab::value(screen, x, vy, cellW, a);
      std::snprintf(b, sizeof(b), "F%d %s", nv::beaufort(h.wind), nv::compass(h.windFrom));
      lab::caption(screen, x, vy + toybox::kUiCut.inkHeight + 6, cellW, b, fui::TextAlign::Left, fui::Color::DarkGray);
    } else {
      lab::value(screen, x, vy, cellW, "--");
    }
    if (h.wave >= 0) {
      const int tenths = static_cast<int>(std::lround(h.wave * 10));
      std::snprintf(a, sizeof(a), "%d.%d M", tenths / 10, tenths % 10);
      lab::value(screen, x + cellW, vy, cellW, a);
      std::snprintf(b, sizeof(b), "%d S", static_cast<int>(std::lround(h.wavePeriod < 0 ? 0 : h.wavePeriod)));
      lab::caption(screen, x + cellW, vy + toybox::kUiCut.inkHeight + 6, cellW, b, fui::TextAlign::Left,
                   fui::Color::DarkGray);
    } else {
      lab::value(screen, x + cellW, vy, cellW, "--");
    }
    if (h.water > -50) {
      std::snprintf(a, sizeof(a), "%d", static_cast<int>(std::lround(h.water)));
      lab::value(screen, x + cellW * 2, vy, cellW, a);
      degree(screen, x + cellW * 2 + static_cast<int>(std::strlen(a)) * 20 + 8, vy + 5, 5);
    } else {
      lab::value(screen, x + cellW * 2, vy, cellW, "--");
    }
  } else {
    lab::caption(screen, x, y + 14, inner, d.hasPlace ? "SIN PARTE DEL MAR PARA ESTE SITIO" : "", fui::TextAlign::Left,
                 fui::Color::DarkGray);
  }
  y += toybox::kTileCut.inkHeight + 8 + toybox::kUiCut.inkHeight + 6 + toybox::kTileCut.inkHeight + 10;

  // --- A day like this one, when EFEMERIDES has fetched it. ---
  if (page.eventText[0] != 0 && bottom - y >= 70) {
    rule(screen, y);
    y += 10;
    history::yearLabel(page.eventYear, a, sizeof(a));
    std::snprintf(b, sizeof(b), "UN DIA COMO HOY, %s", a);
    const int tabW = renderer.getTextWidth(toybox::kTileFontId, b) + 16;
    renderer.fillRect(x, y, tabW, 22, true);
    renderer.drawText(
        toybox::kTileFontId, x + 8,
        y + (22 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight), b, false);
    toyboxserif::draw(renderer, toybox::kReadingSmallFontId, 25, x, y + 24, inner, page.eventText,
                      (bottom - y - 24) / 25);
  }
  zone(screen, 0, 0, width, 204, Clock);
  zone(screen, 0, 204, width, 264, Weather);
  zone(screen, 0, 468, width, 112, Markets);
  zone(screen, 0, 580, width, 72, Sea);
  zone(screen, 0, 652, width, contentBottom(screen) - 652, Event);
}

// ---------------------------------------------------------------------------
// 3. DIARIO: the day as the front page of a paper, in the reader's serif.
// ---------------------------------------------------------------------------
void paper(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = 22;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  char a[48];
  char b[160];
  auto thin = [&](const int y) { r.fillRect(x, y, inner, 1, true); };
  auto thick = [&](const int y) {
    r.fillRect(x, y, inner, 3, true);
    r.fillRect(x, y + 5, inner, 1, true);
  };

  // The masthead between rules, and the dateline under it.
  int y = 14;
  thick(y);
  y += 12;
  textCentre(r, NOTOSERIF_18_FONT_ID, width / 2, y, "La Jornada", kBold);

  y += r.getLineHeight(NOTOSERIF_18_FONT_ID) + 4;
  thin(y);
  y += 5;
  if (page.clockSet) {
    std::snprintf(b, sizeof(b), "%s, %d de %s", kWeekdays[page.now.tm_wday % 7], page.now.tm_mday,
                  kMonths[page.now.tm_mon % 12]);
    b[0] = static_cast<char>(b[0] >= 'a' && b[0] <= 'z' ? b[0] - 32 : b[0]);
    text(r, NOTOSANS_12_FONT_ID, x, y, b);
  }
  if (d.hasPlace) {
    std::snprintf(a, sizeof(a), "%s", d.place.name);
    for (char* c = a + 1; *c != 0; ++c) {
      if (*c >= 'A' && *c <= 'Z' && c[-1] != ' ') *c = static_cast<char>(*c + 32);
    }
    textRight(r, NOTOSANS_12_FONT_ID, x + inner, y, a);
  }
  y += r.getLineHeight(NOTOSANS_12_FONT_ID) + 4;
  thin(y);
  y += 12;

  // The hour, where a paper would carry its photograph.
  const float boxW = 44;
  const int timeW = numerals(r, page, 0, 0, boxW, 5, false);
  numerals(r, page, static_cast<float>((width - timeW) / 2), static_cast<float>(y), boxW, 5);
  y += static_cast<int>(boxW * clockcore::kNumeralHeight) + 12;
  thin(y);
  zone(screen, 0, 0, width, y, Clock);
  const int weatherTop = y;
  y += 10;

  // The weather as the day's lead story.
  if (d.hasForecast) {
    const wx::Forecast& f = d.forecast;
    std::snprintf(b, sizeof(b), "%s, %d\xC2\xB0", skyPhrase(wx::skyOf(f.current.code)),
                  wx::displayTemp(f.current.temp, d.fahrenheit));
    text(r, NOTOSERIF_18_FONT_ID, x, y, b, kBold);
    y += r.getLineHeight(NOTOSERIF_18_FONT_ID) + 2;
    if (f.dayCount > 0) {
      std::snprintf(b, sizeof(b), "M\xC3\xA1x. %d\xC2\xB0, m\xC3\xADn. %d\xC2\xB0. Lluvia %d %%.",
                    wx::displayTemp(f.days[0].max, d.fahrenheit), wx::displayTemp(f.days[0].min, d.fahrenheit),
                    static_cast<int>(f.days[0].precip));
      y += paragraph(r, NOTOSERIF_12_FONT_ID, x, y, inner, b, 2, false) + 4;
    }
    // The days after, in three columns of small print.
    for (int k = 1; k <= 3 && k < f.dayCount; ++k) {
      const wx::Day& day = f.days[k];
      std::snprintf(b, sizeof(b), "%s %d\xC2\xB0/%d\xC2\xB0", kShortDays[wx::weekday(day.year, day.month, day.day) % 7],
                    wx::displayTemp(day.max, d.fahrenheit), wx::displayTemp(day.min, d.fahrenheit));
      if (k == 1) text(r, NOTOSANS_12_FONT_ID, x, y, b);
      if (k == 2) textCentre(r, NOTOSANS_12_FONT_ID, width / 2, y, b);
      if (k == 3) textRight(r, NOTOSANS_12_FONT_ID, x + inner, y, b);
    }
    y += r.getLineHeight(NOTOSANS_12_FONT_ID) + 8;
  } else {
    y += paragraph(r, NOTOSERIF_12_FONT_ID, x, y, inner,
                   d.hasPlace ? "Sin previsi\xC3\xB3n todav\xC3\xAD"
                                "a. Toca el anillo de abajo con wifi."
                              : "Elige un sitio en Weather y vuelve: esta p\xC3\xA1gina usa el mismo.",
                   3, false) +
         8;
  }
  thin(y);
  zone(screen, 0, weatherTop, width, y - weatherTop, Weather);
  const int marketsTop = y;
  y += 8;

  // Markets, as a paper sets a table: name, a row of dots, the figures.
  text(r, NOTOSANS_12_FONT_ID, x, y, "MERCADOS", kBold);
  y += r.getLineHeight(NOTOSANS_12_FONT_ID) + 2;
  const int rowH = r.getLineHeight(NOTOSERIF_12_FONT_ID) + 2;
  for (int i = 0; i < mk::kAssets; ++i) {
    const char* name = mk::infoOf(static_cast<mk::Asset>(i)).name;
    // The catalogue's names are capitals; a paper sets them in lower case.
    std::snprintf(a, sizeof(a), "%s", name);
    for (char* c = a + 1; *c != 0; ++c) {
      if (*c >= 'A' && *c <= 'Z' && c[-1] != ' ' && c[-1] != '&' && c[-1] != '/') *c = static_cast<char>(*c + 32);
    }
    text(r, NOTOSERIF_12_FONT_ID, x, y, a);
    const int nameW = r.getTextWidth(NOTOSERIF_12_FONT_ID, a);
    char price[24];
    char change[16];
    double pct = 0;
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      textRight(r, NOTOSERIF_12_FONT_ID, x + inner, y, change, pct < 0 ? kBold : kRegular);
      const int priceRight = x + inner - 100;
      textRight(r, NOTOSERIF_12_FONT_ID, priceRight, y, price, kBold);
      const int priceW = r.getTextWidth(NOTOSERIF_12_FONT_ID, price, kBold);
      for (int dx = x + nameW + 10; dx < priceRight - priceW - 8; dx += 7) r.fillRect(dx, y + rowH - 13, 2, 2, true);
    } else {
      textRight(r, NOTOSERIF_12_FONT_ID, x + inner, y, "sin datos");
    }
    y += rowH;
  }
  y += 4;
  thin(y);
  zone(screen, 0, marketsTop, width, y - marketsTop, Markets);
  const int seaTop = y;
  y += 8;

  // The sea, in a sentence.
  if (const nv::Hour* h = seaNow(page)) {
    int at = 0;
    b[0] = 0;
    if (h->wind >= 0) {
      at += std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "Mar: %s %d kn", nv::compass(h->windFrom),
                          static_cast<int>(std::lround(h->wind)));
    }
    if (h->wave >= 0 && at < static_cast<int>(sizeof(b)) - 48) {
      const int tenths = static_cast<int>(std::lround(h->wave * 10));
      at += std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%solas %d,%d m", at ? " \xC2\xB7 " : "",
                          tenths / 10, tenths % 10);
    }
    if (h->water > -50 && at < static_cast<int>(sizeof(b)) - 24) {
      std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%s%d\xC2\xB0", at ? " \xC2\xB7 " : "agua ",
                    static_cast<int>(std::lround(h->water)));
    }
    y += paragraph(r, NOTOSERIF_12_FONT_ID, x, y, inner, b, 2, false) + 6;
    thin(y);
    zone(screen, 0, seaTop, width, y - seaTop, Sea);
    y += 8;
  }

  // A day like this one, as the paper's look back.
  const int eventTop = y;
  if (page.eventText[0] != 0 && bottom - y >= r.getLineHeight(NOTOSERIF_12_FONT_ID) * 2 + 20) {
    char year[24];
    history::yearLabel(page.eventYear, year, sizeof(year));
    std::snprintf(a, sizeof(a),
                  "TAL D\xC3\x8D"
                  "A COMO HOY, %s",
                  year);
    text(r, NOTOSANS_12_FONT_ID, x, y, a, kBold);
    y += r.getLineHeight(NOTOSANS_12_FONT_ID) + 2;
    const int lines = (bottom - y) / r.getLineHeight(NOTOSERIF_12_FONT_ID);
    paragraph(r, NOTOSERIF_12_FONT_ID, x, y, inner, page.eventText, lines, false);
    zone(screen, 0, eventTop, width, bottom - eventTop, Event);
  }
}

// ---------------------------------------------------------------------------
// 4. CALMA: almost nothing. The hour in a hairline, and a few quiet lines.
// ---------------------------------------------------------------------------
void calm(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int bottom = contentBottom(screen);
  const int cx = width / 2;
  char b[120];

  const float boxW = 86;
  int y = 40;
  const int timeW = numerals(r, page, 0, 0, boxW, 3, false);
  numerals(r, page, static_cast<float>((width - timeW) / 2), static_cast<float>(y), boxW, 3);
  y += static_cast<int>(boxW * clockcore::kNumeralHeight) + 20;
  r.fillRect(cx - 24, y, 48, 2, true);
  y += 16;
  if (page.clockSet) {
    std::snprintf(b, sizeof(b), "%s %d de %s", kWeekdays[page.now.tm_wday % 7], page.now.tm_mday,
                  kMonths[page.now.tm_mon % 12]);
    textCentre(r, NOTOSANS_16_FONT_ID, cx, y, b);
  }
  y += r.getLineHeight(NOTOSANS_16_FONT_ID) + 14;
  zone(screen, 0, 0, width, y, Clock);

  const int lineH = r.getLineHeight(NOTOSANS_14_FONT_ID) + 8;
  const int weatherTop = y;
  if (d.hasForecast) {
    const wx::Forecast& f = d.forecast;
    std::snprintf(b, sizeof(b), "%d\xC2\xB0   %s", wx::displayTemp(f.current.temp, d.fahrenheit),
                  skyPhrase(wx::skyOf(f.current.code)));
    textCentre(r, NOTOSANS_18_FONT_ID, cx, y, b, kBold);
    y += r.getLineHeight(NOTOSANS_18_FONT_ID) + 6;
    if (f.dayCount > 0) {
      std::snprintf(b, sizeof(b), "de %d\xC2\xB0 a %d\xC2\xB0  \xC2\xB7  lluvia %d %%",
                    wx::displayTemp(f.days[0].min, d.fahrenheit), wx::displayTemp(f.days[0].max, d.fahrenheit),
                    static_cast<int>(f.days[0].precip));
      textCentre(r, NOTOSANS_14_FONT_ID, cx, y, b);
    }
    y += lineH;
  } else {
    textCentre(r, NOTOSANS_14_FONT_ID, cx, y, "sin previsi\xC3\xB3n");
    y += lineH;
  }
  y += 12;
  zone(screen, 0, weatherTop, width, y - weatherTop, Weather);

  const int marketsTop = y;
  for (int i = 0; i < mk::kAssets; ++i) {
    char price[24];
    char change[16];
    double pct = 0;
    const char* name = mk::infoOf(static_cast<mk::Asset>(i)).name;
    // Name to the left of the centre line, figures to the right of it.
    textRight(r, NOTOSANS_14_FONT_ID, cx - 14, y, name);
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      text(r, NOTOSANS_14_FONT_ID, cx + 14, y, price, kBold);
      text(r, NOTOSANS_12_FONT_ID, cx + 14 + r.getTextWidth(NOTOSANS_14_FONT_ID, price, kBold) + 10, y + 3, change);
    } else {
      text(r, NOTOSANS_14_FONT_ID, cx + 14, y, "--");
    }
    y += lineH - 4;
  }
  r.fillRect(cx, marketsTop + 4, 1, y - marketsTop - 14, true);
  y += 16;
  zone(screen, 0, marketsTop, width, y - marketsTop, Markets);

  const int seaTop = y;
  if (const nv::Hour* h = seaNow(page)) {
    int at = 0;
    b[0] = 0;
    if (h->wind >= 0) {
      at += std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%d kn %s",
                          static_cast<int>(std::lround(h->wind)), nv::compass(h->windFrom));
    }
    if (h->wave >= 0 && at < static_cast<int>(sizeof(b)) - 40) {
      const int tenths = static_cast<int>(std::lround(h->wave * 10));
      at += std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%solas %d,%d m", at ? "  \xC2\xB7  " : "",
                          tenths / 10, tenths % 10);
    }
    if (h->water > -50 && at < static_cast<int>(sizeof(b)) - 24) {
      std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%sagua %d\xC2\xB0", at ? "  \xC2\xB7  " : "",
                    static_cast<int>(std::lround(h->water)));
    }
    textCentre(r, NOTOSANS_12_FONT_ID, cx, y, b);
    y += lineH;
    zone(screen, 0, seaTop, width, y - seaTop, Sea);
  }

  // A day like this, small, at the foot.
  if (page.eventText[0] != 0) {
    const int lh = r.getLineHeight(NOTOSERIF_12_FONT_ID);
    const int lines = (bottom - y - lh) / lh;
    if (lines >= 1) {
      char year[24];
      history::yearLabel(page.eventYear, year, sizeof(year));
      textCentre(r, NOTOSANS_12_FONT_ID, cx, y, year, kBold);
      paragraph(r, NOTOSERIF_12_FONT_ID, 40, y + lh, width - 80, page.eventText, lines > 3 ? 3 : lines, true);
      zone(screen, 0, y, width, bottom - y, Event);
    }
  }
}

// ---------------------------------------------------------------------------
// 5. ESFERAS: the day on dials. A clock with hands, three gauges, three bars.
// ---------------------------------------------------------------------------
void dials(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = 20;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  char a[48];
  char b[96];

  // The clock: a ring, twelve marks and two hands.
  const int ccx = x + 104;
  const int ccy = 116;
  const int cr = 98;
  arc(r, ccx, ccy, cr, 0, 360, 3);
  for (int h = 0; h < 12; ++h)
    spoke(r, ccx, ccy, h * 30.0f, h % 3 == 0 ? cr - 20 : cr - 11, cr - 4, h % 3 == 0 ? 4 : 2);
  if (page.clockSet) {
    const float minute = page.now.tm_min * 6.0f;
    const float hour = (page.now.tm_hour % 12) * 30.0f + page.now.tm_min * 0.5f;
    spoke(r, ccx, ccy, hour, -12, cr - 48, 7);
    spoke(r, ccx, ccy, minute, -18, cr - 24, 4);
  }
  r.fillRoundedRect(ccx - 7, ccy - 7, 14, 14, 7, Color::Black);
  r.fillRoundedRect(ccx - 2, ccy - 2, 4, 4, 2, Color::White);

  // The date beside it, the day of the month in the clock's numerals.
  const int dx = ccx + cr + 26;
  const int dw = x + inner - dx;
  if (page.clockSet) {
    std::snprintf(a, sizeof(a), "%s", kWeekdays[page.now.tm_wday % 7]);
    textCentre(r, NOTOSANS_16_FONT_ID, dx + dw / 2, 14, a, kBold);
    const int day = page.now.tm_mday;
    const float boxW = 50;
    const int top = 14 + r.getLineHeight(NOTOSANS_16_FONT_ID) + 6;
    const int under = top + static_cast<int>(boxW * clockcore::kNumeralHeight);
    const float total = day >= 10 ? boxW * 2 + 8 : boxW;
    float nx = dx + (dw - total) / 2;
    if (day >= 10) {
      clockfaces::drawNumeral(r, day / 10, nx, static_cast<float>(top), boxW, 5);
      nx += boxW + 8;
    }
    clockfaces::drawNumeral(r, day % 10, nx, static_cast<float>(top), boxW, 5);
    textCentre(r, NOTOSANS_16_FONT_ID, dx + dw / 2, under + 8, kMonths[page.now.tm_mon % 12]);
  }
  zone(screen, 0, 0, width, 232, Clock);

  // Three gauges: how warm, how likely the rain, where the wind is from.
  const int gy = 318;
  const int gr = 56;
  const int gcx[3] = {x + 70, width / 2, x + inner - 70};
  const int labelY = gy + gr + 8;
  const int summaryY = labelY + r.getLineHeight(NOTOSANS_12_FONT_ID) + 2;
  r.fillRect(x, 232, inner, 1, true);
  if (d.hasForecast) {
    const wx::Forecast& f = d.forecast;
    // Temperature: a 270-degree scale from the day's low to its high.
    arc(r, gcx[0], gy, gr, 225, 495, 2);
    const float lo = f.dayCount > 0 ? f.days[0].min : f.current.temp - 5;
    const float hi = f.dayCount > 0 ? f.days[0].max : f.current.temp + 5;
    float part = hi - lo > 0.5f ? (f.current.temp - lo) / (hi - lo) : 0.5f;
    if (part < 0) part = 0;
    if (part > 1) part = 1;
    arc(r, gcx[0], gy, gr - 6, 225, 225 + 270 * part, 7);
    spoke(r, gcx[0], gy, 225 + 270 * part, gr - 16, gr + 6, 3);
    std::snprintf(a, sizeof(a), "%d\xC2\xB0", wx::displayTemp(f.current.temp, d.fahrenheit));
    textCentre(r, NOTOSANS_18_FONT_ID, gcx[0] + 3, gy - r.getLineHeight(NOTOSANS_18_FONT_ID) / 2, a, kBold);
    std::snprintf(b, sizeof(b), "%d\xC2\xB0 a %d\xC2\xB0", wx::displayTemp(lo, d.fahrenheit),
                  wx::displayTemp(hi, d.fahrenheit));
    textCentre(r, NOTOSANS_12_FONT_ID, gcx[0], labelY, f.dayCount > 0 ? b : "");

    // Rain: a ring filled as far round as it is likely.
    const int rain = f.dayCount > 0 ? f.days[0].precip : 0;
    arc(r, gcx[1], gy, gr, 0, 360, 2);
    arc(r, gcx[1], gy, gr - 7, 0, 3.6f * static_cast<float>(rain), 9);
    std::snprintf(a, sizeof(a), "%d%%", rain % 1000);
    textCentre(r, NOTOSANS_18_FONT_ID, gcx[1], gy - r.getLineHeight(NOTOSANS_18_FONT_ID) / 2, a, kBold);
    textCentre(r, NOTOSANS_12_FONT_ID, gcx[1], labelY, "lluvia hoy");
  } else {
    textCentre(r, NOTOSANS_14_FONT_ID, (gcx[0] + gcx[1]) / 2, gy - 10, "sin previsi\xC3\xB3n");
  }
  zone(screen, 0, 232, gcx[1] + gr + 14, summaryY - 232, Weather);

  // Wind: a compass, the arrow flying the way the wind blows.
  arc(r, gcx[2], gy, gr, 0, 360, 2);
  for (int k = 0; k < 8; ++k) spoke(r, gcx[2], gy, k * 45.0f, k % 2 == 0 ? gr - 9 : gr - 5, gr, 2);
  textCentre(r, NOTOSANS_12_FONT_ID, gcx[2], gy - gr - r.getLineHeight(NOTOSANS_12_FONT_ID) - 1, "N", kBold);
  if (const nv::Hour* h = seaNow(page)) {
    if (h->wind >= 0) {
      const float to = static_cast<float>(h->windFrom) + 180;
      spoke(r, gcx[2], gy, to, gr - 34, gr - 12, 4);
      spoke(r, gcx[2], gy, to - 14, gr - 26, gr - 12, 4);
      spoke(r, gcx[2], gy, to + 14, gr - 26, gr - 12, 4);
      spoke(r, gcx[2], gy, to + 180, gr - 34, gr - 14, 2);
      std::snprintf(a, sizeof(a), "%d", static_cast<int>(std::lround(h->wind)) % 1000);
      textCentre(r, NOTOSANS_18_FONT_ID, gcx[2], gy - r.getLineHeight(NOTOSANS_18_FONT_ID) / 2, a, kBold);
      std::snprintf(b, sizeof(b), "nudos %s", nv::compass(h->windFrom));
      textCentre(r, NOTOSANS_12_FONT_ID, gcx[2], labelY, b);
    }
  } else {
    textCentre(r, NOTOSANS_12_FONT_ID, gcx[2], labelY, "sin parte del mar");
  }
  zone(screen, gcx[1] + gr + 14, 232, width - gcx[1] - gr - 14, summaryY - 232, Sea);

  // Markets: a bar each from a centre line, left for a fall and right for a rise.
  // One line under the three: the sky in words, and the water.
  {
    int at = 0;
    b[0] = 0;
    if (d.hasForecast) at += std::snprintf(b, sizeof(b), "%s", skyPhrase(wx::skyOf(d.forecast.current.code)));
    if (const nv::Hour* h = seaNow(page)) {
      if (h->wave >= 0 && at < static_cast<int>(sizeof(b)) - 32) {
        const int tenths = static_cast<int>(std::lround(h->wave * 10));
        at += std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%solas %d,%d m", at ? "  \xC2\xB7  " : "",
                            tenths / 10, tenths % 10);
      }
      if (h->water > -50 && at < static_cast<int>(sizeof(b)) - 24) {
        std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at), "%sagua %d\xC2\xB0", at ? "  \xC2\xB7  " : "",
                      static_cast<int>(std::lround(h->water)));
      }
    }
    textCentre(r, NOTOSANS_12_FONT_ID, width / 2, summaryY, b);
  }
  zone(screen, 0, summaryY, width, r.getLineHeight(NOTOSANS_12_FONT_ID) + 6, Weather);
  int y = summaryY + r.getLineHeight(NOTOSANS_12_FONT_ID) + 6;
  r.fillRect(x, y, inner, 1, true);
  y += 10;
  const int marketsTop = y - 10;
  const int axis = x + 300;
  const int reach = 56;  // pixels for five percent
  const int rowH = 44;
  for (int i = 0; i < mk::kAssets; ++i) {
    char price[24];
    char change[16];
    double pct = 0;
    text(r, NOTOSANS_14_FONT_ID, x, y, mk::infoOf(static_cast<mk::Asset>(i)).name, kBold);
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      textRight(r, NOTOSANS_14_FONT_ID, axis - reach - 12, y, price);
      double clipped = pct > 5 ? 5 : (pct < -5 ? -5 : pct);
      int w = static_cast<int>(std::lround(reach * (clipped < 0 ? -clipped : clipped) / 5.0));
      if (w < 2) w = 2;
      if (pct < 0) {
        r.fillRect(axis - w, y + 6, w, 18, true);
      } else {
        r.drawRect(axis, y + 6, w, 18, 2, true);
      }
      textRight(r, NOTOSANS_14_FONT_ID, x + inner, y, change, kBold);
    } else {
      textRight(r, NOTOSANS_14_FONT_ID, x + inner, y, "--");
    }
    y += rowH;
  }
  r.fillRect(axis, marketsTop + 10, 1, y - marketsTop - 14, true);
  zone(screen, 0, marketsTop, width, y - marketsTop, Markets);

  // A day like this one.
  if (page.eventText[0] != 0) {
    const int lh = r.getLineHeight(NOTOSERIF_12_FONT_ID);
    const int lines = (bottom - y - 8 - lh) / lh;
    if (lines >= 1) {
      r.fillRect(x, y, inner, 1, true);
      char year[24];
      history::yearLabel(page.eventYear, year, sizeof(year));
      std::snprintf(a, sizeof(a),
                    "Un d\xC3\xAD"
                    "a como hoy, %s",
                    year);
      text(r, NOTOSANS_12_FONT_ID, x, y + 8, a, kBold);
      paragraph(r, NOTOSERIF_12_FONT_ID, x, y + 8 + lh, inner, page.eventText, lines > 3 ? 3 : lines, false);
      zone(screen, 0, y, width, bottom - y, Event);
    }
  }
}

// What every style shares: when the data is from, which of the five this is,
// and the ring that fetches everything again.
void foot(toybox::Screen& screen, GfxRenderer& r, const Page& page, const int style) {
  const int width = screen.device().width;
  const int top = contentBottom(screen);
  const int cy = top + kFootH / 2 + 2;
  r.fillRectDither(toybox::kMargin, top + 1, width - toybox::kMargin * 2, 1, Color::DarkGray);
  if (page.data.hasForecast && std::strlen(page.data.forecast.current.time) >= 16) {
    char when[24];
    std::snprintf(when, sizeof(when), "datos %.5s", page.data.forecast.current.time + 11);
    text(r, NOTOSANS_12_FONT_ID, toybox::kMargin, cy - r.getLineHeight(NOTOSANS_12_FONT_ID) / 2, when);
  }
  for (int i = 0; i < kStyles; ++i) {
    const int dx = width / 2 + (i - kStyles / 2) * 16;
    if (i == style) {
      r.fillRoundedRect(dx - 5, cy - 5, 10, 10, 5, Color::Black);
    } else {
      r.drawRoundedRect(dx - 4, cy - 4, 8, 8, 1, 4, true);
    }
  }
  // The ring with its arrowhead, and a generous touch around it.
  const int rx = width - toybox::kMargin - 13;
  arc(r, rx, cy, 10, 60, 360, 2);
  const int ax[3] = {rx - 1, rx + 8, rx - 1};
  const int ay[3] = {cy - 16, cy - 10, cy - 4};
  r.fillPolygon(ax, ay, 3, true);
  zone(screen, width - 110, top, 110, kFootH + 6, Refresh);
}

}  // namespace

const char* name(const int style) {
  static constexpr const char* kNames[kStyles] = {"CLASICO", "PANEL", "DIARIO", "CALMA", "ESFERAS"};
  return kNames[style >= 0 && style < kStyles ? style : 0];
}

void draw(const int style, toybox::Screen& screen, GfxRenderer& renderer, const Page& page) {
  switch (style) {
    case 1:
      panel(screen, renderer, page);
      break;
    case 2:
      paper(screen, renderer, page);
      break;
    case 3:
      calm(screen, renderer, page);
      break;
    case 4:
      dials(screen, renderer, page);
      break;
    case 0:
    default:
      classic(screen, renderer, page);
      break;
  }
  foot(screen, renderer, page, style >= 0 && style < kStyles ? style : 0);
}

}  // namespace todaystyles
