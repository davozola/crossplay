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
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/AutoRefresh.h"
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
  applyOrientation();
  marketsstore::loadPicks();
  for (int i = 0; i < mk::kAssets; ++i) loadCached(static_cast<mk::Asset>(i));
  marketsstore::loadPortfolio(portfolio_);
  reprice();
  fetchedAt_ = marketsstore::fetchedAt();
  view_ = View::Dashboard;
  lastFetchMs_ = static_cast<uint32_t>(millis());
  quiet_.touched();
  // Left on auto, it comes back on auto, with a fetch straight away: behind
  // the quotes kept from last time, which are what the first frame shows.
  if (autoMinutes_ > 0) startRefresh(false);
  requestUpdate();
}

void MarketsActivity::applyOrientation() {
  renderer.setOrientation(wide_ ? GfxRenderer::Orientation::LandscapeCounterClockwise
                                : GfxRenderer::Orientation::Portrait);
}

void MarketsActivity::requestUpdate(const bool immediate) {
  quiet_.paintPending = true;
  Activity::requestUpdate(immediate);
}

void MarketsActivity::onExit() {
  // The orientation and the clip are the renderer's, not this app's: hand
  // them back upright and open.
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
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
  uint8_t data[6] = {};
  // The period, the turn and the currency came later: a shorter file is an older one, and still good.
  if (file.read(data, sizeof(data)) < 3 || data[0] != kSaveVersion) return;
  wide_ = data[4] == 1;
  inEuro_ = data[5] == 1;
  if (data[3] < mk::kPeriods) period_ = static_cast<mk::Period>(data[3]);
  for (const int choice : marketsui::kAutoChoices) {
    if (choice == data[1]) autoMinutes_ = choice;
  }
  if (data[2] <= static_cast<uint8_t>(marketsui::Span::Year)) span_ = static_cast<marketsui::Span>(data[2]);
}

void MarketsActivity::saveState() {
  HalFile file;
  if (!Storage.openFileForWrite("MKTS", kSavePath, file)) return;
  const uint8_t data[6] = {kSaveVersion,
                           static_cast<uint8_t>(autoMinutes_),
                           static_cast<uint8_t>(span_),
                           static_cast<uint8_t>(period_),
                           static_cast<uint8_t>(wide_ ? 1 : 0),
                           static_cast<uint8_t>(inEuro_ ? 1 : 0)};
  file.write(data, sizeof(data));
  file.flush();
}

bool MarketsActivity::loadCached(const mk::Asset asset) {
  return marketsstore::loadCached(asset, quotes_[static_cast<int>(asset)]);
}

int MarketsActivity::fetchAll() {
  const int ok = marketsstore::fetchAll(quotes_, report_, sizeof(report_));
  lastFetchMs_ = static_cast<uint32_t>(millis());
  const time_t t = time(nullptr);
  // Only once the clock has been set; a reader fresh from a flash has 1970.
  if (ok > 0 && t > 1700000000) fetchedAt_ = static_cast<int64_t>(t);
  reprice();
  return ok;
}

