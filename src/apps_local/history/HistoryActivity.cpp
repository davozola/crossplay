#include "HistoryActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../discover/DiscoverCore.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxSerif.h"
#include "../ui/ToyboxTheme.h"
#include "HistoryStore.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace hs = history;

namespace {

enum : fui::ActionId {
  ActionDayPrev = 1,
  ActionDayNext,
  ActionFetch,
  ActionKind,  // value: a Kind, or -1 for everything
  ActionOpen,  // value: the row on this page
  ActionLead,  // value: an index into the day's events
  ActionPagePrev,
  ActionPageNext,
  ActionDismiss,
};

constexpr int kBottomReserve = 40;
constexpr uint8_t kCleanAfterPaints = 8;
constexpr int kRows = 5;
constexpr int kFootH = 48;
constexpr int kBodyFont = toybox::kReadingSmallFontId;
constexpr int kPageFont = toybox::kReadingFontId;

void dayPath(const int month, const int day, char* out, const size_t cap) {
  historystore::dayPath(month, day, out, cap);
}

}  // namespace

std::unique_ptr<Activity> HistoryActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<HistoryActivity>(renderer, mappedInput);
}

void HistoryActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  const time_t now = time(nullptr);
  thisYear_ = 0;
  month_ = 1;
  day_ = 1;
  if (now > 1700000000) {
    int year = 0;
    // Local days: the reader lives an hour or two east of Greenwich.
    struct tm local{};
    localtime_r(&now, &local);
    year = local.tm_year + 1900;
    month_ = local.tm_mon + 1;
    day_ = local.tm_mday;
    thisYear_ = year;
  }
  loadDay();
  view_ = View::Day;
  cleanNext_ = true;
  requestUpdate();
}

void HistoryActivity::onExit() {
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

bool HistoryActivity::loadDay() {
  events_.clear();
  char path[64];
  dayPath(month_, day_, path, sizeof(path));
  std::string text;
  if (!Storage.exists(path) || !Storage.readFileToString("EFEM", path, 96u * 1024u, text)) return false;
  events_.reserve(128);
  hs::unpack(text, events_);
  return !events_.empty();
}

void HistoryActivity::showStatus(const char* headline, const char* detail, const bool dismissable) {
  if (view_ != View::Status) statusBack_ = view_;
  statusHeadline_ = headline;
  std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", detail);
  statusDismissable_ = dismissable;
  view_ = View::Status;
}

void HistoryActivity::startFetch() {
  step_ = Step::Joining;
  showStatus("CONECTANDO", "", false);
  wifiActivated_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    step_ = Step::Ready;
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             step_ = Step::Idle;
                             showStatus("SIN WIFI", "LOS DIAS QUE YA SE BUSCARON SIGUEN EN LA TARJETA.", true);
                           } else {
                             step_ = Step::Ready;
                           }
                           requestUpdate();
                         });
}

void HistoryActivity::runFetch() {
  view_ = statusBack_;
  std::vector<hs::Event> fresh;
  if (!historystore::fetchDay(month_, day_, fresh)) {
    showStatus("SIN RESPUESTA", "WIKIPEDIA NO HA CONTESTADO, O NO TIENE NADA PARA ESE DIA. PRUEBA MAS TARDE.", true);
    return;
  }
  events_.swap(fresh);
  cleanNext_ = true;
}

int HistoryActivity::listedCount() const {
  if (filter_ < 0) return static_cast<int>(events_.size());
  return hs::countOf(events_, static_cast<hs::Kind>(filter_));
}

int HistoryActivity::listed(const int row) const {
  if (row < 0) return -1;
  if (filter_ < 0) return row < static_cast<int>(events_.size()) ? row : -1;
  int seen = 0;
  for (size_t i = 0; i < events_.size(); ++i) {
    if (static_cast<int>(events_[i].kind) != filter_) continue;
    if (seen == row) return static_cast<int>(i);
    ++seen;
  }
  return -1;
}

void HistoryActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = "BUSCANDO";
    char label[40];
    hs::dayLabel(month_, day_, label, sizeof(label));
    std::snprintf(statusDetail_, sizeof(statusDetail_), "%s. LA LISTA COMPLETA TARDA UN POCO.", label);
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    step_ = Step::Idle;
    runFetch();
    requestUpdate();
    return;
  }

  auto pageBy = [&](const int by) {
    const int pages = (listedCount() + kRows - 1) / kRows;
    if (page_ + by >= 0 && page_ + by < pages) page_ += by;
  };
  auto dayBy = [&](const int by) {
    hs::stepDay(month_, day_, by);
    loadDay();
    cleanNext_ = true;
  };

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    if (view_ == View::Day) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view_ == View::Status) {
      view_ = statusBack_;
    } else if (view_ == View::Event) {
      view_ = statusBack_ == View::List ? View::List : View::Day;
    } else {
      view_ = View::Day;
    }
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::None && step_ == Step::Idle) {
    const int by = swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up ? 1 : -1;
    if (view_ == View::List) {
      pageBy(by);
      requestUpdate();
      return;
    }
    if (view_ == View::Day) {
      dayBy(by);
      requestUpdate();
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  if (event.action == ActionDayPrev || event.action == ActionDayNext) {
    dayBy(event.action == ActionDayNext ? 1 : -1);
    requestUpdate();
    return;
  }
  if (event.action == ActionPagePrev || event.action == ActionPageNext) {
    pageBy(event.action == ActionPageNext ? 1 : -1);
    requestUpdate();
    return;
  }
  route(event);
}

void HistoryActivity::route(const fui::ActionEvent& event) {
  switch (event.action) {
    case ActionFetch:
      startFetch();
      break;
    case ActionKind:
      if (event.value < -1 || event.value >= hs::kKinds) return;
      filter_ = event.value;
      page_ = 0;
      view_ = View::List;
      cleanNext_ = true;
      break;
    case ActionOpen: {
      const int index = listed(page_ * kRows + event.value);
      if (index < 0) return;
      open_ = index;
      statusBack_ = View::List;
      view_ = View::Event;
      cleanNext_ = true;
      break;
    }
    case ActionLead:
      if (event.value < 0 || event.value >= static_cast<int>(events_.size())) return;
      open_ = event.value;
      statusBack_ = View::Day;
      view_ = View::Event;
      cleanNext_ = true;
      break;
    case ActionDismiss:
      view_ = statusBack_;
      cleanNext_ = true;
      break;
    default:
      return;
  }
  requestUpdate();
}

void HistoryActivity::buildDay(toybox::Screen& screen) {
  lab::chrome(screen, "EFEMERIDES");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 12;
  char text[72];

  // The day, with a step either side of it.
  const int side = 52;
  lab::button(screen, lab::rect(x, y, side, 48), "<", ActionDayPrev, 0, false, false);
  lab::button(screen, lab::rect(x + inner - side, y, side, 48), ">", ActionDayNext, 0, false, false);
  hs::dayLabel(month_, day_, text, sizeof(text));
  lab::label(screen, lab::rect(x + side, y, inner - side * 2, 48), text, toybox::kUiFont, toybox::kUiCut);
  y += 48 + 12;

  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  if (events_.empty()) {
    lab::label(screen, lab::rect(x, y + 60, inner, toybox::kDisplayCut.inkHeight), "SIN BUSCAR", toybox::kDisplayFont,
               toybox::kDisplayCut, fui::TextAlign::Left);
    lab::paragraph(screen, x, y + 60 + toybox::kDisplayCut.inkHeight + 22, inner,
                   thisYear_ == 0
                       ? "EL RELOJ NO ESTA EN HORA, ASI QUE NO SE QUE DIA ES HOY: ELIGE EL DIA CON LAS FLECHAS. "
                         "LO QUE SE BUSCA SE QUEDA EN LA TARJETA."
                       : "TOCA BUSCAR PARA TRAER DE WIKIPEDIA LO QUE PASO UN DIA COMO ESTE. LO QUE SE BUSCA SE "
                         "QUEDA EN LA TARJETA.",
                   5);
    lab::button(screen, lab::rect(x, footY, inner, kFootH), "BUSCAR", ActionFetch, 0, true, false);
    return;
  }

  // One event of each kind to lead with.
  const int gap = 10;
  const int cardH = (footY - 10 - y - gap * (hs::kKinds - 1)) / hs::kKinds;
  for (int k = 0; k < hs::kKinds; ++k) {
    const hs::Kind kind = static_cast<hs::Kind>(k);
    const int index = hs::featured(events_, kind, month_, day_);
    const int cy = y + k * (cardH + gap);
    renderer.drawRoundedRect(x, cy, inner, cardH, 2, 12, true);
    // The kind on a black tab in the frame's corner.
    const int tabW = renderer.getTextWidth(toybox::kTileFontId, hs::kindName(kind)) + 20;
    renderer.fillRoundedRect(x, cy, tabW, 26, 12, Color::Black);
    renderer.fillRect(x + tabW - 12, cy, 12, 26, true);
    renderer.fillRect(x, cy + 13, 12, 13, true);
    renderer.drawText(
        toybox::kTileFontId, x + 10,
        cy + (26 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight),
        hs::kindName(kind), false);
    if (index < 0) {
      lab::caption(screen, x, cy + cardH / 2, inner, "NADA ESE DIA", fui::TextAlign::Center, fui::Color::DarkGray);
      continue;
    }
    const hs::Event& e = events_[static_cast<size_t>(index)];
    fui::ButtonProps open;
    open.label = "";
    open.action = ActionLead;
    open.value = static_cast<int16_t>(index);
    open.styles.explicitlySet = true;
    open.minTouchSize = 0;
    screen.button(open, lab::rect(x, cy, inner, cardH));
    char year[24];
    hs::yearLabel(e.year, year, sizeof(year));
    const int yearW = renderer.getTextWidth(toybox::kDisplayFontId, year);
    renderer.drawText(toybox::kDisplayFontId, x + inner - 16 - yearW, cy + 6, year, true);
    if (thisYear_ > 0 && e.year > 0 && thisYear_ > e.year) {
      std::snprintf(text, sizeof(text), "HACE %d", (thisYear_ - e.year) % 100000);
      lab::caption(screen, x + tabW + 10, cy + 8, inner - tabW - 26 - yearW - 10, text, fui::TextAlign::Left,
                   fui::Color::DarkGray);
    }
    const int textY = cy + 6 + toybox::kDisplayCut.ascender + 4;
    toyboxserif::draw(renderer, kBodyFont, 27, x + 16, textY, inner - 32, e.text.c_str(),
                      (cy + cardH - 8 - textY) / 27);
  }

  const int third = (inner - 6 * 3) / 4;
  static constexpr const char* kShort[4] = {"TODO", "CIENCIA", "HISTORIA", "CULTURA"};
  for (int i = 0; i < 4; ++i) {
    lab::button(screen, lab::rect(x + i * (third + 6), footY, third, kFootH), kShort[i], ActionKind, i - 1);
  }
}

void HistoryActivity::buildList(toybox::Screen& screen) {
  char title[40];
  hs::dayLabel(month_, day_, title, sizeof(title));
  lab::chrome(screen, filter_ < 0 ? "TODO" : hs::kindName(static_cast<hs::Kind>(filter_)));
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  int y = toybox::kChromeHeight + 10;
  const int count = listedCount();
  const int pages = count > 0 ? (count + kRows - 1) / kRows : 1;
  if (page_ >= pages) page_ = pages - 1;
  const int rowH = (footY - 6 - y) / kRows;
  char text[48];
  for (int row = 0; row < kRows; ++row) {
    const int index = listed(page_ * kRows + row);
    if (index < 0) break;
    const hs::Event& e = events_[static_cast<size_t>(index)];
    fui::ButtonProps open;
    open.label = "";
    open.action = ActionOpen;
    open.value = static_cast<int16_t>(row);
    open.styles = toybox::rowStyles();
    open.styles.normal.borderWidth = 0;
    open.minTouchSize = 0;
    screen.button(open, lab::rect(x, y, inner, rowH - 2));
    hs::yearLabel(e.year, text, sizeof(text));
    lab::value(screen, x, y + 8, inner, text);
    lab::caption(screen, x, y + 12, inner, hs::kindName(e.kind), fui::TextAlign::Right, fui::Color::DarkGray);
    const int textY = y + 8 + toybox::kUiCut.inkHeight + 6;
    toyboxserif::draw(renderer, kBodyFont, 26, x, textY, inner, e.text.c_str(), (y + rowH - 6 - textY) / 26);
    y += rowH;
    lab::box(screen, x, y - 3, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }
  if (count == 0) lab::caption(screen, x, y + 60, inner, "NADA DE ESTO ESE DIA.", fui::TextAlign::Center);

  const int side = 56;
  lab::button(screen, lab::rect(x, footY, side, kFootH), "<", ActionPagePrev, 0, false, false);
  lab::button(screen, lab::rect(x + side + 6, footY, side, kFootH), ">", ActionPageNext, 0, false, false);
  std::snprintf(text, sizeof(text), "%d / %d", (page_ + 1) % 1000, pages % 1000);
  lab::caption(screen, x + side * 2 + 12, footY + (kFootH - toybox::kTileCut.inkHeight) / 2, 70, text,
               fui::TextAlign::Center);
  lab::caption(screen, x + side * 2 + 90, footY + (kFootH - toybox::kTileCut.inkHeight) / 2, inner - side * 2 - 90,
               title, fui::TextAlign::Right, fui::Color::DarkGray);
}

void HistoryActivity::buildEvent(toybox::Screen& screen) {
  if (open_ < 0 || open_ >= static_cast<int>(events_.size())) return;
  const hs::Event& e = events_[static_cast<size_t>(open_)];
  lab::chrome(screen, hs::kindName(e.kind));
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 24;
  char text[64];
  hs::dayLabel(month_, day_, text, sizeof(text));
  lab::caption(screen, x, y, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 10;
  hs::yearLabel(e.year, text, sizeof(text));
  renderer.drawText(e.year >= 0 ? toybox::kHugeFontId : toybox::kLargeFontId, x, y, text, true);
  y += (e.year >= 0 ? toybox::kHugeCut.ascender : toybox::kLargeCut.ascender) + 14;
  if (thisYear_ > 0 && e.year > 0 && thisYear_ > e.year) {
    // In the serif: the pixel face has no letter for the year's word.
    std::snprintf(text, sizeof(text), "Hace %d a\xC3\xB1os", (thisYear_ - e.year) % 100000);
    renderer.drawText(toybox::kReadingBoldSmallFontId, x, y + 2, text, true);
    y += 40;
  }
  lab::ink(screen, x, y, inner, 2);
  y += 16;
  toyboxserif::draw(renderer, kPageFont, 38, x, y, inner, e.text.c_str(), 12);
  lab::caption(screen, x, screen.device().height - kBottomReserve - toybox::kTileCut.inkHeight - 10, inner,
               "WIKIPEDIA, CC BY-SA", fui::TextAlign::Right, fui::Color::DarkGray);
}

void HistoryActivity::buildStatus(toybox::Screen& screen) {
  lab::chrome(screen, "EFEMERIDES");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int y = screen.device().height / 2 - 90;
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), statusHeadline_, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Center);
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Center;
  s.maxLines = 4;
  screen.target().text(lab::rect(x, y + toybox::kDisplayCut.inkHeight + 24, inner, toybox::kTileCut.lineHeight * 4),
                       statusDetail_, s);
  if (statusDismissable_) {
    lab::button(screen, lab::rect(x + inner / 4, y + toybox::kDisplayCut.inkHeight + 130, inner / 2, 56), "VALE",
                ActionDismiss, 0, true);
  }
}

void HistoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Day:
      buildDay(screen);
      break;
    case View::List:
      buildList(screen);
      break;
    case View::Event:
      buildEvent(screen);
      break;
    case View::Status:
      buildStatus(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "History");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
