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

// The page runs to the foot of the panel: a touch device draws no button hints there.
int contentBottom(const toybox::Screen& screen) { return screen.device().height - 6 - kFootH; }

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

// What happened on days like this one, one event after another for as far as
// the page goes: the block that takes whatever height a style has left.
enum class EventLook : uint8_t { Pixel, Reader, Centred };

void eventsBlock(toybox::Screen& screen, const GfxRenderer& r, const Page& page, const int x, int y, const int width,
                 const int bottom, const EventLook look) {
  const bool pixel = look == EventLook::Pixel;
  // The music first, when there is some: one line, which opens the Jukebox.
  if (page.playing[0] != 0 && bottom - y > 60) {
    const int musicTop = y;
    const int font = NOTOSANS_12_FONT_ID;
    const std::string line = r.truncatedText(font, page.playing, width);
    if (pixel) {
      const char* head = "SUENA AHORA";
      const int tabW = r.getTextWidth(toybox::kTileFontId, head) + 16;
      r.fillRect(x, y, tabW, 22, true);
      r.drawText(toybox::kTileFontId, x + 8,
                 y + (22 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight),
                 head, false);
      y += 26;
      r.drawText(font, x, y, line.c_str(), true);
    } else if (look == EventLook::Centred) {
      textCentre(r, font, x + width / 2, y, "Suena ahora", kBold);
      y += r.getLineHeight(font);
      textCentre(r, font, x + width / 2, y, line.c_str());
    } else {
      text(r, font, x, y, "Suena ahora", kBold);
      y += r.getLineHeight(font);
      text(r, font, x, y, line.c_str());
    }
    y += r.getLineHeight(font) + (pixel ? 10 : 6);
    zone(screen, 0, musicTop, screen.device().width, y - musicTop, Music);
  }
  if (page.eventCount == 0) return;
  const int top = y;
  const int bodyFont = pixel ? toybox::kReadingSmallFontId : NOTOSERIF_12_FONT_ID;
  const int lineH = pixel ? 25 : r.getLineHeight(bodyFont);
  const int headH = pixel ? 26 : r.getLineHeight(NOTOSANS_12_FONT_ID);
  bool drawn = false;
  for (int i = 0; i < page.eventCount; ++i) {
    const int lines = (bottom - y - headH - (pixel ? 8 : 0)) / lineH;
    if (lines < 1) break;
    char year[24];
    char head[56];
    history::yearLabel(page.events[i].year, year, sizeof(year));
    if (pixel) {
      std::snprintf(head, sizeof(head), i == 0 ? "UN DIA COMO HOY, %s" : "%s", year);
      const int tabW = r.getTextWidth(toybox::kTileFontId, head) + 16;
      r.fillRect(x, y, tabW, 22, true);
      r.drawText(toybox::kTileFontId, x + 8,
                 y + (22 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight),
                 head, false);
    } else {
      std::snprintf(head, sizeof(head),
                    i == 0 ? "Un d\xC3\xAD"
                             "a como hoy, %s"
                           : "%s",
                    year);
      if (look == EventLook::Centred) {
        textCentre(r, NOTOSANS_12_FONT_ID, x + width / 2, y, head, kBold);
      } else {
        text(r, NOTOSANS_12_FONT_ID, x, y, head, kBold);
      }
    }
    y += headH;
    std::vector<std::string> wrapped;
    wrapped.reserve(8);
    toyboxserif::wrap(r, bodyFont, page.events[i].text, width, lines, wrapped);
    for (size_t k = 0; k < wrapped.size(); ++k) {
      const int w = r.getTextWidth(bodyFont, wrapped[k].c_str());
      r.drawText(bodyFont, look == EventLook::Centred ? x + (width - w) / 2 : x, y, wrapped[k].c_str(), true);
      y += lineH;
    }
    y += pixel ? 10 : 6;
    drawn = true;
  }
  if (drawn) zone(screen, 0, top, screen.device().width, bottom - top, Event);
}

// "Nubes y claros  ·  olas 1,1 m  ·  agua 19°": the sky in words and the water.
void skyAndSea(const Page& page, char* b, const size_t cap, const bool sky, const bool wind) {
  int at = 0;
  b[0] = 0;
  auto add = [&](const char* piece) {
    if (at < static_cast<int>(cap) - 40) {
      at += std::snprintf(b + at, cap - static_cast<size_t>(at), "%s%s", at ? "  \xC2\xB7  " : "", piece);
    }
  };
  char piece[40];
  if (sky && page.data.hasForecast) add(skyPhrase(wx::skyOf(page.data.forecast.current.code)));
  if (const nv::Hour* h = seaNow(page)) {
    if (wind && h->wind >= 0) {
      std::snprintf(piece, sizeof(piece), "%s %d kn", nv::compass(h->windFrom),
                    static_cast<int>(std::lround(h->wind)) % 1000);
      add(piece);
    }
    if (h->wave >= 0) {
      const int tenths = static_cast<int>(std::lround(h->wave * 10)) % 1000;
      std::snprintf(piece, sizeof(piece), "olas %d,%d m", tenths / 10, tenths % 10);
      add(piece);
    }
    if (h->water > -50) {
      std::snprintf(piece, sizeof(piece), "agua %d\xC2\xB0", static_cast<int>(std::lround(h->water)) % 1000);
      add(piece);
    }
  }
}

// The sky in one word, for where a phrase does not fit.
const char* skyWord(const wx::Sky sky) {
  switch (sky) {
    case wx::Sky::Clear:
      return "despejado";
    case wx::Sky::PartCloud:
      return "claros";
    case wx::Sky::Cloud:
      return "nublado";
    case wx::Sky::Fog:
      return "niebla";
    case wx::Sky::Drizzle:
      return "llovizna";
    case wx::Sky::Rain:
      return "lluvia";
    case wx::Sky::Showers:
      return "chubascos";
    case wx::Sky::Snow:
      return "nieve";
    case wx::Sky::Thunder:
    default:
      return "tormenta";
  }
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

  rule(screen, 648);
  eventsBlock(screen, renderer, page, x, 658, inner, bottom, EventLook::Pixel);
  zone(screen, 0, 0, width, 150, Clock);
  zone(screen, 0, 150, width, 258, Weather);
  zone(screen, 0, 408, width, 150, Markets);
  zone(screen, 0, 558, width, 90, Sea);
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

  rule(screen, y);
  eventsBlock(screen, renderer, page, x, y + 10, inner, bottom, EventLook::Pixel);
  zone(screen, 0, 0, width, 204, Clock);
  zone(screen, 0, 204, width, 264, Weather);
  zone(screen, 0, 468, width, 112, Markets);
  zone(screen, 0, 580, width, 72, Sea);
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

  // Days like this one, as the paper's look back.
  eventsBlock(screen, r, page, x, y, inner, bottom, EventLook::Reader);
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

  // Days like this, small, at the foot.
  eventsBlock(screen, r, page, 40, y, width - 80, bottom, EventLook::Centred);
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

  r.fillRect(x, y, inner, 1, true);
  eventsBlock(screen, r, page, x, y + 6, inner, bottom, EventLook::Reader);
}