// "21:40" for today, "18 NOV 21:40" for any other day: quotes read off the
// card must not pass for this morning's.
void MarketsActivity::sayUpdated() {
  updated_[0] = 0;
  if (fetchedAt_ <= 0) return;
  static constexpr const char* kMonths[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                              "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const time_t at = static_cast<time_t>(fetchedAt_);
  const time_t now = time(nullptr);
  struct tm then = {};
  struct tm today = {};
  localtime_r(&at, &then);
  localtime_r(&now, &today);
  if (then.tm_year == today.tm_year && then.tm_yday == today.tm_yday) {
    std::snprintf(updated_, sizeof(updated_), "%02d:%02d", then.tm_hour % 24, then.tm_min % 60);
  } else {
    std::snprintf(updated_, sizeof(updated_), "%d %s %02d:%02d", then.tm_mday % 32, kMonths[then.tm_mon % 12],
                  then.tm_hour % 24, then.tm_min % 60);
  }
}

void MarketsActivity::reprice() { marketsstore::pricePortfolio(portfolio_, priced_, euro_, period_); }

void MarketsActivity::openAmount(const int instrument) {
  amountInstrument_ = instrument;
  const int at = mk::positionOf(portfolio_, instrument);
  mk::entryFrom(at >= 0 ? portfolio_.positions[at].amount : 0, entry_);
  view_ = View::Amount;
}

// The keyboard, for a ticker or a name; what comes back goes out as a search.
void MarketsActivity::askSearch() {
  searchFrom_ = view_;
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "TICKER OR NAME", "", 20);
  if (!keyboard) {
    LOG_ERR("MKTS", "OOM: the keyboard");
    return;
  }
  // The keyboard is an upright screen; the page turns back when it returns.
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    applyOrientation();
    interactionsReady_ = false;
    if (!result.isCancelled) {
      mk::tidyQuery(std::get<KeyboardResult>(result.data).text.c_str(), query_, sizeof(query_));
      if (query_[0] != 0) {
        job_ = Job::Search;
        startRefresh();
      }
    }
    requestUpdate();
  });
}

void MarketsActivity::runSearch() {
  int status = 0;
  foundCount_ = marketsstore::search(query_, found_, kMaxFound, status);
  view_ = View::Results;
}

// Makes a symbol one of the instruments, sparing the slots the cards and the
// portfolio are using. -1 when every slot is in use.
int MarketsActivity::adopt(const char* symbol, const char* company) {
  bool used[mk::kCustom] = {};
  auto spare = [&](const int instrument) {
    if (instrument >= mk::kBuiltIn && instrument < mk::kInstruments) used[instrument - mk::kBuiltIn] = true;
  };
  for (int i = 0; i < mk::kAssets; ++i) spare(mk::instrumentOf(static_cast<mk::Asset>(i)));
  for (int i = 0; i < portfolio_.count; ++i) spare(portfolio_.positions[i].instrument);
  bool recycled = false;
  const int instrument = mk::addCustom(symbol, company, used, &recycled);
  if (instrument < mk::kBuiltIn) return instrument;
  if (recycled) marketsstore::forget(instrument);
  marketsstore::saveCustoms();
  return instrument;
}

// An instrument chosen in a picker, or found by a search started from one.
// `fresh` is one just added, which has no quote on the card yet.
void MarketsActivity::pick(const int instrument, const bool fresh) {
  if (!mk::present(instrument)) return;
  const View from = view_ == View::Results ? searchFrom_ : view_;
  if (from == View::HoldingPicker) {
    if (!mk::instrumentAt(instrument).ownable) {
      view_ = View::Portfolio;
      showStatus("NOT THIS ONE", "ITS PRICE IS NOT IN DOLLARS OR EUROS, SO IT CANNOT BE ADDED TO THE TOTAL.", true);
      return;
    }
    openAmount(instrument);
    return;
  }
  mk::Picks picks = mk::picks();
  picks.slot[static_cast<int>(open_)] = static_cast<uint8_t>(instrument);
  marketsstore::savePicks(picks);
  // The quote kept for it, if it was ever fetched; otherwise the card says to
  // refresh, and one just found is fetched straight away.
  quotes_[static_cast<int>(open_)] = mk::Quote{};
  loadCached(open_);
  view_ = View::Dashboard;
  if (fresh && !quotes_[static_cast<int>(open_)].valid) {
    job_ = Job::Refresh;
    startRefresh();
  }
}

