#include "MarketsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
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

namespace mk = markets;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/markets.sav";
constexpr char kCacheDir[] = "/.crosspoint/markets";
constexpr uint8_t kSaveVersion = 1;
constexpr uint8_t kCacheVersion = 2;    // 1 kept the source's raw answer
constexpr size_t kMaxBody = 64 * 1024;  // 300 daily candles are ~22 KB
// CNBC's endpoints are written for a browser; the others take anything.
constexpr char kBrowserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36";

void cachePath(const mk::Asset asset, const char* ext, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%s.%s", kCacheDir, mk::infoOf(asset).file, ext);
}

// A GET into memory, by way of a file on the card: the downloader that takes
// request headers writes to one. Leaves the HTTP status (0 when nothing
// answered) in `status`.
bool get(const char* url, std::unique_ptr<char[]>& body, size_t& size, int& status) {
  char staged[64];
  std::snprintf(staged, sizeof(staged), "%s/answer.tmp", kCacheDir);
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kBrowserAgent}, {"Accept", "application/json"}};
  const auto result = HttpDownloader::downloadToFile(url, staged, nullptr, nullptr, "", "", headers);
  status = HttpDownloader::lastStatus();
  if (result != HttpDownloader::OK) {
    LOG_ERR("MKTS", "fetch failed, status %d: %s", status, url);
    return false;
  }
  bool ok = false;
  {
    HalFile file;
    if (Storage.openFileForRead("MKTS", staged, file)) {
      size = file.fileSize();
      if (size > 0 && size <= kMaxBody) {
        body = makeUniqueNoThrow<char[]>(size);
        if (!body) {
          LOG_ERR("MKTS", "OOM: %u bytes for an answer", static_cast<unsigned>(size));
        } else {
          ok = file.read(body.get(), size) == static_cast<int>(size);
        }
      }
    }
  }
  Storage.remove(staged);
  return ok;
}

// "200", or what went wrong in a word, for the line shown when nothing came.
void outcome(const bool parsed, const int status, char* out, const size_t cap) {
  if (parsed)
    std::snprintf(out, cap, "OK");
  else if (status == 0)
    std::snprintf(out, cap, "NO LINK");
  else if (status == 200)
    std::snprintf(out, cap, "UNREADABLE");
  else
    std::snprintf(out, cap, "HTTP %d", status % 1000);
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
  cachePath(asset, "q", path, sizeof(path));
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("MKTS", path, file)) return false;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kCacheVersion) return false;
  auto fresh = makeUniqueNoThrow<mk::Quote>();
  if (!fresh) {
    LOG_ERR("MKTS", "OOM: a quote");
    return false;
  }
  if (file.read(reinterpret_cast<uint8_t*>(fresh.get()), sizeof(mk::Quote)) != sizeof(mk::Quote)) return false;
  if (!fresh->valid || fresh->count < 2 || fresh->count > mk::kMaxCloses) return false;
  fresh->currency[sizeof(fresh->currency) - 1] = 0;
  fresh->zone[sizeof(fresh->zone) - 1] = 0;
  quotes_[static_cast<int>(asset)] = *fresh;
  return true;
}

void MarketsActivity::saveCached(const mk::Asset asset) {
  char path[64];
  cachePath(asset, "q", path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("MKTS", path, file)) return;
  file.write(&kCacheVersion, 1);
  file.write(reinterpret_cast<const uint8_t*>(&quotes_[static_cast<int>(asset)]), sizeof(mk::Quote));
  file.flush();
}

// Bitcoin from Coinbase's candles, then each index from CNBC's bars, then one
// CNBC quote laid over both. A quote replaces the one held only once it has
// parsed, so an error page never costs a good one. Returns how many arrived.
int MarketsActivity::fetchAll() {
  Storage.ensureDirectoryExists(kCacheDir);
  int ok = 0;
  char url[256];
  std::unique_ptr<char[]> body;
  size_t size = 0;
  int status = 0;
  char words[3][12];
  auto fresh = makeUniqueNoThrow<mk::Quote>();
  if (!fresh) {
    LOG_ERR("MKTS", "OOM: a quote");
    return 0;
  }

  mk::coinbaseUrl(url, sizeof(url));
  bool parsed = get(url, body, size, status) && mk::parseCoinbase(body.get(), size, *fresh);
  outcome(parsed, status, words[0], sizeof(words[0]));
  // Today, for the bars' date range: the device's clock when it has been set,
  // and otherwise the newest candle's day.
  const time_t clock = time(nullptr);
  int32_t today = clock > 1700000000 ? static_cast<int32_t>(clock / 86400) : 0;
  if (parsed) {
    if (clock > 1700000000) fresh->time = static_cast<int64_t>(clock);
    if (today == 0) today = fresh->days[fresh->count - 1];
    quotes_[0] = *fresh;
    saveCached(mk::Asset::Bitcoin);
    ++ok;
  }
  body.reset();

  bool haveIndex[mk::kAssets] = {};
  for (int i = 1; i < mk::kAssets; ++i) {
    const mk::Asset asset = static_cast<mk::Asset>(i);
    if (today == 0) {
      std::snprintf(words[i], sizeof(words[i]), "NO DATE");
      continue;
    }
    mk::cnbcBarsUrl(asset, today, url, sizeof(url));
    parsed = get(url, body, size, status) && mk::parseCnbcBars(body.get(), size, *fresh);
    outcome(parsed, status, words[i], sizeof(words[i]));
    body.reset();
    if (!parsed) continue;
    quotes_[i] = *fresh;
    haveIndex[i] = true;
    ++ok;
  }
  if (haveIndex[1] || haveIndex[2]) {
    mk::cnbcQuoteUrl(url, sizeof(url));
    if (get(url, body, size, status)) {
      for (int i = 1; i < mk::kAssets; ++i) {
        if (haveIndex[i]) mk::applyCnbcQuote(body.get(), size, static_cast<mk::Asset>(i), quotes_[i]);
      }
    }
    for (int i = 1; i < mk::kAssets; ++i) {
      if (haveIndex[i]) saveCached(static_cast<mk::Asset>(i));
    }
  }
  std::snprintf(report_, sizeof(report_), "COINBASE: %s.  CNBC S&P: %s.  CNBC NASDAQ: %s.", words[0], words[1],
                words[2]);
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
    if (ok == mk::kAssets) {
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