// --- Pieces the styles past the fifth share. ---

void disc(const GfxRenderer& r, const int cx, const int cy, const int rad, const Color color) {
  r.fillRoundedRect(cx - rad, cy - rad, rad * 2, rad * 2, rad, color);
}

// An arrowhead for a rise or a fall, `size` across.
void arrowhead(const GfxRenderer& r, const int cx, const int cy, const int size, const bool up) {
  const int xs[3] = {cx - size / 2, cx + size / 2, cx};
  const int ys[3] = {up ? cy + size / 3 : cy - size / 3, up ? cy + size / 3 : cy - size / 3,
                     up ? cy - size / 2 : cy + size / 2};
  r.fillPolygon(xs, ys, 3, true);
}

// The last `n` closes of an asset as a line across a box.
void spark(const GfxRenderer& r, const mk::Quote& q, const int x, const int y, const int w, const int h, int n,
           const int pen) {
  if (!q.valid || q.count < 2) return;
  if (n > q.count) n = q.count;
  const int first = q.count - n;
  float lo = q.closes[first];
  float hi = lo;
  for (int i = 1; i < n; ++i) {
    const float v = q.closes[first + i];
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const float span = hi - lo > 0 ? hi - lo : 1;
  int px = 0;
  int py = 0;
  for (int i = 0; i < n; ++i) {
    const int qx = x + w * i / (n - 1);
    const int qy = y + h - static_cast<int>(std::lround((q.closes[first + i] - lo) / span * static_cast<float>(h)));
    if (i > 0) r.drawLine(px, py, qx, qy, pen, true);
    px = qx;
    py = qy;
  }
  disc(r, px, py, pen + 2, Color::Black);
}

// A name in capitals, set down as a name: "S&P 500" stays, "BITCOIN" is "Bitcoin".
void lowerName(const char* name, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s", name);
  for (char* c = out + 1; *c != 0; ++c) {
    if (*c >= 'A' && *c <= 'Z' && c[-1] != ' ' && c[-1] != '&' && c[-1] != '/') *c = static_cast<char>(*c + 32);
  }
}

void upper(char* s) {
  for (; *s != 0; ++s) {
    if (*s >= 'a' && *s <= 'z') *s = static_cast<char>(*s - 32);
  }
}

// A price with its move under it, the sign as an arrowhead. Centred in
// `width` when that is not zero. Two lines of `font`, less four pixels.
void priceMove(const GfxRenderer& r, const int font, const int x, const int y, const char* price, const char* change,
               const double pct, const int width) {
  const char* amount = change[0] == '-' || change[0] == '+' ? change + 1 : change;
  const int lineH = r.getLineHeight(font);
  const int moveW = 16 + r.getTextWidth(font, amount, kBold);
  const int px = width > 0 ? x + (width - r.getTextWidth(font, price)) / 2 : x;
  const int mx = width > 0 ? x + (width - moveW) / 2 : x;
  text(r, font, px, y, price);
  arrowhead(r, mx + 6, y + lineH - 4 + lineH / 2 + 2, 12, pct >= 0);
  text(r, font, mx + 16, y + lineH - 4, amount, kBold);
}

// How far round its month the moon is, 0 at new and 0.5 at full.
float moonPhase(const Page& page) {
  struct tm copy = page.now;
  const time_t t = mktime(&copy);
  const double months = (static_cast<double>(t) - 947182440.0) / 2551442.8;
  return static_cast<float>(months - std::floor(months));
}

const char* moonName(const float p) {
  static constexpr const char* kNames[8] = {"luna nueva", "luna creciente",   "cuarto creciente", "gibosa creciente",
                                            "luna llena", "gibosa menguante", "cuarto menguante", "luna menguante"};
  return kNames[static_cast<int>(p * 8 + 0.5f) % 8];
}

// The moon as it stands tonight: a ring, with the part in shadow inked.
void moon(const GfxRenderer& r, const int cx, const int cy, const int rad, const float p) {
  const float c = std::cos(2 * kPi * p);
  for (int dy = -rad; dy <= rad; ++dy) {
    const float w = std::sqrt(static_cast<float>(rad * rad - dy * dy));
    const float t = w * c;
    // Waxing, the light grows from the right; waning, it leaves from the right.
    const float from = p < 0.5f ? -w : -t;
    const float to = p < 0.5f ? t : w;
    if (to > from) {
      r.fillRect(cx + static_cast<int>(std::lround(from)), cy + dy,
                 static_cast<int>(std::lround(to)) - static_cast<int>(std::lround(from)), 1, true);
    }
  }
  arc(r, cx, cy, rad, 0, 360, 2);
}

bool isDaytime(const Page& page) {
  if (page.data.hasForecast) return page.data.forecast.current.isDay;
  return !page.clockSet || (page.now.tm_hour >= 7 && page.now.tm_hour < 20);
}

// "9° a 17°", today's low and high.
void tempRange(const Page& page, char* out, const size_t cap) {
  out[0] = 0;
  if (!page.data.hasForecast || page.data.forecast.dayCount == 0) return;
  const wx::Day& day = page.data.forecast.days[0];
  std::snprintf(out, cap, "%d\xC2\xB0 a %d\xC2\xB0", wx::displayTemp(day.min, page.data.fahrenheit),
                wx::displayTemp(day.max, page.data.fahrenheit));
}

int rainToday(const Page& page) {
  return page.data.hasForecast && page.data.forecast.dayCount > 0 ? page.data.forecast.days[0].precip : 0;
}

// ---------------------------------------------------------------------------
// 6. CARTEL: a poster. The hour stacked on the minute in the heaviest pen the
// page can carry, and everything else in one narrow column beside them.
// ---------------------------------------------------------------------------
void poster(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = 18;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  char a[48];
  char b[120];

  // The numerals: two rows of two.
  const float boxW = 128;
  const float pen = 22;
  const int boxH = static_cast<int>(boxW * clockcore::kNumeralHeight);
  const int top = 20;
  const int digits[4] = {page.now.tm_hour / 10, page.now.tm_hour % 10, page.now.tm_min / 10, page.now.tm_min % 10};
  for (int i = 0; i < 4; ++i) {
    const float nx = static_cast<float>(x + (i % 2) * (static_cast<int>(boxW) + 14));
    const float ny = static_cast<float>(top + (i / 2) * (boxH + 20));
    if (page.clockSet) {
      clockfaces::drawNumeral(r, digits[i], nx, ny, boxW, pen);
    } else {
      r.fillRect(static_cast<int>(nx) + 20, static_cast<int>(ny) + boxH / 2 - 10, static_cast<int>(boxW) - 40, 20,
                 true);
    }
  }
  const int numeralsBottom = top + boxH * 2 + 20;
  const int colX = x + static_cast<int>(boxW) * 2 + 14 + 22;
  const int colW = x + inner - colX;
  zone(screen, 0, 0, colX - 10, numeralsBottom + 8, Clock);

  // The column: a black block to hang it from, then the day and the weather.
  int y = top;
  r.fillRect(colX, y, colW, 12, true);
  y += 24;
  if (page.clockSet) {
    std::snprintf(a, sizeof(a), "%s", kWeekdays[page.now.tm_wday % 7]);
    upper(a);
    // Accented capitals are not in every face: the weekday's own letters are kept when it has any.
    text(r, NOTOSANS_14_FONT_ID, colX, y,
         page.now.tm_wday == 3 || page.now.tm_wday == 6 ? kWeekdays[page.now.tm_wday] : a, kBold);
    y += r.getLineHeight(NOTOSANS_14_FONT_ID);
    std::snprintf(a, sizeof(a), "%d %.3s", page.now.tm_mday, kMonths[page.now.tm_mon % 12]);
    text(r, NOTOSANS_18_FONT_ID, colX, y, a, kBold);
    y += r.getLineHeight(NOTOSANS_18_FONT_ID) + 10;
  }
  r.fillRect(colX, y, colW, 3, true);
  y += 14;
  const int weatherTop = y;
  if (d.hasForecast) {
    const wx::Sky sky = wx::skyOf(d.forecast.current.code);
    weatherui::skyIcon(screen, sky, isDaytime(page), colX + 40, y + 36, 80);
    y += 84;
    std::snprintf(a, sizeof(a), "%d\xC2\xB0", wx::displayTemp(d.forecast.current.temp, d.fahrenheit));
    // The temperature in the clock's own numerals, to stand up to the hour.
    const int temp = wx::displayTemp(d.forecast.current.temp, d.fahrenheit);
    const int shown = temp < 0 ? -temp : temp;
    float nx = static_cast<float>(colX);
    if (temp < 0) {
      r.fillRect(colX, y + 36, 18, 8, true);
      nx += 26;
    }
    const float tb = 44;
    if (shown >= 10) {
      clockfaces::drawNumeral(r, (shown / 10) % 10, nx, static_cast<float>(y), tb, 8);
      nx += tb + 6;
    }
    clockfaces::drawNumeral(r, shown % 10, nx, static_cast<float>(y), tb, 8);
    nx += tb + 6;
    disc(r, static_cast<int>(nx) + 8, y + 10, 9, Color::Black);
    disc(r, static_cast<int>(nx) + 8, y + 10, 4, Color::White);
    y += static_cast<int>(tb * clockcore::kNumeralHeight) + 10;
    text(r, NOTOSANS_12_FONT_ID, colX, y, skyWord(sky), kBold);
    y += r.getLineHeight(NOTOSANS_12_FONT_ID);
    tempRange(page, a, sizeof(a));
    text(r, NOTOSANS_12_FONT_ID, colX, y, a);
    y += r.getLineHeight(NOTOSANS_12_FONT_ID);
    std::snprintf(a, sizeof(a), "lluvia %d %%", rainToday(page) % 1000);
    text(r, NOTOSANS_12_FONT_ID, colX, y, a);
  } else {
    paragraph(r, NOTOSANS_12_FONT_ID, colX, y, colW, "sin previsi\xC3\xB3n", 2, false);
  }
  zone(screen, colX - 10, weatherTop, width - colX + 10, numeralsBottom + 8 - weatherTop, Weather);

  // Markets: three columns, the change set large.
  y = numeralsBottom + 16;
  const int marketsTop = y;
  r.fillRect(x, y, inner, 6, true);
  y += 16;
  const int cellW = inner / 3;
  for (int i = 0; i < mk::kAssets; ++i) {
    const int cxl = x + cellW * i;
    char price[24];
    char change[16];
    double pct = 0;
    text(r, NOTOSANS_12_FONT_ID, cxl, y, mk::infoOf(static_cast<mk::Asset>(i)).name, kBold);
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      const int ly = y + r.getLineHeight(NOTOSANS_12_FONT_ID);
      arrowhead(r, cxl + 9, ly + r.getLineHeight(NOTOSANS_16_FONT_ID) / 2 + 2, 18, pct >= 0);
      text(r, NOTOSANS_16_FONT_ID, cxl + 24, ly, change[0] == '-' || change[0] == '+' ? change + 1 : change, kBold);
      text(r, NOTOSANS_12_FONT_ID, cxl, ly + r.getLineHeight(NOTOSANS_16_FONT_ID), price);
    }
    if (i > 0) r.fillRect(cxl - 10, y + 4, 1, 86, true);
  }
  y += r.getLineHeight(NOTOSANS_12_FONT_ID) * 2 + r.getLineHeight(NOTOSANS_16_FONT_ID) + 6;
  zone(screen, 0, marketsTop, width, y - marketsTop, Markets);

  // The sea on one line between rules.
  skyAndSea(page, b, sizeof(b), false, true);
  if (b[0] != 0) {
    r.fillRect(x, y, inner, 3, true);
    text(r, NOTOSANS_12_FONT_ID, x, y + 7, b, kBold);
    zone(screen, 0, y, width, r.getLineHeight(NOTOSANS_12_FONT_ID) + 12, Sea);
    y += r.getLineHeight(NOTOSANS_12_FONT_ID) + 12;
  }
  r.fillRect(x, y, inner, 3, true);
  eventsBlock(screen, r, page, x, y + 8, inner, bottom, EventLook::Reader);
}

