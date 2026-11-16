#include "MarketsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "MarketsStore.h"

namespace mk = markets;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/markets.sav";
constexpr uint8_t kSaveVersion = 1;
}  // namespace

std::unique_ptr<Activity> MarketsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MarketsActivity>(renderer, mappedInput);
}

void MarketsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  marketsstore::loadPicks();
  for (int i = 0; i < mk::kAssets; ++i) loadCached(static_cast<mk::Asset>(i));
  marketsstore::loadPortfolio(portfolio_);
  reprice();
  view_ = View::Dashboard;
  lastFetchMs_ = static_cast<uint32_t>(millis());
  requestUpdate();
}

void MarketsActivity::onExit() {
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

void MarketsActivity::loadState() {
  autoMinutes_ = 0;
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("MKTS", kSavePath, file)) return;
  uint8_t data[3] = {};
  if (file.read(data, sizeof(data)) != sizeof(data) || data[0] != kSaveVersion) return;
  for (const int choice : marketsui::kAutoChoices) {
    if (choice == data[1]) autoMinutes_ = choice;
  }
  if (data[2] <= static_cast<uint8_t>(marketsui::Span::Quarter)) span_ = static_cast<marketsui::Span>(data[2]);
}

void MarketsActivity::saveState() {
  HalFile file;
  if (!Storage.openFileForWrite("MKTS", kSavePath, file)) return;
  const uint8_t data[3] = {kSaveVersion, static_cast<uint8_t>(autoMinutes_), static_cast<uint8_t>(span_)};
  file.write(data, sizeof(data));
  file.flush();
}

bool MarketsActivity::loadCached(const mk::Asset asset) {
  return marketsstore::loadCached(asset, quotes_[static_cast<int>(asset)]);
}

int MarketsActivity::fetchAll() {
  const int ok = marketsstore::fetchAll(quotes_, report_, sizeof(report_));
  lastFetchMs_ = static_cast<uint32_t>(millis());
  reprice();
  return ok;
}

void MarketsActivity::reprice() { marketsstore::pricePortfolio(portfolio_, priced_, euro_); }

void MarketsActivity::openAmount(const int instrument) {
  amountInstrument_ = instrument;
  const int at = mk::positionOf(portfolio_, instrument);
  mk::entryFrom(at >= 0 ? portfolio_.positions[at].amount : 0, entry_);
  view_ = View::Amount;
}

void MarketsActivity::startRefresh() {
  step_ = Step::Joining;
  showStatus("JOINING WI-FI", "", false);
  wifiActivated_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    step_ = Step::Ready;
    return;
  }
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput), [this](const ActivityResult& result) {
        if (result.isCancelled) {
          step_ = Step::Idle;
          showStatus("NO WI-FI", "QUOTES NEED A NETWORK. THE LAST ONES FETCHED STAY ON THE CARD.", true);
        } else {
          step_ = Step::Ready;
        }
        requestUpdate();
      });
}

void MarketsActivity::showStatus(const char* headline, const char* detail, const bool dismissable) {
  if (view_ != View::Status) statusBack_ = view_;
  statusHeadline_ = headline;
  statusDetail_ = detail;
  statusDismissable_ = dismissable;
  view_ = View::Status;
}

void MarketsActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = "FETCHING";
    std::snprintf(fetching_, sizeof(fetching_), "%s, %s, %s%s", mk::infoOf(mk::Asset::Bitcoin).name,
                  mk::infoOf(mk::Asset::Sp500).name, mk::infoOf(mk::Asset::Nasdaq).name,
                  portfolio_.count > 0 ? " AND THE PORTFOLIO" : "");
    statusDetail_ = fetching_;
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    step_ = Step::Idle;
    const int ok = fetchAll();
    if (ok == mk::kAssets && std::strncmp(report_, "ALL", 3) == 0) {
      view_ = statusBack_;
    } else {
      // Say which source it was; what did arrive is on the dashboard behind this.
      view_ = statusBack_;
      showStatus(ok == 0 ? "NO ANSWER" : "NOT ALL ANSWERED", report_, true);
    }
    requestUpdate();
    return;
  }

  // Auto-refresh: quietly, with no status page, and only when already online.
  // The radio reconnects by itself after a drop; a round that finds it down
  // is skipped and the next one tries again.
  if (autoMinutes_ > 0 && step_ == Step::Idle &&
      (view_ == View::Dashboard || view_ == View::Detail || view_ == View::Portfolio)) {
    const uint32_t now = static_cast<uint32_t>(millis());
    if (now - lastFetchMs_ >= static_cast<uint32_t>(autoMinutes_) * 60000u) {
      lastFetchMs_ = now;
      if (wifiActivated_ && WiFi.status() == WL_CONNECTED && fetchAll() > 0) requestUpdate();
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    if (view_ == View::Dashboard) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    // Each page goes back to the one it was opened from.
    if (view_ == View::Status) {
      view_ = statusBack_;
    } else if (view_ == View::CardPicker) {
      view_ = View::Detail;
    } else if (view_ == View::HoldingPicker || view_ == View::Amount) {
      view_ = View::Portfolio;
    } else {
      view_ = View::Dashboard;
    }
    requestUpdate();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions_.route(input));
}