void MarketsActivity::startRefresh(const bool asked) {
  // With a page of quotes showing and a network to join alone, the page stays:
  // the join and the fetch happen behind it.
  const bool showing = view_ == View::Dashboard || view_ == View::Detail || view_ == View::Portfolio;
  if (job_ == Job::Refresh && showing && step_ == Step::Idle &&
      (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone())) {
    behindDue_ = true;
    behindPainted_ = false;
    behindAsked_ = asked;
    return;
  }
  step_ = Step::Joining;
  showStatus("JOINING WI-FI", "", false);
  wifiActivated_ = true;
  // Connected already, or a network saved to join by itself: the join happens
  // inside the fetch, with no picker in front of the page.
  if (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone()) {
    step_ = Step::Ready;
    return;
  }
  // The picker is an upright screen; the page turns back when it returns.
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  {
    // The saved network is joined behind the page; the picker is for when there is none.
    ActivityResultHandler afterWifi = [this](const ActivityResult& result) {
      applyOrientation();
      if (result.isCancelled) {
        step_ = Step::Idle;
        job_ = Job::Refresh;
        showStatus("NO WI-FI", "QUOTES NEED A NETWORK. THE LAST ONES FETCHED STAY ON THE CARD.", true);
      } else {
        step_ = Step::Ready;
      }
      requestUpdate();
    };
    if (autorefresh::joinSaved()) {
      afterWifi(ActivityResult{});
    } else {
      startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput), std::move(afterWifi));
    }
  }
}

// Says what a round that somebody asked for did not bring; a full one says nothing.
void MarketsActivity::reportRound(const int ok) {
  if (ok < 0) {
    showStatus("NO WI-FI", "COULD NOT JOIN THE SAVED NETWORK. THE LAST QUOTES FETCHED STAY ON THE CARD.", true);
  } else if (ok != mk::kAssets || std::strncmp(report_, "ALL", 3) != 0) {
    // Say which source it was; what did arrive is on the page behind this.
    showStatus(ok == 0 ? "NO ANSWER" : "NOT ALL ANSWERED", report_, true);
  }
}

void MarketsActivity::showStatus(const char* headline, const char* detail, const bool dismissable) {
  if (view_ != View::Status) statusBack_ = view_;
  statusHeadline_ = headline;
  statusDetail_ = detail;
  statusDismissable_ = dismissable;
  view_ = View::Status;
}

