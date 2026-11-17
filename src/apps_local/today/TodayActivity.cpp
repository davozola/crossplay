#include "TodayActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../clock/ClockCore.h"
#include "../clock/ClockFaces.h"
#include "../history/HistoryCore.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxSerif.h"
#include "../ui/ToyboxTheme.h"
#include "../weather/WeatherScreens.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace wx = weather;
namespace mk = markets;
namespace nv = naval;

namespace {

enum : fui::ActionId {
  ActionRefresh = 1,
  ActionDismiss,
};

constexpr time_t kClockSetAfter = 1704067200;  // 2024-01-01: before it the clock was never set

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);

// A small ring: the degree sign the display face does not have.
void degree(toybox::Screen& screen, const int x, const int y, const int r) {
  toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r), fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r - 3),
               fui::Color::White);
}

void rule(toybox::Screen& screen, const int y) {
  lab::ink(screen, toybox::kMargin, y, screen.device().width - toybox::kMargin * 2, toybox::kHairline);
}

}  // namespace

std::unique_ptr<Activity> TodayActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TodayActivity>(renderer, mappedInput);
}

void TodayActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  data_ = makeUniqueNoThrow<todaystore::Snapshot>();
  if (!data_) {
    LOG_ERR("HOY", "OOM: %u bytes for the page's data", static_cast<unsigned>(sizeof(todaystore::Snapshot)));
    shelf::leave(renderer, mappedInput);
    return;
  }
  todaystore::load(*data_);
  loadEvent();
  shownMinute_ = -1;
  statusHeadline_ = nullptr;
  requestUpdate();
}

void TodayActivity::onExit() {
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

void TodayActivity::startRefresh() {
  step_ = Step::Joining;
  statusHeadline_ = "WI-FI";
  report_[0] = 0;
  statusDismissable_ = false;
  wifiActivated_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    step_ = Step::Ready;
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             step_ = Step::Idle;
                             statusHeadline_ = "SIN WI-FI";
                             std::snprintf(report_, sizeof(report_), "LO ULTIMO DESCARGADO SIGUE EN LA TARJETA.");
                             statusDismissable_ = true;
                           } else {
                             step_ = Step::Ready;
                           }
                           requestUpdate();
                         });
}

void TodayActivity::loop() {
  if (!data_) return;
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = "DESCARGANDO";
    std::snprintf(report_, sizeof(report_), "TIEMPO, MAR Y MERCADOS");
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    step_ = Step::Idle;
    const int kinds = todaystore::fetchAll(*data_, report_, sizeof(report_));
    if (kinds == 3) {
      statusHeadline_ = nullptr;
    } else {
      // Say which source it was; what did arrive is on the page behind this.
      statusHeadline_ = kinds == 0 ? "SIN RESPUESTA" : "FALTA ALGO";
      statusDismissable_ = true;
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    if (statusHeadline_ != nullptr) {
      statusHeadline_ = nullptr;
      requestUpdate();
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY) && interactionsReady_) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    const fui::ActionEvent event = interactions_.route(input);
    if (event.action == ActionRefresh && step_ == Step::Idle) {
      startRefresh();
      requestUpdate();
    } else if (event.action == ActionDismiss) {
      statusHeadline_ = nullptr;
      requestUpdate();
    }
    return;
  }

  // The minute turning over.
  const time_t t = time(nullptr);
  const int minute = t >= kClockSetAfter ? static_cast<int>((t / 60) % 100000) : -2;
  if (minute != shownMinute_ && statusHeadline_ == nullptr) requestUpdate();
}

void TodayActivity::buildStatus(toybox::Screen& screen) {
  lab::chrome(screen, "HOY");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int y = screen.device().height / 2 - 120;
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), statusHeadline_, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Left);
  lab::paragraph(screen, x, y + toybox::kDisplayCut.inkHeight + 22, inner, report_, 5);
  if (statusDismissable_) lab::button(screen, lab::rect(x, y + 220, inner, 48), "OK", ActionDismiss, 0);
}

void TodayActivity::loadEvent() {
  eventText_[0] = 0;
  eventYear_ = 0;
  const time_t t = time(nullptr);
  if (t < kClockSetAfter) return;
  struct tm parts = {};
  localtime_r(&t, &parts);
  char path[64];
  std::snprintf(path, sizeof(path), "/.crosspoint/efemerides/%02d-%02d.txt", (parts.tm_mon + 1) % 100,
                parts.tm_mday % 100);
  std::string text;
  if (!Storage.exists(path) || !Storage.readFileToString("HOY", path, 96u * 1024u, text)) return;
  std::vector<history::Event> events;
  events.reserve(128);
  history::unpack(text, events);
  // Science first, then whatever the day has.
  int pick = -1;
  for (int k = 0; k < history::kKinds && pick < 0; ++k) {
    pick = history::featured(events, static_cast<history::Kind>(k), parts.tm_mon + 1, parts.tm_mday);
  }
  if (pick < 0) return;
  std::snprintf(eventText_, sizeof(eventText_), "%s", events[static_cast<size_t>(pick)].text.c_str());
  eventYear_ = events[static_cast<size_t>(pick)].year;
}