// ---------------------------------------------------------------------------
// 7. HORIZONTE: the day as a landscape. The sun or the moon stands where the
// hour puts it, the sky is the weather, the three ridges are the three markets
// over the last weeks, and the sea runs as high as the sea does.
// ---------------------------------------------------------------------------
void horizon(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = 20;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  const int horizonY = 372;
  const int seaBottom = 470;
  char a[48];
  char b[120];
  const wx::Sky sky = d.hasForecast ? wx::skyOf(d.forecast.current.code) : wx::Sky::Clear;
  const bool day = isDaytime(page);

  // The path across the sky, dotted, and the sun or the moon on it.
  const int hour = page.clockSet ? page.now.tm_hour * 60 + page.now.tm_min : 12 * 60;
  // Day runs 7 to 20, night the rest; either way left to right.
  float part = day ? static_cast<float>(hour - 7 * 60) / (13.0f * 60)
                   : static_cast<float>((hour + 4 * 60) % 1440) / (11.0f * 60);
  if (part < 0.03f) part = 0.03f;
  if (part > 0.97f) part = 0.97f;
  const int pathR = 196;
  const int pathBase = 264;
  const int pathRise = 62;
  const int pcx = width / 2;
  for (int k = 0; k <= 30; ++k) {
    const float ang = kPi * static_cast<float>(k) / 30.0f;
    r.fillRect(pcx - static_cast<int>(std::lround(pathR * std::cos(ang))),
               pathBase - static_cast<int>(std::lround(pathRise * std::sin(ang))), 2, 2, true);
  }
  const float ang = kPi * part;
  const int sx = pcx - static_cast<int>(std::lround(pathR * std::cos(ang)));
  const int sy = pathBase - static_cast<int>(std::lround(pathRise * std::sin(ang)));
  if (day) {
    disc(r, sx, sy, 40, Color::White);
    for (int k = 0; k < 16; ++k) spoke(r, sx, sy, k * 22.5f, 32, k % 2 == 0 ? 46 : 40, 3);
    disc(r, sx, sy, 24, Color::Black);
    disc(r, sx, sy, 20, Color::White);
    r.fillRectDither(sx - 12, sy - 12, 24, 24, Color::LightGray);
  } else {
    disc(r, sx, sy, 30, Color::White);
    moon(r, sx, sy, 24, page.clockSet ? moonPhase(page) : 0.25f);
  }

  // The hour and the day, top left; the weather in words, top right.
  const float boxW = 50;
  numerals(r, page, static_cast<float>(x), 24, boxW, 5);
  int ty = 24 + static_cast<int>(boxW * clockcore::kNumeralHeight) + 12;
  if (page.clockSet) {
    std::snprintf(b, sizeof(b), "%s %d de %s", kWeekdays[page.now.tm_wday % 7], page.now.tm_mday,
                  kMonths[page.now.tm_mon % 12]);
    text(r, NOTOSANS_12_FONT_ID, x, ty, b, kBold);
  }
  zone(screen, 0, 0, 290, ty + 34, Clock);
  if (d.hasForecast) {
    std::snprintf(a, sizeof(a), "%d\xC2\xB0", wx::displayTemp(d.forecast.current.temp, d.fahrenheit));
    textRight(r, NOTOSANS_18_FONT_ID, x + inner, 20, a, kBold);
    int wy = 20 + r.getLineHeight(NOTOSANS_18_FONT_ID);
    textRight(r, NOTOSANS_12_FONT_ID, x + inner, wy, skyPhrase(sky), kBold);
    wy += r.getLineHeight(NOTOSANS_12_FONT_ID);
    tempRange(page, a, sizeof(a));
    textRight(r, NOTOSANS_12_FONT_ID, x + inner, wy, a);
    wy += r.getLineHeight(NOTOSANS_12_FONT_ID);
    std::snprintf(a, sizeof(a), "lluvia %d %%", rainToday(page) % 1000);
    textRight(r, NOTOSANS_12_FONT_ID, x + inner, wy, a);
  }

  // The weather itself: clouds for cloud, strokes for rain, dots for snow, bars for fog.
  auto cloud = [&](const int cx, const int cy, const int s) {
    const int lumps[4][3] = {
        {-s, 0, s * 6 / 10}, {0, -s * 4 / 10, s * 8 / 10}, {s, 0, s * 6 / 10}, {0, s * 2 / 10, s * 6 / 10}};
    for (const auto& l : lumps) disc(r, cx + l[0], cy + l[1], l[2] + 3, Color::Black);
    r.fillRect(cx - s, cy + s * 2 / 10, s * 2, s * 4 / 10 + 3, true);
    for (const auto& l : lumps) disc(r, cx + l[0], cy + l[1], l[2], Color::White);
    r.fillRect(cx - s, cy + s * 2 / 10, s * 2, s * 4 / 10, false);
  };
  const bool wet =
      sky == wx::Sky::Drizzle || sky == wx::Sky::Rain || sky == wx::Sky::Showers || sky == wx::Sky::Thunder;
  const int clouds = sky == wx::Sky::Clear || sky == wx::Sky::Fog ? 0 : (sky == wx::Sky::PartCloud ? 2 : 3);
  const int cloudAt[3][3] = {{120, 226, 30}, {388, 234, 26}, {300, 206, 22}};
  if (wet || sky == wx::Sky::Snow) {
    for (int k = 0; k < 26; ++k) {
      const int rx = 60 + (k * 97) % 380;
      const int ry = 262 + (k * 53) % 90;
      if (sky == wx::Sky::Snow) {
        disc(r, rx, ry, 3, Color::Black);
      } else {
        r.drawLine(rx, ry, rx - 7, ry + 18, 2, true);
      }
    }
  }
  for (int k = 0; k < clouds; ++k) cloud(cloudAt[k][0], cloudAt[k][1], cloudAt[k][2]);
  if (sky == wx::Sky::Fog) {
    for (int k = 0; k < 5; ++k) r.fillRectDither(40 + (k % 2) * 60, 230 + k * 22, 340, 6, Color::DarkGray);
  }
  zone(screen, 290, 0, width - 290, 170, Weather);
  zone(screen, 0, 170, width, 80, Weather);

  // The ridges: the furthest an outline, the middle one pale, the nearest dark.
  const int ridgeBase[3] = {horizonY, horizonY, horizonY};
  const int ridgeMax[3] = {58, 82, 106};
  const int order[3] = {0, 1, 2};  // nearest first in the data, drawn furthest first below
  for (int layer = 2; layer >= 0; --layer) {
    const mk::Quote& q = d.quotes[order[layer]];
    if (!q.valid || q.count < 2) continue;
    const int n = q.count < 60 ? q.count : 60;
    const int first = q.count - n;
    float lo = q.closes[first];
    float hi = lo;
    for (int i = 1; i < n; ++i) {
      const float v = q.closes[first + i];
      if (v < lo) lo = v;
      if (v > hi) hi = v;
    }
    const float span = hi - lo > 0 ? hi - lo : 1;
    int prevTop = 0;
    for (int px = 0; px < width; px += 2) {
      const float at = static_cast<float>(px) * static_cast<float>(n - 1) / static_cast<float>(width - 2);
      const int i = static_cast<int>(at);
      const float frac = at - static_cast<float>(i);
      const float v = q.closes[first + i] + (i + 1 < n ? (q.closes[first + i + 1] - q.closes[first + i]) * frac : 0);
      const int h = 24 + static_cast<int>(std::lround((v - lo) / span * static_cast<float>(ridgeMax[2 - layer] - 24)));
      const int topY = ridgeBase[layer] - h;
      r.fillRect(px, topY, 2, h, false);
      if (layer == 1) r.fillRectDither(px, topY, 2, h, Color::LightGray);
      if (layer == 0) r.fillRectDither(px, topY, 2, h, Color::DarkGray);
      // The crest, inked: a step between columns is drawn as its riser.
      if (px > 0) {
        const int from = prevTop < topY ? prevTop : topY;
        const int span2 = prevTop < topY ? topY - prevTop : prevTop - topY;
        r.fillRect(px, from, 2, span2 + 2, true);
      } else {
        r.fillRect(px, topY, 2, 2, true);
      }
      prevTop = topY;
    }
  }
  zone(screen, 0, 250, width, horizonY - 250, Markets);

  // The sea: lines of swell, taller for taller waves and longer as they near.
  r.fillRect(0, horizonY, width, 3, true);
  const nv::Hour* h = seaNow(page);
  float amp = h != nullptr && h->wave >= 0 ? 1.5f + h->wave * 3.0f : 2.5f;
  if (amp > 9) amp = 9;
  for (int row = 0; row < 7; ++row) {
    const int wy = horizonY + 12 + row * 13;
    const float length = 26.0f + row * 9.0f;
    const float a2 = amp * (0.5f + row * 0.1f);
    int px0 = 0;
    int py0 = wy;
    for (int px = 0; px <= width; px += 6) {
      const int py =
          wy + static_cast<int>(std::lround(a2 * std::sin(2 * kPi * (static_cast<float>(px) / length) + row * 1.3f)));
      if (px > 0) r.drawLine(px0, py0, px, py, row > 3 ? 2 : 1, true);
      px0 = px;
      py0 = py;
    }
  }
  zone(screen, 0, horizonY, width, seaBottom - horizonY, Sea);

  // Under the picture, what it shows: the ridges by name, then the sea.
  int y = seaBottom;
  r.fillRect(0, y, width, 3, true);
  y += 10;
  const int cellW = inner / 3;
  for (int i = 0; i < mk::kAssets; ++i) {
    const int cxl = x + cellW * i;
    char price[24];
    char change[16];
    double pct = 0;
    // The swatch says which ridge.
    r.drawRect(cxl, y + 6, 14, 14, 2, true);
    if (i == 0) r.fillRectDither(cxl + 2, y + 8, 10, 10, Color::DarkGray);
    if (i == 1) r.fillRectDither(cxl + 2, y + 8, 10, 10, Color::LightGray);
    lowerName(mk::infoOf(static_cast<mk::Asset>(i)).name, a, sizeof(a));
    text(r, NOTOSANS_12_FONT_ID, cxl + 20, y, a, kBold);
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      priceMove(r, NOTOSANS_12_FONT_ID, cxl + 20, y + r.getLineHeight(NOTOSANS_12_FONT_ID) - 4, price, change, pct, 0);
    }
  }
  zone(screen, 0, seaBottom, width, r.getLineHeight(NOTOSANS_12_FONT_ID) * 3 + 8, Markets);
  y += r.getLineHeight(NOTOSANS_12_FONT_ID) * 3 - 2;
  skyAndSea(page, b, sizeof(b), false, true);
  if (b[0] != 0) {
    r.fillRect(x, y, inner, 1, true);
    textCentre(r, NOTOSANS_12_FONT_ID, width / 2, y + 4, b);
    zone(screen, 0, y, width, r.getLineHeight(NOTOSANS_12_FONT_ID) + 8, Sea);
    y += r.getLineHeight(NOTOSANS_12_FONT_ID) + 8;
  }
  r.fillRect(x, y, inner, 1, true);
  eventsBlock(screen, r, page, x, y + 6, inner, bottom, EventLook::Reader);
}