void MarketsActivity::loop() {
  if (step_ == Step::Ready && job_ == Job::Search) {
    step_ = Step::Running;
    statusHeadline_ = "SEARCHING";
    statusDetail_ = query_;
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running && job_ == Job::Search) {
    step_ = Step::Idle;
    job_ = Job::Refresh;
    {
      autorefresh::Radio radio;
      if (radio.online()) {
        runSearch();
      } else {
        view_ = statusBack_;
        showStatus("NO WI-FI", "COULD NOT JOIN THE SAVED NETWORK.", true);
      }
    }
    quiet_.touched();
    requestUpdate();
    return;
  }
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
    int ok = -1;
    {
      autorefresh::Radio radio;
      if (radio.online()) ok = fetchAll();
    }
    lastFetchMs_ = static_cast<uint32_t>(millis());
    quiet_.touched();
    view_ = statusBack_;
    reportRound(ok);
    requestUpdate();
    return;
  }

  if (behindDue_ && step_ == Step::Idle) {
    if (!behindPainted_) {
      // The tag has to be on the panel before the radio blocks.
      behindPainted_ = true;
      requestUpdateAndWait();
      return;
    }
    wifiActivated_ = true;
    int ok = -1;
    {
      autorefresh::Radio radio;
      if (radio.online()) ok = fetchAll();
    }
    behindDue_ = false;
    behindPainted_ = false;
    lastFetchMs_ = static_cast<uint32_t>(millis());
    quiet_.touched();
    if (behindAsked_) {
      reportRound(ok);
    } else if (ok < 0 && autoMinutes_ > 0) {
      // Nobody asked: the page keeps what it shows and tries again soon.
      autorefresh::retrySoon(lastFetchMs_, static_cast<uint32_t>(autoMinutes_) * 60000u, lastFetchMs_);
    }
    requestUpdate();
    return;
  }

  // Auto-refresh: quietly, with no status page. The radio comes up for the
  // round and goes down after it; in between the page naps.
  quiet_.noteInput(mappedInput);
  if (autoMinutes_ > 0 && step_ == Step::Idle &&
      (view_ == View::Dashboard || view_ == View::Detail || view_ == View::Portfolio)) {
    const uint32_t intervalMs = static_cast<uint32_t>(autoMinutes_) * 60000u;
    const uint32_t now = static_cast<uint32_t>(millis());
    if (now - lastFetchMs_ >= intervalMs) {
      lastFetchMs_ = now;
      if (!wifiActivated_ && !autorefresh::canJoinAlone()) {
        startRefresh();
        requestUpdate();
        return;
      }
      wifiActivated_ = true;
      autorefresh::Radio radio;
      if (!radio.online()) {
        autorefresh::retrySoon(now, intervalMs, lastFetchMs_);
      } else if (fetchAll() > 0) {
        requestUpdate();
      }
      return;
    }
    quiet_.rest(lastFetchMs_, intervalMs);
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
    } else if (view_ == View::Results) {
      view_ = searchFrom_;
    } else if (view_ == View::HoldingPicker || view_ == View::Amount) {
      view_ = View::Portfolio;
    } else {
      view_ = View::Dashboard;
    }
    requestUpdate();
    return;
  }

  // A swipe up or down turns the page on its side and back, as the clock does.
  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  if ((swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) &&
      step_ == Step::Idle) {
    wide_ = !wide_;
    applyOrientation();
    saveState();
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
      if (event.value < 0 || event.value > static_cast<int16_t>(ui::Span::Year)) return;
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
    case ui::ActionPortfolioPage:
      ++portfolioPage_;
      break;
    case ui::ActionCurrency:
      inEuro_ = event.value == 1;
      saveState();
      break;
    case ui::ActionAddPosition:
      if (portfolio_.count >= mk::kMaxPositions) return;
      view_ = View::HoldingPicker;
      break;
    case ui::ActionPick:
      if (view_ != View::CardPicker && view_ != View::HoldingPicker) return;
      pick(event.value, false);
      break;
    case ui::ActionSearch:
      if (view_ != View::CardPicker && view_ != View::HoldingPicker) return;
      askSearch();
      return;
    case ui::ActionFound: {
      if (view_ != View::Results) return;
      int instrument = -1;
      if (event.value == -1) {
        instrument = adopt(query_, "");
      } else if (event.value >= 0 && event.value < foundCount_) {
        instrument = adopt(found_[event.value].symbol, found_[event.value].company);
      }
      if (instrument < 0) {
        view_ = searchFrom_;
        showStatus("NO ROOM",
                   "NINE ADDED INSTRUMENTS ARE ALL IN USE. TAKE ONE OFF A CARD OR OUT OF THE PORTFOLIO FIRST.", true);
        break;
      }
      pick(instrument, true);
      break;
    }
    case ui::ActionPeriod:
      if (view_ != View::Portfolio || event.value < 0 || event.value >= mk::kPeriods) return;
      period_ = static_cast<mk::Period>(event.value);
      saveState();
      reprice();
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
  // The clip is in logical coordinates and whoever drew last left theirs.
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
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

  sayUpdated();
  switch (view_) {
    case View::Dashboard: {
      marketsui::DashboardModel m;
      m.quotes = quotes_;
      m.autoMinutes = autoMinutes_;
      m.updated = updated_;
      m.portfolio = &portfolio_;
      m.priced = priced_;
      m.period = period_;
      m.euro = euro_;
      m.inEuro = inEuro_;
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
      m.inEuro = inEuro_;
      m.period = period_;
      m.faces = faces;
      m.updated = updated_;
      m.page = portfolioPage_;
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
    case View::Results: {
      marketsui::ResultsModel m;
      m.query = query_;
      m.found = found_;
      m.count = foundCount_;
      m.asTyped = mk::tickerLike(query_);
      marketsui::buildResults(surface, m);
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
  // On its side the dashboard is the whole panel: no hints under it.
  if (!(wide_ && view_ == View::Dashboard)) {
    const auto labels = mappedInput.mapLabels("Back", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  if (behindDue_) autorefresh::drawUpdatingMark(renderer);
  renderer.displayBuffer();
  quiet_.paintPending = false;
}