void MarketsActivity::route(const fui::ActionEvent& event) {
  namespace ui = marketsui;
  switch (event.action) {
    case ui::ActionOpen:
      if (event.value < 0 || event.value >= mk::kAssets) return;
      open_ = static_cast<mk::Asset>(event.value);
      view_ = View::Detail;
      break;
    case ui::ActionRefresh:
      startRefresh();
      break;
    case ui::ActionAuto:
      view_ = View::AutoPicker;
      break;
    case ui::ActionChooseAuto: {
      int chosen = -1;
      for (const int choice : ui::kAutoChoices) {
        if (choice == event.value) chosen = choice;
      }
      if (chosen < 0) return;
      autoMinutes_ = chosen;
      saveState();
      view_ = View::Dashboard;
      // Turning it on with nothing fresh fetches straight away, which is also
      // what joins the network the timer will need.
      if (autoMinutes_ > 0 && !wifiActivated_) startRefresh();
      break;
    }
    case ui::ActionSpan:
      if (event.value < 0 || event.value > static_cast<int16_t>(ui::Span::Quarter)) return;
      span_ = static_cast<ui::Span>(event.value);
      saveState();
      break;
    case ui::ActionDismiss:
      view_ = statusBack_;
      break;
    case ui::ActionPortfolio:
      reprice();
      view_ = View::Portfolio;
      break;
    case ui::ActionChange:
      view_ = View::CardPicker;
      break;
    case ui::ActionAddPosition:
      if (portfolio_.count >= mk::kMaxPositions) return;
      view_ = View::HoldingPicker;
      break;
    case ui::ActionPick:
      if (event.value < 0 || event.value >= mk::kInstruments) return;
      if (view_ == View::CardPicker) {
        mk::Picks picks = mk::picks();
        picks.slot[static_cast<int>(open_)] = static_cast<uint8_t>(event.value);
        marketsstore::savePicks(picks);
        // The quote kept for it, if it was ever fetched; otherwise the card
        // says to refresh.
        quotes_[static_cast<int>(open_)] = mk::Quote{};
        loadCached(open_);
        view_ = View::Dashboard;
      } else if (view_ == View::HoldingPicker) {
        openAmount(event.value);
      } else {
        return;
      }
      break;
    case ui::ActionPosition:
      if (event.value < 0 || event.value >= portfolio_.count) return;
      openAmount(portfolio_.positions[event.value].instrument);
      break;
    case ui::ActionKey:
      if (view_ != View::Amount || !mk::entryKey(entry_, static_cast<char>(event.value))) return;
      break;
    case ui::ActionSaveAmount:
    case ui::ActionRemovePosition:
      if (view_ != View::Amount) return;
      // Saving nothing, or zero, is taking it out.
      mk::setPosition(portfolio_, amountInstrument_,
                      event.action == ui::ActionRemovePosition ? 0 : mk::entryValue(entry_));
      marketsstore::savePortfolio(portfolio_);
      reprice();
      view_ = View::Portfolio;
      break;
    default:
      return;
  }
  requestUpdate();
}

void MarketsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  marketsui::FaceSwap faces;
  faces.ctx = &target;
  faces.bindTitle = [](void* ctx, const marketsui::Face face) {
    int id = toybox::kDisplayFontId;
    if (face == marketsui::Face::Large) id = toybox::kLargeFontId;
    if (face == marketsui::Face::Huge) id = toybox::kHugeFontId;
    static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE, id);
  };

  switch (view_) {
    case View::Dashboard: {
      marketsui::DashboardModel m;
      m.quotes = quotes_;
      m.autoMinutes = autoMinutes_;
      m.faces = faces;
      marketsui::buildDashboard(surface, m);
      break;
    }
    case View::Detail: {
      marketsui::DetailModel m;
      m.asset = open_;
      m.quote = &quotes_[static_cast<int>(open_)];
      m.span = span_;
      m.faces = faces;
      marketsui::buildDetail(surface, m);
      break;
    }
    case View::AutoPicker:
      marketsui::buildAutoPicker(surface, autoMinutes_);
      break;
    case View::Portfolio: {
      marketsui::PortfolioModel m;
      m.portfolio = &portfolio_;
      m.priced = priced_;
      m.euro = euro_;
      m.faces = faces;
      marketsui::buildPortfolio(surface, m);
      break;
    }
    case View::CardPicker: {
      marketsui::PickerModel m;
      m.title = "SHOW HERE";
      m.current = mk::instrumentOf(open_);
      marketsui::buildPicker(surface, m);
      break;
    }
    case View::HoldingPicker: {
      marketsui::PickerModel m;
      m.title = "ADD";
      m.ownableOnly = true;
      m.held = &portfolio_;
      marketsui::buildPicker(surface, m);
      break;
    }
    case View::Amount: {
      marketsui::AmountModel m;
      m.instrument = amountInstrument_;
      m.text = entry_.text;
      m.held = mk::positionOf(portfolio_, amountInstrument_) >= 0;
      m.faces = faces;
      marketsui::buildAmount(surface, m);
      break;
    }
    case View::Status: {
      marketsui::StatusModel m;
      m.headline = statusHeadline_;
      m.detail = statusDetail_;
      m.dismissable = statusDismissable_;
      marketsui::buildStatus(surface, m);
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Markets");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