// ---------------------------------------------------------------------------
// 8. MOSAICO: tiles, each its own size for what it holds. The rain tile fills
// like a glass, the markets carry their last weeks as a line.
// ---------------------------------------------------------------------------
void mosaic(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int m = 12;
  const int gap = 10;
  const int inner = width - m * 2;
  const int bottom = contentBottom(screen);
  const int labelH = r.getLineHeight(NOTOSANS_12_FONT_ID);
  char a[48];
  char b[120];
  auto tile = [&](const int tx, const int ty, const int tw, const int th) {
    r.drawRoundedRect(tx, ty, tw, th, 3, 18, true);
  };
  auto tag = [&](const int tx, const int ty, const char* s) { r.drawText(toybox::kTileFontId, tx, ty, s, true); };

  // The hour, across the top.
  int y = m;
  const int timeH = 196;
  tile(m, y, inner, timeH);
  const float boxW = 78;
  const int timeW = numerals(r, page, 0, 0, boxW, 11, false);
  numerals(r, page, static_cast<float>((width - timeW) / 2), static_cast<float>(y + 16), boxW, 11);
  if (page.clockSet) {
    std::snprintf(b, sizeof(b), "%s %d de %s", kWeekdays[page.now.tm_wday % 7], page.now.tm_mday,
                  kMonths[page.now.tm_mon % 12]);
    textCentre(r, NOTOSANS_14_FONT_ID, width / 2, y + 16 + static_cast<int>(boxW * clockcore::kNumeralHeight) + 10, b,
               kBold);
  }
  zone(screen, 0, 0, width, y + timeH + gap / 2, Clock);
  y += timeH + gap;

  // The weather, wide, and the rain beside it as a glass.
  const int rowH = 148;
  const int rainW = 136;
  const int wxW = inner - rainW - gap;
  tile(m, y, wxW, rowH);
  tag(m + 16, y + 14, "TIEMPO");
  if (d.hasForecast) {
    const wx::Sky sky = wx::skyOf(d.forecast.current.code);
    weatherui::skyIcon(screen, sky, isDaytime(page), m + 62, y + 84, 88);
    const int temp = wx::displayTemp(d.forecast.current.temp, d.fahrenheit);
    const int shown = temp < 0 ? -temp : temp;
    float nx = static_cast<float>(m + 124);
    const float tb = 40;
    const int ny = y + 32;
    if (temp < 0) {
      r.fillRect(static_cast<int>(nx), ny + 30, 16, 7, true);
      nx += 22;
    }
    if (shown >= 10) {
      clockfaces::drawNumeral(r, (shown / 10) % 10, nx, static_cast<float>(ny), tb, 7);
      nx += tb + 6;
    }
    clockfaces::drawNumeral(r, shown % 10, nx, static_cast<float>(ny), tb, 7);
    nx += tb + 6;
    disc(r, static_cast<int>(nx) + 8, ny + 9, 8, Color::Black);
    disc(r, static_cast<int>(nx) + 8, ny + 9, 4, Color::White);
    text(r, NOTOSANS_12_FONT_ID, m + 124, ny + 68, skyWord(sky), kBold);
    tempRange(page, a, sizeof(a));
    textRight(r, NOTOSANS_12_FONT_ID, m + wxW - 14, ny + 68, a);
  }
  zone(screen, 0, y - gap / 2, m + wxW + gap / 2, rowH + gap, Weather);
  const int rx = m + wxW + gap;
  {
    const int rain = rainToday(page);
    // The glass: grey up to the chance of rain, inside the tile's own outline.
    const int level = (rowH - 12) * rain / 100;
    if (level > 0) {
      r.fillRectDither(rx + 6, y + rowH - 6 - level, rainW - 12, level, Color::LightGray);
      r.fillRect(rx + 6, y + rowH - 6 - level, rainW - 12, 2, true);
    }
    // The corners the grey squared off are given back to the page.
    r.fillRect(rx, y + rowH - 20, 20, 20, false);
    r.fillRect(rx + rainW - 20, y + rowH - 20, 20, 20, false);
    if (level > 14) {
      r.fillRoundedRect(rx + 5, y + rowH - 6 - (level < 30 ? level : 30), rainW - 10, level < 30 ? level : 30, 14,
                        false, false, true, true, Color::LightGray);
    }
    tile(rx, y, rainW, rowH);
    const int bw = r.getTextWidth(NOTOSANS_18_FONT_ID, "100%", kBold) + 12;
    r.fillRoundedRect(rx + (rainW - bw) / 2, y + 44, bw, r.getLineHeight(NOTOSANS_18_FONT_ID) + 4, 10, Color::White);
    r.fillRoundedRect(rx + 12, y + 10, 76, 22, 6, Color::White);
    tag(rx + 16, y + 14, "LLUVIA");
    std::snprintf(a, sizeof(a), "%d%%", rain % 1000);
    textCentre(r, NOTOSANS_18_FONT_ID, rx + rainW / 2, y + 46, a, kBold);
  }
  zone(screen, rx - gap / 2, y - gap / 2, width - rx + gap / 2, rowH + gap, Weather);
  y += rowH + gap;

  // The markets, three alike.
  const int mkH = 150;
  const int mkW = (inner - gap * 2) / 3;
  for (int i = 0; i < mk::kAssets; ++i) {
    const int tx = m + (mkW + gap) * i;
    tile(tx, y, mkW, mkH);
    char price[24];
    char change[16];
    double pct = 0;
    tag(tx + 12, y + 14, mk::infoOf(static_cast<mk::Asset>(i)).name);
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      text(r, NOTOSANS_14_FONT_ID, tx + 12, y + 30, price, kBold);
      const int cy2 = y + 30 + r.getLineHeight(NOTOSANS_14_FONT_ID);
      arrowhead(r, tx + 19, cy2 + labelH / 2 + 2, 14, pct >= 0);
      text(r, NOTOSANS_12_FONT_ID, tx + 32, cy2, change[0] == '-' || change[0] == '+' ? change + 1 : change, kBold);
      spark(r, d.quotes[i], tx + 14, cy2 + labelH + 4, mkW - 32, y + mkH - 14 - (cy2 + labelH + 4), 30, 2);
    }
  }
  zone(screen, 0, y - gap / 2, width, mkH + gap, Markets);
  y += mkH + gap;

  // The sea: three figures in one long tile.
  if (const nv::Hour* h = seaNow(page)) {
    const int seaH = 78;
    tile(m, y, inner, seaH);
    const int at[3] = {0, (inner - 32) * 48 / 100, (inner - 32) * 76 / 100};
    const char* tags[3] = {"VIENTO", "OLAS", "AGUA"};
    for (int i = 0; i < 3; ++i) {
      const int tx = m + 16 + at[i];
      tag(tx, y + 14, tags[i]);
      a[0] = 0;
      if (i == 0 && h->wind >= 0) {
        std::snprintf(a, sizeof(a), "%d kn %s", static_cast<int>(std::lround(h->wind)) % 1000,
                      nv::compass(h->windFrom));
      }
      if (i == 1 && h->wave >= 0) {
        const int tenths = static_cast<int>(std::lround(h->wave * 10)) % 1000;
        std::snprintf(a, sizeof(a), "%d,%d m", tenths / 10, tenths % 10);
      }
      if (i == 2 && h->water > -50) {
        std::snprintf(a, sizeof(a), "%d\xC2\xB0", static_cast<int>(std::lround(h->water)) % 1000);
      }
      text(r, NOTOSANS_14_FONT_ID, tx, y + 32, a[0] != 0 ? a : "--", kBold);
      if (i > 0) r.fillRect(tx - 10, y + 14, 1, seaH - 28, true);
    }
    zone(screen, 0, y - gap / 2, width, seaH + gap, Sea);
    y += seaH + gap;
  }

  // What is left of the page, to days like this one.
  if (page.eventCount > 0 && bottom - y > labelH * 3) {
    tile(m, y, inner, bottom - y - 2);
    eventsBlock(screen, r, page, m + 16, y + 10, inner - 32, bottom - 10, EventLook::Reader);
  }
}

