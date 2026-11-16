#include "MarketsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
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

namespace mk = markets;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/markets.sav";
constexpr char kCacheDir[] = "/.crosspoint/markets";
constexpr uint8_t kSaveVersion = 1;
constexpr size_t kMaxBody = 64 * 1024;  // three months of daily bars is ~15 KB
// The endpoint answers a browser and turns a bare client away.
constexpr char kBrowserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36";

void cachePath(const mk::Asset asset, const char* ext, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%s.%s", kCacheDir, mk::infoOf(asset).file, ext);
}

// Reads a whole answer off the card and parses it. False when there is none,
// or it is not a quote.
bool readQuote(const char* path, mk::Quote& out) {
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("MKTS", path, file)) return false;
  const size_t size = file.fileSize();
  if (size == 0 || size > kMaxBody) return false;
  auto body = makeUniqueNoThrow<char[]>(size);
  if (!body) {
    LOG_ERR("MKTS", "OOM: %u bytes for a quote", static_cast<unsigned>(size));
    return false;
  }
  if (file.read(body.get(), size) != static_cast<int>(size)) return false;
  return mk::parseQuote(body.get(), size, out);
}

}  // namespace

std::unique_ptr<Activity> MarketsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MarketsActivity>(renderer, mappedInput);
}

void MarketsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  for (int i = 0; i < mk::kAssets; ++i) loadCached(static_cast<mk::Asset>(i));
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
  char path[64];
  cachePath(asset, "json", path, sizeof(path));
  auto fresh = makeUniqueNoThrow<mk::Quote>();
  if (!fresh || !readQuote(path, *fresh)) return false;
  quotes_[static_cast<int>(asset)] = *fresh;
  return true;
}

// One instrument: downloaded beside its cache file, and moved over it only
// once it has parsed, so an error page never replaces a good quote.
bool MarketsActivity::fetch(const mk::Asset asset) {
  char url[192];
  mk::quoteUrl(asset, url, sizeof(url));
  char staged[64];
  char path[64];
  cachePath(asset, "new", staged, sizeof(staged));
  cachePath(asset, "json", path, sizeof(path));
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kBrowserAgent}, {"Accept", "application/json"}};
  const auto result = HttpDownloader::downloadToFile(url, staged, nullptr, nullptr, "", "", headers);
  if (result != HttpDownloader::OK) {
    LOG_ERR("MKTS", "%s: fetch failed, status %d", mk::infoOf(asset).name, HttpDownloader::lastStatus());
    return false;
  }
  auto fresh = makeUniqueNoThrow<mk::Quote>();
  if (!fresh || !readQuote(staged, *fresh)) {
    LOG_ERR("MKTS", "%s: answer is not a quote", mk::infoOf(asset).name);
    Storage.remove(staged);
    return false;
  }
  quotes_[static_cast<int>(asset)] = *fresh;
  Storage.remove(path);
  Storage.rename(staged, path);
  return true;
}

int MarketsActivity::fetchAll() {
  Storage.ensureDirectoryExists(kCacheDir);
  int ok = 0;
  for (int i = 0; i < mk::kAssets; ++i) {
    if (fetch(static_cast<mk::Asset>(i))) ++ok;
  }
  lastFetchMs_ = static_cast<uint32_t>(millis());
  return ok;
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
    statusDetail_ = "BITCOIN, S&P 500, NASDAQ";
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    step_ = Step::Idle;
    const int ok = fetchAll();
    if (ok == 0) {
      showStatus("NO ANSWER", "YAHOO FINANCE DID NOT ANSWER. CHECK THE WI-FI AND TRY AGAIN IN A WHILE.", true);
    } else {
      view_ = statusBack_;
    }
    requestUpdate();
    return;
  }

  // Auto-refresh: quietly, with no status page, and only when already online.
  // The radio reconnects by itself after a drop; a round that finds it down
  // is skipped and the next one tries again.
  if (autoMinutes_ > 0 && step_ == Step::Idle && view_ != View::Status && view_ != View::AutoPicker) {
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
    view_ = view_ == View::Status ? statusBack_ : View::Dashboard;
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