void TodayActivity::buildPage(toybox::Screen& screen) {
  const todaystore::Snapshot& d = *data_;
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  char a[32];
  char b[72];

  // --- The hour, white on a black band across the top. ---
  const time_t t = time(nullptr);
  const bool clockSet = t >= kClockSetAfter;
  struct tm parts = {};
  if (clockSet) localtime_r(&t, &parts);
  const int heroH = 204;
  renderer.fillRect(0, 0, width, heroH, true);
  {
    const float boxW = 78;
    const float pen = 8.5f;
    const float boxH = boxW * clockcore::kNumeralHeight;
    float nx = static_cast<float>(x + 4);
    const float ny = 22;
    const int digits[4] = {parts.tm_hour / 10, parts.tm_hour % 10, parts.tm_min / 10, parts.tm_min % 10};
    for (int i = 0; i < 4; ++i) {
      if (clockSet) {
        clockfaces::drawNumeral(renderer, digits[i], nx, ny, boxW, pen, false);
      } else {
        renderer.fillRect(static_cast<int>(nx) + 14, static_cast<int>(ny + boxH / 2) - 5, static_cast<int>(boxW) - 28,
                          10, false);
      }
      if (i == 1) {
        const int cx = static_cast<int>(nx + boxW + 18);
        renderer.fillRoundedRect(cx - 6, static_cast<int>(ny + boxH * 0.30f) - 6, 12, 12, 6, Color::White);
        renderer.fillRoundedRect(cx - 6, static_cast<int>(ny + boxH * 0.70f) - 6, 12, 12, 6, Color::White);
      }
      nx += boxW + (i == 1 ? 36 : 10);
    }
    // The day under the numerals; when the data is from, in the corner.
    if (clockSet) {
      std::snprintf(b, sizeof(b), "%s %d %s", clockcore::weekdayEs(parts.tm_wday), parts.tm_mday,
                    clockcore::monthEs(parts.tm_mon + 1));
      renderer.drawText(toybox::kUiFontId, x + 4,
                        heroH - 16 - toybox::kUiCut.inkHeight - (toybox::kUiCut.ascender - toybox::kUiCut.inkHeight), b,
                        false);
    }
    // Fetching everything again: a ring with an arrowhead, top right.
    const int bx = width - x - 46;
    const int by = 16;
    fui::ButtonProps refresh;
    refresh.label = "";
    refresh.action = ActionRefresh;
    refresh.styles.explicitlySet = true;
    refresh.minTouchSize = 0;
    screen.button(refresh, lab::rect(bx - 14, 0, 46 + 14 + x, 84));
    renderer.drawRoundedRect(bx, by, 46, 46, 2, 12, false);
    renderer.drawRoundedRect(bx + 12, by + 12, 22, 22, 3, 11, false);
    renderer.fillRect(bx + 23, by + 8, 14, 12, true);
    const int ax[3] = {bx + 22, bx + 32, bx + 22};
    const int ay[3] = {by + 8, by + 14, by + 20};
    renderer.fillPolygon(ax, ay, 3, false);
    if (d.hasForecast && std::strlen(d.forecast.current.time) >= 16) {
      std::snprintf(b, sizeof(b), "%.5s", d.forecast.current.time + 11);
      const int w = renderer.getTextWidth(toybox::kTileFontId, b);
      renderer.drawText(toybox::kTileFontId, bx + (46 - w) / 2,
                        by + 54 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight), b, false);
    }
  }
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
                   d.hasPlace ? "SIN PREVISION TODAVIA. TOCA EL ANILLO DE ARRIBA CON WI-FI."
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
  if (eventText_[0] != 0 && bottom - y >= 70) {
    rule(screen, y);
    y += 10;
    history::yearLabel(eventYear_, a, sizeof(a));
    std::snprintf(b, sizeof(b), "UN DIA COMO HOY, %s", a);
    const int tabW = renderer.getTextWidth(toybox::kTileFontId, b) + 16;
    renderer.fillRect(x, y, tabW, 22, true);
    renderer.drawText(
        toybox::kTileFontId, x + 8,
        y + (22 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight), b, false);
    toyboxserif::draw(renderer, toybox::kReadingSmallFontId, 25, x, y + 24, inner, eventText_, (bottom - y - 24) / 25);
  }
}

void TodayActivity::render(RenderLock&&) {
  const time_t t = time(nullptr);
  shownMinute_ = t >= kClockSetAfter ? static_cast<int>((t / 60) % 100000) : -2;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (statusHeadline_ != nullptr || !data_) {
    if (data_) buildStatus(screen);
  } else {
    buildPage(screen);
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Today");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