// ---------------------------------------------------------------------------
// 9. BILLETE: the day as a ticket for it. Fields with their small labels,
// perforations between the parts, and a bar code made of the day's own figures.
// ---------------------------------------------------------------------------
void ticket(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int m = 10;
  const int x = 30;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  const int valueH = r.getLineHeight(NOTOSANS_16_FONT_ID);
  char a[48];
  char b[120];
  r.drawRoundedRect(m, 8, width - m * 2, bottom - 12, 3, 22, true);
  auto field = [&](const int fx, const int fy, const char* label, const char* value,
                   const int font = NOTOSANS_16_FONT_ID) {
    r.drawText(toybox::kTileFontId, fx, fy, label, true);
    text(r, font, fx, fy + 16, value, kBold);
  };
  // A tear line: dashes across, and a bite out of each edge.
  auto perforation = [&](const int py) {
    for (int px = m + 22; px < width - m - 22; px += 12) r.fillRect(px, py, 6, 2, true);
    disc(r, m + 1, py + 1, 14, Color::Black);
    disc(r, m - 2, py + 1, 14, Color::White);
    r.fillRect(0, py - 16, m, 34, false);
    disc(r, width - m - 1, py + 1, 14, Color::Black);
    disc(r, width - m + 2, py + 1, 14, Color::White);
    r.fillRect(width - m + 1, py - 16, m, 34, false);
  };

  // The head: what this is, and its bar code.
  int y = 26;
  text(r, NOTOSANS_18_FONT_ID, x, y - 4, "HOY", kBold);
  r.drawText(toybox::kTileFontId, x + r.getTextWidth(NOTOSANS_18_FONT_ID, "HOY", kBold) + 12, y + 4, "PASE DE UN DIA",
             true);
  r.drawText(toybox::kTileFontId, x + r.getTextWidth(NOTOSANS_18_FONT_ID, "HOY", kBold) + 12, y + 20, "VALIDO HOY",
             true);
  {
    // The bars: widths from the digits of the hour, the date and the prices.
    uint32_t seed =
        static_cast<uint32_t>(page.now.tm_yday * 1440 + page.now.tm_hour * 60 + page.now.tm_min) * 2654435761u;
    for (int i = 0; i < mk::kAssets; ++i) {
      if (d.quotes[i].valid) seed ^= static_cast<uint32_t>(d.quotes[i].price * 100) * 40503u;
    }
    int bx = x + inner - 150;
    while (bx < x + inner) {
      seed = seed * 1664525u + 1013904223u;
      const int w = 1 + static_cast<int>((seed >> 28) & 3);
      const int w2 = bx + w > x + inner ? x + inner - bx : w;
      r.fillRect(bx, y - 2, w2, 40, true);
      bx += w + 1 + static_cast<int>((seed >> 24) & 3);
    }
  }
  y += 52;
  r.fillRect(x, y, inner, 3, true);
  y += 14;

  // The hour, large, with the date and the place beside it.
  const int clockTop = y;
  r.drawText(toybox::kTileFontId, x, y, "HORA", true);
  const float boxW = 46;
  const int timeW = numerals(r, page, static_cast<float>(x), static_cast<float>(y + 20), boxW, 7);
  const int sideX = x + timeW + 22;
  if (page.clockSet) {
    std::snprintf(a, sizeof(a), "%.3s %d %.3s", kWeekdays[page.now.tm_wday % 7], page.now.tm_mday,
                  kMonths[page.now.tm_mon % 12]);
    field(sideX, y, "FECHA", a, NOTOSANS_14_FONT_ID);
  }
  if (d.hasPlace) {
    lowerName(d.place.name, a, sizeof(a));
    a[13] = 0;
    field(sideX, y + 16 + r.getLineHeight(NOTOSANS_14_FONT_ID) + 4, "LUGAR", a, NOTOSANS_14_FONT_ID);
  }
  y += 20 + static_cast<int>(boxW * clockcore::kNumeralHeight) + 18;
  zone(screen, 0, 0, width, y, Clock);
  (void)clockTop;
  perforation(y);
  y += 18;

  // The weather: four fields in a row, and tomorrow's under them.
  const int weatherTop = y;
  const int cw = inner / 4;
  if (d.hasForecast) {
    const wx::Sky sky = wx::skyOf(d.forecast.current.code);
    weatherui::skyIcon(screen, sky, isDaytime(page), x + 30, y + 30, 64);
    std::snprintf(a, sizeof(a), "%d\xC2\xB0", wx::displayTemp(d.forecast.current.temp, d.fahrenheit));
    field(x + cw, y, "AHORA", a);
    tempRange(page, a, sizeof(a));
    field(x + cw * 2 - 10, y, "MIN A MAX", a);
    std::snprintf(a, sizeof(a), "%d%%", rainToday(page) % 1000);
    field(x + cw * 3 + 24, y, "LLUVIA", a);
    y += 16 + valueH + 8;
    std::snprintf(b, sizeof(b), "%s.", skyPhrase(sky));
    int at = static_cast<int>(std::strlen(b));
    if (d.forecast.dayCount > 1) {
      const wx::Day& next = d.forecast.days[1];
      std::snprintf(b + at, sizeof(b) - static_cast<size_t>(at),
                    " Ma\xC3\xB1"
                    "ana %d\xC2\xB0/%d\xC2\xB0, lluvia %d %%.",
                    wx::displayTemp(next.max, d.fahrenheit), wx::displayTemp(next.min, d.fahrenheit),
                    static_cast<int>(next.precip) % 1000);
    }
    y += paragraph(r, NOTOSANS_12_FONT_ID, x, y + 4, inner, b, 2, false) + 10;
  } else {
    field(x, y, "TIEMPO", "sin previsi\xC3\xB3n");
    y += 16 + valueH + 8;
  }
  zone(screen, 0, weatherTop - 8, width, y - weatherTop + 8, Weather);

  // The sea, when the place has one.
  if (const nv::Hour* h = seaNow(page)) {
    const int seaTop = y;
    r.fillRect(x, y, inner, 1, true);
    y += 10;
    const int sw = inner / 3;
    a[0] = 0;
    if (h->wind >= 0)
      std::snprintf(a, sizeof(a), "%d kn %s", static_cast<int>(std::lround(h->wind)) % 1000, nv::compass(h->windFrom));
    field(x, y, "VIENTO", a[0] != 0 ? a : "--");
    a[0] = 0;
    if (h->wave >= 0) {
      const int tenths = static_cast<int>(std::lround(h->wave * 10)) % 1000;
      std::snprintf(a, sizeof(a), "%d,%d m", tenths / 10, tenths % 10);
    }
    field(x + sw + 54, y, "OLAS", a[0] != 0 ? a : "--");
    a[0] = 0;
    if (h->water > -50) std::snprintf(a, sizeof(a), "%d\xC2\xB0", static_cast<int>(std::lround(h->water)) % 1000);
    field(x + sw * 2 + 46, y, "AGUA", a[0] != 0 ? a : "--");
    y += 16 + valueH + 10;
    zone(screen, 0, seaTop, width, y - seaTop, Sea);
  }
  perforation(y);
  y += 18;

  // The stub: the three markets.
  const int marketsTop = y;
  const int mw = inner / 3;
  for (int i = 0; i < mk::kAssets; ++i) {
    const int fx = x + mw * i + (i > 0 ? 8 : 0);
    char price[24];
    char change[16];
    double pct = 0;
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      field(fx, y, mk::infoOf(static_cast<mk::Asset>(i)).name, price);
      arrowhead(r, fx + 7, y + 16 + valueH + r.getLineHeight(NOTOSANS_12_FONT_ID) / 2, 14, pct >= 0);
      text(r, NOTOSANS_12_FONT_ID, fx + 20, y + 16 + valueH - 2,
           change[0] == '-' || change[0] == '+' ? change + 1 : change, kBold);
    } else {
      field(fx, y, mk::infoOf(static_cast<mk::Asset>(i)).name, "--");
    }
  }
  y += 16 + valueH + r.getLineHeight(NOTOSANS_12_FONT_ID) + 6;
  zone(screen, 0, marketsTop - 8, width, y - marketsTop + 8, Markets);
  if (page.eventCount > 0) {
    perforation(y);
    eventsBlock(screen, r, page, x, y + 16, inner, bottom - 14, EventLook::Reader);
  }
}

