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

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../clock/ClockCore.h"
#include "../clock/ClockFaces.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
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

// An arrowhead for a rise or a fall.
void tick(toybox::Screen& screen, const int x, const int y, const bool up) {
  for (int i = 0; i < 8; ++i) {
    const int w = up ? i * 2 + 2 : (7 - i) * 2 + 2;
    lab::ink(screen, x - w / 2, y + i, w, 1);
  }
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

void TodayActivity::buildPage(toybox::Screen& screen) {
  const todaystore::Snapshot& d = *data_;
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  char a[32];
  char b[64];

  // The time in the clock's own numerals, and the date beside it.
  const time_t t = time(nullptr);
  const bool clockSet = t >= kClockSetAfter;
  struct tm parts = {};
  if (clockSet) localtime_r(&t, &parts);
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
    const int rowH = 44;
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

  // When the forecast is from, and the way to fetch everything again.
  if (d.hasForecast && std::strlen(d.forecast.current.time) >= 16) {
    std::snprintf(b, sizeof(b), "DATOS DE LAS %.5s", d.forecast.current.time + 11);
    lab::caption(screen, x, bottom - 46 + (46 - toybox::kTileCut.inkHeight) / 2, inner - 190, b, fui::TextAlign::Left,
                 fui::Color::DarkGray);
  }
  lab::button(screen, lab::rect(x + inner - 180, bottom - 46, 180, 46), "ACTUALIZAR", ActionRefresh, 0);
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
