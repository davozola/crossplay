#include "TodayActivity.h"

#include <HalDisplay.h>
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
#include "TodayStyles.h"

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

constexpr char kStylePath[] = "/.crosspoint/hoy.cfg";
constexpr time_t kClockSetAfter = 1704067200;  // 2024-01-01: before it the clock was never set

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);

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
  style_ = 0;
  if (Storage.exists(kStylePath)) {
    HalFile file;
    uint8_t saved = 0;
    if (Storage.openFileForRead("HOY", kStylePath, file) && file.read(&saved, 1) == 1 && saved < todaystyles::kStyles) {
      style_ = saved;
    }
  }
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

  int pressX = 0;
  int pressY = 0;
  if (mappedInput.wasScreenTouchPressed(pressX, pressY)) pressX_ = pressX;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    if (statusHeadline_ != nullptr) {
      statusHeadline_ = nullptr;
      requestUpdate();
      return;
    }
    // A rightward swipe from mid-page arrives as Back: it is the face before.
    if (mappedInput.wasBackGesture() && pressX_ >= 28) {
      setStyle(style_ + todaystyles::kStyles - 1);
      return;
    }
    shelf::leave(renderer, mappedInput);
    return;
  }

  // A swipe either way is the next face or the one before.
  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::None && step_ == Step::Idle && statusHeadline_ == nullptr) {
    const bool on = swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up;
    setStyle(style_ + (on ? 1 : todaystyles::kStyles - 1));
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
    const int target = static_cast<int>(event.action) - static_cast<int>(todaystyles::kTargetAction);
    if (target == todaystyles::Refresh && step_ == Step::Idle) {
      startRefresh();
      requestUpdate();
    } else if (target > todaystyles::None && target < todaystyles::Refresh && step_ == Step::Idle &&
               statusHeadline_ == nullptr) {
      // Each part of the page opens the app it is drawn from, and Back there
      // comes back here.
      static constexpr const char* kApps[] = {"", "RELOJ", "WEATHER", "MARKETS", "NAVAL WEATHER", "EFEMERIDES"};
      shelf::openFrom(kApps[target], "HOY", renderer, mappedInput);
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

void TodayActivity::setStyle(const int style) {
  style_ = ((style % todaystyles::kStyles) + todaystyles::kStyles) % todaystyles::kStyles;
  HalFile file;
  if (Storage.openFileForWrite("HOY", kStylePath, file)) {
    const uint8_t saved = static_cast<uint8_t>(style_);
    file.write(&saved, 1);
    file.flush();
  }
  cleanNext_ = true;
  requestUpdate();
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
  const time_t t = time(nullptr);
  todaystyles::Page page{*data_};
  page.clockSet = t >= kClockSetAfter;
  if (page.clockSet) localtime_r(&t, &page.now);
  page.eventText = eventText_;
  page.eventYear = eventYear_;
  todaystyles::draw(style_, screen, renderer, page);
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
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