// ---------------------------------------------------------------------------
// 10. ALMANAQUE: a leaf of a tear-off calendar. The day of the month as large
// as the leaf allows, tonight's moon, and the day's notes in the serif.
// ---------------------------------------------------------------------------
void almanac(toybox::Screen& screen, GfxRenderer& r, const Page& page) {
  const todaystore::Snapshot& d = page.data;
  const int width = screen.device().width;
  const int x = 24;
  const int inner = width - x * 2;
  const int bottom = contentBottom(screen);
  const int cx = width / 2;
  const int lineH = r.getLineHeight(NOTOSERIF_12_FONT_ID);
  char a[48];
  char b[120];

  // The binding: a grey strip with the two holes the leaf hangs by.
  r.fillRectDither(0, 0, width, 30, Color::DarkGray);
  r.fillRect(0, 30, width, 3, true);
  for (const int hx : {cx - 110, cx + 110}) {
    disc(r, hx, 15, 9, Color::Black);
    disc(r, hx, 15, 6, Color::White);
  }

  int y = 42;
  if (page.clockSet) {
    // The month, letter-spaced, with the year and the day of the year at its sides.
    std::snprintf(a, sizeof(a), "%s", kMonths[page.now.tm_mon % 12]);
    upper(a);
    textCentre(r, NOTOSERIF_18_FONT_ID, cx, y, a, kBold);
    std::snprintf(a, sizeof(a), "%d", (page.now.tm_year + 1900) % 10000);
    text(r, NOTOSANS_12_FONT_ID, x, y + 10, a, kBold);
    std::snprintf(a, sizeof(a),
                  "d\xC3\xAD"
                  "a %d",
                  (page.now.tm_yday + 1) % 1000);
    textRight(r, NOTOSANS_12_FONT_ID, x + inner, y + 10, a, kBold);
  }
  y += r.getLineHeight(NOTOSERIF_18_FONT_ID) + 6;
  r.fillRect(x, y, inner, 3, true);
  r.fillRect(x, y + 6, inner, 1, true);
  y += 22;

  // The day.
  const float boxW = 118;
  const float pen = 22;
  const int boxH = static_cast<int>(boxW * clockcore::kNumeralHeight);
  const int day = page.clockSet ? page.now.tm_mday : 0;
  if (page.clockSet) {
    const float total = day >= 10 ? boxW * 2 + 26 : boxW;
    float nx = static_cast<float>(cx) - total / 2;
    if (day >= 10) {
      clockfaces::drawNumeral(r, day / 10, nx, static_cast<float>(y), boxW, pen);
      nx += boxW + 26;
    }
    clockfaces::drawNumeral(r, day % 10, nx, static_cast<float>(y), boxW, pen);
  }
  y += boxH + 14;
  if (page.clockSet) {
    std::snprintf(b, sizeof(b), "%s  \xC2\xB7  %02d:%02d", kWeekdays[page.now.tm_wday % 7], page.now.tm_hour % 100,
                  page.now.tm_min % 100);
    textCentre(r, NOTOSERIF_18_FONT_ID, cx, y, b, kBold);
  }
  y += r.getLineHeight(NOTOSERIF_18_FONT_ID) + 8;
  zone(screen, 0, 0, width, y, Clock);

  // A rule with a lozenge at its middle.
  r.fillRect(x, y, inner, 1, true);
  r.fillRect(cx - 14, y - 2, 28, 5, false);
  const int lx[4] = {cx - 8, cx, cx + 8, cx};
  const int ly[4] = {y, y - 7, y, y + 7};
  r.fillPolygon(lx, ly, 4, true);
  y += 14;

  // Two columns of notes: the moon and the year to the left, the weather to the right.
  const int notesTop = y;
  if (page.clockSet) {
    const float p = moonPhase(page);
    moon(r, x + 22, y + lineH, 20, p);
    text(r, NOTOSERIF_12_FONT_ID, x + 52, y, moonName(p), kBold);
    const int lit = static_cast<int>(std::lround((1 - std::cos(2 * kPi * p)) * 50));
    std::snprintf(a, sizeof(a), "iluminada al %d %%", lit % 1000);
    text(r, NOTOSERIF_12_FONT_ID, x + 52, y + lineH, a);
    const bool leap =
        (page.now.tm_year % 4 == 0 && page.now.tm_year % 100 != 0) || (page.now.tm_year + 1900) % 400 == 0;
    std::snprintf(a, sizeof(a),
                  "quedan %d d\xC3\xAD"
                  "as",
                  ((leap ? 365 : 364) - page.now.tm_yday) % 1000);
    text(r, NOTOSERIF_12_FONT_ID, x, y + lineH * 2, a);
  }
  const int split = x + inner * 63 / 100;
  r.fillRect(split, y + 4, 1, lineH * 3 - 8, true);
  if (d.hasForecast) {
    const int rxl = split + 14;
    std::snprintf(b, sizeof(b), "%s, %d\xC2\xB0", skyWord(wx::skyOf(d.forecast.current.code)),
                  wx::displayTemp(d.forecast.current.temp, d.fahrenheit));
    b[0] = static_cast<char>(b[0] >= 'a' && b[0] <= 'z' ? b[0] - 32 : b[0]);
    text(r, NOTOSERIF_12_FONT_ID, rxl, y, b, kBold);
    tempRange(page, a, sizeof(a));
    std::snprintf(b, sizeof(b), "de %s", a);
    text(r, NOTOSERIF_12_FONT_ID, rxl, y + lineH, b);
    std::snprintf(b, sizeof(b), "lluvia %d %%", rainToday(page) % 1000);
    text(r, NOTOSERIF_12_FONT_ID, rxl, y + lineH * 2, b);
  }
  y += lineH * 3 + 6;
  zone(screen, 0, notesTop, split, y - notesTop, Clock);
  zone(screen, split, notesTop, width - split, y - notesTop, Weather);

  // The markets on one line each side of a rule, then the sea.
  r.fillRect(x, y, inner, 1, true);
  y += 6;
  const int marketsTop = y;
  const int mw = inner / 3;
  for (int i = 0; i < mk::kAssets; ++i) {
    char price[24];
    char change[16];
    double pct = 0;
    const int mcx = x + mw * i + mw / 2;
    lowerName(mk::infoOf(static_cast<mk::Asset>(i)).name, a, sizeof(a));
    textCentre(r, NOTOSERIF_12_FONT_ID, mcx, y, a, kBold);
    if (quoteOf(page, i, price, sizeof(price), change, sizeof(change), pct)) {
      priceMove(r, NOTOSANS_12_FONT_ID, x + mw * i, y + lineH - 4, price, change, pct, mw);
    }
  }
  y += lineH * 3 - 4;
  zone(screen, 0, marketsTop, width, y - marketsTop, Markets);
  skyAndSea(page, b, sizeof(b), false, true);
  if (b[0] != 0 && bottom - y > lineH * 4) {
    r.fillRect(x, y, inner, 1, true);
    textCentre(r, NOTOSERIF_12_FONT_ID, cx, y + 4, b);
    zone(screen, 0, y, width, lineH + 8, Sea);
    y += lineH + 8;
  }
  r.fillRect(x, y, inner, 3, true);
  eventsBlock(screen, r, page, x, y + 8, inner, bottom, EventLook::Reader);
}

// What every style shares: when the data is from, which of the five this is,
// and the ring that fetches everything again.
void foot(toybox::Screen& screen, GfxRenderer& r, const Page& page, const int style) {
  const int width = screen.device().width;
  const int top = contentBottom(screen);
  const int cy = top + kFootH / 2 + 2;
  r.fillRectDither(toybox::kMargin, top + 1, width - toybox::kMargin * 2, 1, Color::DarkGray);
  // When the data is from, led by how often it is fetched: "datos" for by
  // hand, "auto 30m" and the like otherwise. A tap here steps the interval.
  {
    char lead[toybox::kIntChars + 12];
    if (page.autoMinutes <= 0) {
      std::snprintf(lead, sizeof(lead), "datos");
    } else if (page.autoMinutes >= 60) {
      std::snprintf(lead, sizeof(lead), "auto %dh", (page.autoMinutes / 60) % 100);
    } else {
      std::snprintf(lead, sizeof(lead), "auto %dm", page.autoMinutes % 100);
    }
    char when[toybox::kIntChars + 24];
    if (page.data.hasForecast && std::strlen(page.data.forecast.current.time) >= 16) {
      std::snprintf(when, sizeof(when), "%s %.5s", lead, page.data.forecast.current.time + 11);
    } else {
      std::snprintf(when, sizeof(when), "%s", lead);
    }
    text(r, NOTOSANS_12_FONT_ID, toybox::kMargin, cy - r.getLineHeight(NOTOSANS_12_FONT_ID) / 2, when,
         page.autoMinutes > 0 ? kBold : kRegular);
    zone(screen, 0, top, 160, kFootH + 6, Auto);
  }
  for (int i = 0; i < kStyles; ++i) {
    const int dx = width / 2 + (i * 2 - kStyles + 1) * 7;
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
  static constexpr const char* kNames[kStyles] = {"CLASICO", "PANEL",  "DIARIO",  "CALMA",
                                                  "ESFERAS", "ORBITA", "TABLERO", "SOLAR"};
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
    case 5:
      poster(screen, renderer, page);
      break;
    case 6:
      horizon(screen, renderer, page);
      break;
    case 7:
      mosaic(screen, renderer, page);
      break;
    case 8:
      ticket(screen, renderer, page);
      break;
    case 9:
      almanac(screen, renderer, page);
      break;
    case 0:
    default:
      classic(screen, renderer, page);
      break;
  }
  foot(screen, renderer, page, style >= 0 && style < kStyles ? style : 0);
}

}  // namespace todaystyles
