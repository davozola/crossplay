#include "NavalActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
#include "../meteo/WeatherSave.h"
#include "../meteo/WeatherScreens.h"
#include "NavalPaths.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace nv = naval;

namespace {

using naval::kCacheDir;
constexpr size_t kMaxBody = 48 * 1024;  // the marine answer is ~14 KB
constexpr char kAutoPath[] = "/.crosspoint/naval-auto.cfg";
constexpr char kSourcePath[] = "/.crosspoint/naval-source.cfg";
// The owner's meteoblue key, as text. It can be typed in the app or the file
// can be put here by hand.
constexpr char kKeyPath[] = "/.crosspoint/naval/meteoblue.key";
// Each meteoblue answer spends the owner's credits, so a page left on auto
// asks it again only this often.
constexpr uint32_t kMeteoblueRestMs = 3u * 60u * 60u * 1000u;

enum : fui::ActionId {
  ActionPlace = 1,  // value: 0 the one before, 1 the one after
  ActionRefresh,
  ActionDay,      // value: day index
  ActionStepDay,  // value: 0 the day before, 1 the day after
  ActionDismiss,
  ActionAuto,
  ActionSource,
  ActionChooseSource,  // value: 0 Open-Meteo, 1 meteoblue
  ActionEnterKey,
  ActionRemoveKey,
  ActionWindow,  // value: 1 the sailing hours, 0 the whole day
};

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

constexpr const char* kWeekdays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
constexpr const char* kMonths[13] = {"",    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// Even-odd scanline fill, in whole pixels.
void polygon(toybox::Screen& screen, const float* xs, const float* ys, const int n, const fui::Paint& paint) {
  float top = ys[0];
  float bottom = ys[0];
  for (int i = 1; i < n; ++i) {
    if (ys[i] < top) top = ys[i];
    if (ys[i] > bottom) bottom = ys[i];
  }
  float cross[8];
  for (int y = static_cast<int>(std::floor(top)); y <= static_cast<int>(std::ceil(bottom)); ++y) {
    const float sy = static_cast<float>(y) + 0.5f;
    int count = 0;
    for (int i = 0, j = n - 1; i < n && count < 8; j = i++) {
      if ((ys[i] <= sy && ys[j] > sy) || (ys[j] <= sy && ys[i] > sy)) {
        cross[count++] = xs[i] + (sy - ys[i]) * (xs[j] - xs[i]) / (ys[j] - ys[i]);
      }
    }
    for (int a = 1; a < count; ++a) {
      for (int b = a; b > 0 && cross[b - 1] > cross[b]; --b) {
        const float t = cross[b];
        cross[b] = cross[b - 1];
        cross[b - 1] = t;
      }
    }
    for (int k = 0; k + 1 < count; k += 2) {
      const int x0 = static_cast<int>(std::lround(cross[k]));
      const int x1 = static_cast<int>(std::lround(cross[k + 1]));
      if (x1 > x0) lab::box(screen, x0, y, x1 - x0, 1, paint);
    }
  }
}

// An arrow through (cx, cy) flying the way something that comes FROM `from`
// travels: wind from the north points down the page.
void arrow(toybox::Screen& screen, const int cx, const int cy, const int length, const int from,
           const fui::Paint& paint) {
  const float a = static_cast<float>(from + 180) * 3.14159265f / 180.0f;
  const float dx = std::sin(a);
  const float dy = -std::cos(a);
  const float half = static_cast<float>(length) / 2;
  const float head = static_cast<float>(length) * 0.42f;
  const float wide = static_cast<float>(length) * 0.26f;
  const float thin = static_cast<float>(length) * 0.08f + 1;
  // Along (u) and across (v) the arrow: tip, the head's two barbs, the shaft.
  const float us[7] = {half, half - head, half - head, -half, -half, half - head, half - head};
  const float vs[7] = {0, wide, thin, thin, -thin, -thin, -wide};
  float xs[7];
  float ys[7];
  for (int i = 0; i < 7; ++i) {
    xs[i] = static_cast<float>(cx) + us[i] * dx - vs[i] * dy;
    ys[i] = static_cast<float>(cy) + us[i] * dy + vs[i] * dx;
  }
  polygon(screen, xs, ys, 7, paint);
}

void knots(const float v, char* out, const size_t cap) {
  if (v < 0)
    std::snprintf(out, cap, "--");
  else
    std::snprintf(out, cap, "%d", static_cast<int>(std::lround(v)));
}

void metres(const float v, char* out, const size_t cap) {
  if (v < 0)
    std::snprintf(out, cap, "--");
  else
    std::snprintf(out, cap, "%d.%d", static_cast<int>(std::lround(v * 10)) / 10,
                  static_cast<int>(std::lround(v * 10)) % 10);
}

// The small cut here is a size up from the fork's usual one: this is read at
// arm's length, on a boat.
const toybox::CutMetrics& kCap = toybox::kButtonCut;
// The hours somebody is out sailing, both included.
constexpr int kDayFrom = 8;
constexpr int kDayTo = 21;
constexpr int kDaysShown = 7;
constexpr int kWideLayouts = 3;
constexpr char kViewPath[] = "/.crosspoint/naval-view.cfg";

void cap(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
         const fui::TextAlign align = fui::TextAlign::Left, const fui::Color color = fui::Color::Black) {
  lab::label(screen, lab::rect(x, y, w, kCap.inkHeight), text, toybox::kTileFont, kCap, align, color);
}

void para(toybox::Screen& screen, const int x, const int y, const int w, const char* text, const int lines) {
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Left;
  s.maxLines = static_cast<uint8_t>(lines);
  screen.target().text(lab::rect(x, y, w, kCap.lineHeight * lines), text, s);
}

// A titled cell: a caption, a value in the body cut and a line of detail.
void cell(toybox::Screen& screen, const int x, const int y, const int w, const char* title, const char* value,
          const char* detail) {
  cap(screen, x, y, w, title, fui::TextAlign::Left, fui::Color::DarkGray);
  lab::value(screen, x, y + kCap.inkHeight + 8, w, value);
  cap(screen, x, y + kCap.inkHeight + 8 + toybox::kUiCut.inkHeight + 8, w, detail);
}

}  // namespace

std::unique_ptr<Activity> NavalActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<NavalActivity>(renderer, mappedInput);
}

void NavalActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  Storage.mkdir(kCacheDir);
  // One allocation for the life of the activity: the week of hours is ~8 KB.
  sea_ = makeUniqueNoThrow<nv::Sea>();
  if (!sea_) {
    LOG_ERR("NAVL", "OOM: %u bytes for the forecast", static_cast<unsigned>(sizeof(nv::Sea)));
    shelf::leave(renderer, mappedInput);
    return;
  }
  loadPlaces();
  if (placeCount_ == 0) {
    showStatus("NO PLACES YET", "THIS READS THE PLACES KEPT IN WEATHER. ADD A PORT OR A BEACH THERE AND COME BACK.",
               false);
  } else {
    loadSource();
    loadView();
    haveSea_ = loadCache();
  }
  loadAuto();
  lastFetchMs_ = static_cast<uint32_t>(millis());
  quiet_.touched();
  requestUpdate();
  // Left on auto, it comes back on auto, with a fetch straight away.
  if (autoMinutes_ > 0 && placeCount_ > 0) startFetch(false);
}

void NavalActivity::onExit() {
  // The orientation and the clip are the renderer's: hand them back upright.
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, 32767, 32767);  // open, as the renderer starts: a screen-sized clip set upright cuts a page drawn wide
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

void NavalActivity::loadPlaces() {
  placeCount_ = 0;
  selected_ = 0;
  if (!Storage.exists(meteo::kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("NAVL", meteo::kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != meteo::kSaveVersion) return;
  auto s = makeUniqueNoThrow<meteo::SaveState>();
  if (!s) {
    LOG_ERR("NAVL", "OOM: places");
    return;
  }
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(meteo::SaveState)) != sizeof(meteo::SaveState)) return;
  placeCount_ = s->count <= meteo::kMaxPlaces ? s->count : 0;
  for (int i = 0; i < placeCount_; ++i) {
    places_[i] = s->places[i];
    places_[i].name[meteo::kNameChars - 1] = 0;
    places_[i].region[meteo::kRegionChars - 1] = 0;
  }
  selected_ = s->selected < placeCount_ ? s->selected : 0;
}

void NavalActivity::cachePath(const char* which, char* out, const size_t cap) const {
  const meteo::Place& p = places_[selected_];
  naval::cachePathOf(p.lat, p.lon, which, out, cap);
}

bool NavalActivity::readFile(const char* path, std::unique_ptr<char[]>& body, size_t& size) const {
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("NAVL", path, file)) return false;
  size = file.fileSize();
  if (size == 0 || size > kMaxBody) return false;
  body = makeUniqueNoThrow<char[]>(size);
  if (!body) {
    LOG_ERR("NAVL", "OOM: %u bytes for cache", static_cast<unsigned>(size));
    return false;
  }
  return file.read(body.get(), size) == static_cast<int>(size);
}

bool NavalActivity::loadCache() {
  char path[80];
  std::unique_ptr<char[]> body;
  size_t size = 0;
  cachePath("sea", path, sizeof(path));
  if (!readFile(path, body, size) || !nv::parseMarine(body.get(), size, *sea_)) {
    *sea_ = nv::Sea{};
    return false;
  }
  body.reset();
  cachePath("wind", path, sizeof(path));
  if (readFile(path, body, size)) nv::parseWind(body.get(), size, *sea_);
  if (useMeteoblue_) {
    body.reset();
    cachePath("mbwind", path, sizeof(path));
    if (readFile(path, body, size)) nv::parseMeteoblueWind(body.get(), size, *sea_);
  }
  return true;
}

void NavalActivity::showStatus(const char* headline, const char* detail, const bool dismissable) {
  statusHeadline_ = headline;
  statusDetail_ = detail;
  statusDismissable_ = dismissable;
  view_ = View::Status;
}

void NavalActivity::startFetch(const bool asked) {
  // With a forecast showing and a network to join alone, the page stays: the
  // join and the fetch happen behind it.
  if (haveSea_ && (view_ == View::Now || view_ == View::Day) && step_ == Step::Idle &&
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
  {
    // The saved network is joined behind the page; the picker is for when there is none.
    ActivityResultHandler afterWifi = [this](const ActivityResult& result) {
      if (result.isCancelled) {
        step_ = Step::Idle;
        showStatus("NO WI-FI", "THE FORECAST NEEDS A NETWORK. THE LAST ONE FETCHED STAYS HERE.", true);
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

NavalActivity::Fetched NavalActivity::fetchSea(const bool asked) {
  const meteo::Place& p = places_[selected_];
  char url[512];
  char path[80];
  std::string body;
  body.reserve(16 * 1024);
  nv::marineUrl(p.lat, p.lon, url, sizeof(url));
  if (!HttpDownloader::fetchUrl(url, body) || body.size() > kMaxBody) {
    LOG_ERR("NAVL", "marine fetch failed: status %d", HttpDownloader::lastStatus());
    return Fetched::NoAnswer;
  }
  auto fresh = makeUniqueNoThrow<nv::Sea>();
  if (!fresh || !nv::parseMarine(body.data(), body.size(), *fresh)) {
    LOG_ERR("NAVL", "no sea in %u bytes", static_cast<unsigned>(body.size()));
    return Fetched::NoSea;
  }
  cachePath("sea", path, sizeof(path));
  {
    HalFile file;
    if (Storage.openFileForWrite("NAVL", path, file)) {
      file.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
      file.flush();
    }
  }
  // The wind is a second answer; the sea still reads without it.
  body.clear();
  nv::windUrl(p.lat, p.lon, url, sizeof(url));
  if (HttpDownloader::fetchUrl(url, body) && body.size() <= kMaxBody &&
      nv::parseWind(body.data(), body.size(), *fresh)) {
    cachePath("wind", path, sizeof(path));
    HalFile file;
    if (Storage.openFileForWrite("NAVL", path, file)) {
      file.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
      file.flush();
    }
  } else {
    LOG_ERR("NAVL", "wind fetch failed: status %d", HttpDownloader::lastStatus());
  }
  // meteoblue's wind over that, when the owner chose it and has a key.
  meteoblueFailed_ = false;
  if (useMeteoblue_ && key_[0]) {
    const uint32_t now = static_cast<uint32_t>(millis());
    const bool rested =
        asked || meteobluePlace_ != selected_ || meteoblueAtMs_ == 0 || now - meteoblueAtMs_ >= kMeteoblueRestMs;
    cachePath("mbwind", path, sizeof(path));
    if (!rested) {
      std::unique_ptr<char[]> kept;
      size_t size = 0;
      if (readFile(path, kept, size)) nv::parseMeteoblueWind(kept.get(), size, *fresh);
    } else {
      body.clear();
      nv::meteoblueUrl(p.lat, p.lon, key_, url, sizeof(url));
      if (HttpDownloader::fetchUrl(url, body) && body.size() <= kMaxBody &&
          nv::parseMeteoblueWind(body.data(), body.size(), *fresh)) {
        HalFile file;
        if (Storage.openFileForWrite("NAVL", path, file)) {
          file.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
          file.flush();
        }
        meteoblueAtMs_ = now == 0 ? 1 : now;
        meteobluePlace_ = selected_;
      } else {
        // The status and nothing else: the URL carries the key.
        meteoblueFailed_ = true;
        meteoblueStatus_ = HttpDownloader::lastStatus();
        LOG_ERR("NAVL", "meteoblue wind failed: status %d, %u bytes", meteoblueStatus_,
                static_cast<unsigned>(body.size()));
      }
    }
  }
  *sea_ = *fresh;
  haveSea_ = true;
  return Fetched::Ok;
}

void NavalActivity::runFetch() {
  step_ = Step::Idle;
  {
    autorefresh::Radio radio;
    if (!radio.online()) {
      showStatus("NO WI-FI", "COULD NOT JOIN THE SAVED NETWORK. THE LAST FORECAST FETCHED STAYS HERE.", true);
    } else {
      switch (fetchSea(true)) {
        case Fetched::Ok:
          view_ = View::Now;
          if (meteoblueFailed_) {
            const bool refused = meteoblueStatus_ == 401 || meteoblueStatus_ == 403;
            showStatus(refused ? "KEY REFUSED" : "NO METEOBLUE",
                       refused ? "METEOBLUE DID NOT TAKE THE KEY, OR IT HAS NO CREDITS LEFT. THE WIND SHOWN IS "
                                 "OPEN-METEO'S. CHECK THE KEY UNDER SOURCE."
                               : "METEOBLUE DID NOT ANSWER. THE WIND SHOWN IS OPEN-METEO'S.",
                       true);
          }
          break;
        case Fetched::NoSea:
          showStatus("NO SEA HERE", "THERE IS NO WAVE FORECAST FOR THIS PLACE. IT IS PROBABLY TOO FAR INLAND.", true);
          break;
        case Fetched::NoAnswer:
          showStatus("NO ANSWER", "OPEN-METEO DID NOT ANSWER. CHECK THE WI-FI AND TRY AGAIN.", true);
          break;
      }
    }
  }
  lastFetchMs_ = static_cast<uint32_t>(millis());
  quiet_.touched();
  requestUpdate();
}

void NavalActivity::loadAuto() {
  autoMinutes_ = 0;
  if (!Storage.exists(kAutoPath)) return;
  HalFile file;
  uint8_t saved = 0;
  if (!Storage.openFileForRead("NAVL", kAutoPath, file) || file.read(&saved, 1) < 1) return;
  for (const int choice : meteoui::kAutoChoices) {
    if (choice == saved) autoMinutes_ = choice;
  }
}

void NavalActivity::saveAuto() {
  HalFile file;
  if (!Storage.openFileForWrite("NAVL", kAutoPath, file)) return;
  const uint8_t saved = static_cast<uint8_t>(autoMinutes_);
  file.write(&saved, 1);
  file.flush();
}

// The week and a day turn; the pages with something to choose stay upright.
bool NavalActivity::onItsSide() const {
  return wide_ && (view_ == View::Now || view_ == View::Day) && haveSea_ && sea_ != nullptr;
}

void NavalActivity::loadView() {
  daylight_ = true;
  wide_ = false;
  wideLayout_ = 1;
  tallLayout_ = 0;
  if (!Storage.exists(kViewPath)) return;
  HalFile file;
  uint8_t saved[5] = {1, 0, 1, 0, 0};
  if (!Storage.openFileForRead("NAVL", kViewPath, file) || file.read(saved, 5) < 2) return;
  if (saved[4] < placeCount_) selected_ = saved[4];
  tallLayout_ = saved[3] < kWideLayouts ? saved[3] : 0;
  daylight_ = saved[0] != 0;
  wide_ = saved[1] == 1;
  wideLayout_ = saved[2] < kWideLayouts ? saved[2] : 1;
}

void NavalActivity::saveView() {
  HalFile file;
  if (!Storage.openFileForWrite("NAVL", kViewPath, file)) return;
  const uint8_t saved[5] = {static_cast<uint8_t>(daylight_ ? 1 : 0), static_cast<uint8_t>(wide_ ? 1 : 0),
                            static_cast<uint8_t>(wideLayout_), static_cast<uint8_t>(tallLayout_),
                            static_cast<uint8_t>(selected_)};
  file.write(saved, 5);
  file.flush();
}

void NavalActivity::loadSource() {
  useMeteoblue_ = false;
  key_[0] = 0;
  {
    HalFile file;
    uint8_t saved = 0;
    if (Storage.exists(kSourcePath) && Storage.openFileForRead("NAVL", kSourcePath, file) &&
        file.read(&saved, 1) == 1) {
      useMeteoblue_ = saved == 1;
    }
  }
  if (!Storage.exists(kKeyPath)) return;
  HalFile file;
  if (!Storage.openFileForRead("NAVL", kKeyPath, file)) return;
  char raw[64] = "";
  const int got = file.read(raw, sizeof(raw) - 1);
  if (got <= 0) return;
  raw[got] = 0;
  if (!nv::tidyKey(raw, key_, sizeof(key_))) key_[0] = 0;
}

void NavalActivity::saveSource() {
  HalFile file;
  if (!Storage.openFileForWrite("NAVL", kSourcePath, file)) return;
  const uint8_t saved = useMeteoblue_ ? 1 : 0;
  file.write(&saved, 1);
  file.flush();
}

void NavalActivity::saveKey() {
  if (!key_[0]) {
    if (Storage.exists(kKeyPath)) Storage.remove(kKeyPath);
    return;
  }
  Storage.mkdir(kCacheDir);
  HalFile file;
  if (!Storage.openFileForWrite("NAVL", kKeyPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(key_), std::strlen(key_));
  file.flush();
}

void NavalActivity::askKey() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "METEOBLUE API KEY", "", 32);
  if (!keyboard) {
    LOG_ERR("NAVL", "OOM: the keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    interactionsReady_ = false;
    if (!result.isCancelled) {
      char typed[40];
      if (nv::tidyKey(std::get<KeyboardResult>(result.data).text.c_str(), typed, sizeof(typed))) {
        std::snprintf(key_, sizeof(key_), "%s", typed);
        saveKey();
        useMeteoblue_ = true;
        saveSource();
        meteoblueAtMs_ = 0;
      }
    }
    view_ = View::Source;
    requestUpdate();
  });
}

void NavalActivity::requestUpdate(const bool immediate) {
  quiet_.paintPending = true;
  Activity::requestUpdate(immediate);
}

void NavalActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = "FETCHING";
    statusDetail_ = placeCount_ ? places_[selected_].name : "";
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    runFetch();
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
    behindDue_ = false;
    behindPainted_ = false;
    if (behindAsked_) {
      // Says what went wrong, and otherwise leaves the page where it was.
      const View was = view_;
      runFetch();
      if (view_ == View::Now) view_ = was;
    } else {
      {
        autorefresh::Radio radio;
        if (radio.online()) fetchSea(false);
      }
      lastFetchMs_ = static_cast<uint32_t>(millis());
      quiet_.touched();
      requestUpdate();
    }
    return;
  }

  // Auto: quietly, with no status page. The radio comes up for the round and
  // goes down after it; in between the page naps.
  quiet_.noteInput(mappedInput);
  if (autoMinutes_ > 0 && step_ == Step::Idle && placeCount_ > 0 && (view_ == View::Now || view_ == View::Day)) {
    const uint32_t intervalMs = static_cast<uint32_t>(autoMinutes_) * 60000u;
    const uint32_t now = static_cast<uint32_t>(millis());
    if (now - lastFetchMs_ >= intervalMs) {
      lastFetchMs_ = now;
      wifiActivated_ = true;
      autorefresh::Radio radio;
      if (!radio.online()) {
        autorefresh::retrySoon(now, intervalMs, lastFetchMs_);
      } else if (fetchSea(false) == Fetched::Ok) {
        requestUpdate();
      }
      return;
    }
    quiet_.rest(lastFetchMs_, intervalMs);
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    if (view_ == View::Now || placeCount_ == 0) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    // Leaving the source page shows the wind of whichever source is now chosen.
    if (view_ == View::Source) haveSea_ = loadCache();
    view_ = View::Now;
    requestUpdate();
    return;
  }

  // A swipe up or down turns the week's page on its side and back.
  // A swipe sideways is the next arrangement of the week, either way up.
  if (step_ == Step::Idle && (view_ == View::Now || view_ == View::Day) && haveSea_) {
    const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
      wide_ = !wide_;
      saveView();
      requestUpdate();
      return;
    }
    if (view_ == View::Now &&
        (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right)) {
      const int stepBy = swipe == MappedInputManager::SwipeDir::Left ? 1 : kWideLayouts - 1;
      int& layout = wide_ ? wideLayout_ : tallLayout_;
      layout = (layout + stepBy) % kWideLayouts;
      saveView();
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
  // The picker is WEATHER's, and so are its actions.
  if (view_ == View::AutoPicker) {
    if (event.action != meteoui::ActionChooseAuto) return;
    int chosen = -1;
    for (const int choice : meteoui::kAutoChoices) {
      if (choice == event.value) chosen = choice;
    }
    if (chosen < 0) return;
    autoMinutes_ = chosen;
    saveAuto();
    view_ = View::Now;
    lastFetchMs_ = static_cast<uint32_t>(millis());
    // Turning it on fetches straight away.
    if (autoMinutes_ > 0 && placeCount_ > 0) {
      startFetch();
    }
    requestUpdate();
    return;
  }
  switch (event.action) {
    case ActionPlace:
      if (placeCount_ < 2) return;
      selected_ = (selected_ + (event.value ? 1 : placeCount_ - 1)) % placeCount_;
      saveView();
      haveSea_ = loadCache();
      break;
    case ActionRefresh:
      if (placeCount_ == 0) return;
      startFetch();
      break;
    case ActionDay:
      if (!haveSea_ || event.value < 0 || event.value >= sea_->dayCount) return;
      dayOpen_ = event.value;
      view_ = View::Day;
      break;
    case ActionStepDay:
      dayOpen_ = (dayOpen_ + (event.value ? 1 : sea_->dayCount - 1)) % (sea_->dayCount ? sea_->dayCount : 1);
      break;
    case ActionDismiss:
      if (view_ == View::Source) haveSea_ = loadCache();
      view_ = View::Now;
      break;
    case ActionAuto:
      view_ = View::AutoPicker;
      break;
    case ActionWindow:
      daylight_ = event.value == 1;
      saveView();
      break;
    case ActionSource:
      view_ = View::Source;
      break;
    case ActionChooseSource:
      if (event.value == 1 && !key_[0]) {
        askKey();
        return;
      }
      useMeteoblue_ = event.value == 1;
      saveSource();
      break;
    case ActionEnterKey:
      askKey();
      return;
    case ActionRemoveKey:
      key_[0] = 0;
      saveKey();
      useMeteoblue_ = false;
      saveSource();
      break;
    default:
      return;
  }
  requestUpdate();
}

// --- the pieces both ways up share -------------------------------------------------

// Wind: a rose with the arrow flying downwind, and the swell's fainter one under it.
void NavalActivity::drawRose(toybox::Screen& screen, const int cx, const int cy, const int r, const nv::Hour& h) {
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r - 3),
               fui::Color::White);
  for (int t = 0; t < 8; ++t) {
    const float ang = static_cast<float>(t) * 0.7853982f;
    const int len = t % 2 == 0 ? 9 : 5;
    const int tx = cx + static_cast<int>(std::sin(ang) * static_cast<float>(r - 3 - len / 2));
    const int ty = cy - static_cast<int>(std::cos(ang) * static_cast<float>(r - 3 - len / 2));
    lab::ink(screen, tx - 2, ty - 2, 4, 4);
  }
  if (h.swell > 0) arrow(screen, cx, cy, r - 6, h.swellFrom, fui::Paint::dither(fui::Color::DarkGray));
  if (h.wind >= 0) arrow(screen, cx, cy, r + 12, h.windFrom, kInk);
}

// 08-21 or the whole day: two halves of one switch.
void NavalActivity::drawWindow(toybox::Screen& screen, const int x, const int y, const int w, const int h) {
  const int half = w / 2;
  lab::button(screen, lab::rect(x, y, half, h), "08-21", ActionWindow, 1, daylight_);
  lab::button(screen, lab::rect(x + half, y, w - half, h), "24 H", ActionWindow, 0, !daylight_);
}

void NavalActivity::autoLabel(char* out, const size_t cap) const {
  if (autoMinutes_ <= 0) {
    std::snprintf(out, cap, "AUTO OFF");
  } else if (autoMinutes_ >= 60) {
    std::snprintf(out, cap, "AUTO %d H", autoMinutes_ / 60);
  } else {
    std::snprintf(out, cap, "AUTO %d M", autoMinutes_);
  }
}

// The hours of a day the page is looking at: its first index and how many.
int NavalActivity::windowOf(const int d, int& first) const {
  int dayFirst = 0;
  const int n = nv::hoursOfDay(*sea_, d, dayFirst);
  first = dayFirst;
  if (!daylight_) return n;
  int count = 0;
  first = -1;
  for (int i = dayFirst; i < dayFirst + n; ++i) {
    const int hour = sea_->hours[i].hour;
    if (hour < kDayFrom || hour > kDayTo) continue;
    if (first < 0) first = i;
    ++count;
  }
  if (first < 0) first = dayFirst;
  return count;
}

nv::DayStats NavalActivity::statsFor(const int d) const {
  return daylight_ ? nv::statsOf(*sea_, d, kDayFrom, kDayTo) : nv::statsOf(*sea_, d);
}

// A day's wind hour by hour in `r`: a bar for the wind inside a taller outline
// for the gusts, to a scale `top` knots high.
void NavalActivity::drawBars(toybox::Screen& screen, const fui::Rect& r, const int d, const float top,
                             const bool fine) {
  int first = 0;
  const int n = windowOf(d, first);
  const int slots = daylight_ ? kDayTo - kDayFrom + 1 : 24;
  const int base = daylight_ ? kDayFrom : 0;
  const int slot = r.width / slots;
  const int x0 = r.x + (r.width - slot * slots) / 2;
  const int gapW = fine ? 4 : 2;
  for (int i = first; i < first + n; ++i) {
    const nv::Hour& h = sea_->hours[i];
    const int bx = x0 + (h.hour - base) * slot + gapW / 2;
    if (h.gust > 0 && fine) {
      const int gh = static_cast<int>(h.gust / top * static_cast<float>(r.height));
      lab::box(screen, bx, r.y + r.height - gh, slot - gapW, gh, kInk, 2);
      lab::box(screen, bx + 1, r.y + r.height - gh + 1, slot - gapW - 2, gh - 1, kPaper, 1);
    }
    if (h.wind > 0) {
      const int wh = static_cast<int>(h.wind / top * static_cast<float>(r.height));
      lab::box(screen, bx, r.y + r.height - wh, slot - gapW, wh, kInk, fine ? 2 : 0);
    }
  }
}

float NavalActivity::scaleOf(const int d) const {
  int first = 0;
  const int n = windowOf(d, first);
  float top = 20;
  for (int i = first; i < first + n; ++i) {
    if (sea_->hours[i].gust > top) top = sea_->hours[i].gust;
    if (sea_->hours[i].wind > top) top = sea_->hours[i].wind;
  }
  return std::ceil(top / 10) * 10;
}

// --- upright --------------------------------------------------------------------

void NavalActivity::buildNow(toybox::Screen& screen) {
  if (wide_ && haveSea_) {
    buildWide(screen);
    return;
  }
  lab::chrome(screen, "NAVAL WEATHER");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  int y = toybox::kChromeHeight + 10;

  // The place, with a step either side when there is more than one.
  const meteo::Place& place = places_[selected_];
  if (placeCount_ > 1) {
    lab::button(screen, lab::rect(x, y, 52, 46), "<", ActionPlace, 0, false, false);
    lab::button(screen, lab::rect(x + inner - 52, y, 52, 46), ">", ActionPlace, 1, false, false);
  }
  lab::value(screen, x + 58, y + (46 - toybox::kUiCut.inkHeight) / 2, inner - 116, place.name, fui::TextAlign::Center);
  y += 46 + 12;

  if (!haveSea_) {
    para(screen, x, y + 40, inner,
         "NOTHING FETCHED FOR THIS PLACE YET. REFRESH WHILE THERE IS WI-FI AND THE WEEK STAYS ON THE CARD.", 4);
    const int half = (inner - 8) / 2;
    lab::button(screen, lab::rect(x, bottom - 46, half, 46), "SOURCE", ActionSource, 0, useMeteoblue_);
    lab::button(screen, lab::rect(x + half + 8, bottom - 46, inner - half - 8, 46), "REFRESH", ActionRefresh, 0);
    return;
  }

  const nv::Sea& sea = *sea_;
  const nv::Hour& h = sea.hours[nv::nowIndex(sea)];
  char a[24];
  char b[48];
  const int listBottom = bottom - 52 - kCap.inkHeight - 10;

  if (tallLayout_ == 1) {
    // The week in big rows, with now as one line over them.
    knots(h.wind, a, sizeof(a));
    cap(screen, x, y + (toybox::kUiCut.inkHeight - kCap.inkHeight), 60, "NOW", fui::TextAlign::Left,
        fui::Color::DarkGray);
    if (h.wind >= 0) {
      std::snprintf(b, sizeof(b), "%s KN %s", a, nv::compass(h.windFrom));
      lab::value(screen, x + 62, y, 220, b);
    }
    metres(h.wave, a, sizeof(a));
    std::snprintf(b, sizeof(b), "%s M", a);
    lab::value(screen, x + inner - 130, y, 130, b, fui::TextAlign::Right);
    y += toybox::kUiCut.inkHeight + 12;
    lab::ink(screen, x, y, inner, toybox::kHairline);
    y += 8;
    const int headH = 34;
    cap(screen, x + 8, y + (headH - kCap.inkHeight) / 2, inner - 190, "WIND MAX, SEA MAX", fui::TextAlign::Left,
        fui::Color::DarkGray);
    drawWindow(screen, x + inner - 176, y, 176, headH);
    y += headH + 6;
    const int rows = sea.dayCount;
    const int rowH = (listBottom - y) / (rows ? rows : 1);
    for (int dIdx = 0; dIdx < rows; ++dIdx) {
      const nv::DayStats s = statsFor(dIdx);
      const int ry = y + dIdx * rowH;
      dayHit(screen, lab::rect(x, ry, inner, rowH), dIdx);
      if (dIdx > 0) lab::box(screen, x, ry, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
      const int cy = ry + (rowH - kCap.inkHeight) / 2;
      std::snprintf(b, sizeof(b), "%s %d", kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day);
      cap(screen, x + 8, cy, 96, dIdx == 0 ? "TODAY" : b);
      if (s.windMax >= 0) {
        knots(s.windMax, a, sizeof(a));
        lab::label(screen, lab::rect(x + 108, ry, 66, rowH), a, toybox::kDisplayFont, toybox::kDisplayCut,
                   fui::TextAlign::Right);
        std::snprintf(b, sizeof(b), "KN F%d", nv::beaufort(s.windMax));
        cap(screen, x + 182, cy, 80, b);
        arrow(screen, x + 280, ry + rowH / 2, 28, s.windFrom, kInk);
        cap(screen, x + 300, cy, 56, nv::compass(s.windFrom));
      }
      metres(s.waveMax, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s M", a);
      lab::label(screen, lab::rect(x + 350, ry, inner - 358, rowH), b, toybox::kUiFont, toybox::kUiCut,
                 fui::TextAlign::Right);
    }
  } else {
    const int roseR = 60;
    drawRose(screen, x + roseR + 12, y + roseR + 2, roseR, h);
    const int tx = x + roseR * 2 + 40;
    const int tw = x + inner - tx;
    cap(screen, tx, y, tw, "WIND NOW", fui::TextAlign::Left, fui::Color::DarkGray);
    knots(h.wind, a, sizeof(a));
    std::snprintf(b, sizeof(b), "%s KN", a);
    lab::label(screen, lab::rect(tx, y + 26, tw, toybox::kDisplayCut.inkHeight), b, toybox::kDisplayFont,
               toybox::kDisplayCut, fui::TextAlign::Left);
    if (h.wind >= 0) {
      const int force = nv::beaufort(h.wind);
      std::snprintf(b, sizeof(b), "F%d %s", force, nv::beaufortName(force));
      cap(screen, tx, y + 74, tw, b);
      knots(h.gust, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s  GUSTS %s", nv::compass(h.windFrom), a);
      cap(screen, tx, y + 100, tw, b);
    } else {
      cap(screen, tx, y + 74, tw, "NO WIND FETCHED");
    }
    y += roseR * 2 + 14;

    if (tallLayout_ == 2) {
      // Today by the hour, and the week as a strip under it.
      lab::ink(screen, x, y, inner, toybox::kHairline);
      y += 8;
      const int headH = 34;
      cap(screen, x + 8, y + (headH - kCap.inkHeight) / 2, inner - 190, "TODAY, KNOTS", fui::TextAlign::Left,
          fui::Color::DarkGray);
      drawWindow(screen, x + inner - 176, y, 176, headH);
      y += headH + 10;
      const int weekH = 92;
      const int plotH = listBottom - weekH - 86 - y;
      dayHit(screen, lab::rect(x, y, inner, plotH + 86), 0);
      const float top = scaleOf(0);
      for (float line = 10; line <= top; line += 10) {
        const int ly = y + plotH - static_cast<int>(line / top * static_cast<float>(plotH));
        lab::box(screen, x, ly, inner, 1, fui::Paint::dither(fui::Color::LightGray));
      }
      drawBars(screen, lab::rect(x, y, inner, plotH), 0, top, true);
      lab::ink(screen, x, y + plotH, inner, 2);
      {
        int first = 0;
        const int n = windowOf(0, first);
        const int slots = daylight_ ? kDayTo - kDayFrom + 1 : 24;
        const int base = daylight_ ? kDayFrom : 0;
        const int slot = inner / slots;
        const int x0 = x + (inner - slot * slots) / 2;
        const int step = daylight_ ? 1 : 3;
        for (int i = first; i < first + n; ++i) {
          const nv::Hour& hr = sea.hours[i];
          if ((hr.hour - base) % step != 0) continue;
          const int sx = x0 + (hr.hour - base) * slot;
          const int sw = slot * step;
          std::snprintf(b, sizeof(b), "%02d", hr.hour);
          cap(screen, sx, y + plotH + 10, sw, b, fui::TextAlign::Center, fui::Color::DarkGray);
          if (hr.wind >= 0) {
            knots(hr.wind, a, sizeof(a));
            cap(screen, sx, y + plotH + 36, sw, a, fui::TextAlign::Center);
            arrow(screen, sx + sw / 2, y + plotH + 70, 20, hr.windFrom, kInk);
          }
        }
      }
      y += plotH + 86;
      lab::ink(screen, x, y, inner, toybox::kHairline);
      const int cellW = inner / kDaysShown;
      for (int dIdx = 0; dIdx < sea.dayCount && dIdx < kDaysShown; ++dIdx) {
        const nv::DayStats s = statsFor(dIdx);
        const int cx = x + dIdx * cellW;
        dayHit(screen, lab::rect(cx, y + 1, cellW, weekH - 1), dIdx);
        if (dIdx > 0) lab::box(screen, cx, y + 10, 1, weekH - 20, fui::Paint::dither(fui::Color::DarkGray));
        int cy = y + 10;
        cap(screen, cx, cy, cellW, kWeekdays[nv::weekday(sea.year, s.month, s.day)], fui::TextAlign::Center,
            dIdx == 0 ? fui::Color::Black : fui::Color::DarkGray);
        cy += kCap.inkHeight + 8;
        knots(s.windMax, a, sizeof(a));
        lab::value(screen, cx, cy, cellW, a, fui::TextAlign::Center);
        cy += toybox::kUiCut.inkHeight + 8;
        metres(s.waveMax, a, sizeof(a));
        cap(screen, cx, cy, cellW, a, fui::TextAlign::Center);
      }
    } else {
      // The sea in three cells.
      lab::ink(screen, x, y, inner, toybox::kHairline);
      y += 12;
      const int cw = inner / 3;
      char d[32];
      metres(h.wave, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s M", a);
      std::snprintf(d, sizeof(d), "%dS %s", static_cast<int>(std::lround(h.wavePeriod < 0 ? 0 : h.wavePeriod)),
                    nv::compass(h.waveFrom));
      cell(screen, x, y, cw, "WAVES", b, d);
      metres(h.swell, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s M", a);
      std::snprintf(d, sizeof(d), "%dS %s", static_cast<int>(std::lround(h.swellPeriod < 0 ? 0 : h.swellPeriod)),
                    nv::compass(h.swellFrom));
      cell(screen, x + cw, y, cw, "SWELL", b, d);
      if (h.water > -50)
        std::snprintf(b, sizeof(b), "%d C", static_cast<int>(std::lround(h.water)));
      else
        std::snprintf(b, sizeof(b), "--");
      cell(screen, x + cw * 2, y, cw, "WATER", b, h.wave >= 0 ? nv::seaStateName(nv::seaState(h.wave)) : "");
      y += kCap.inkHeight * 2 + toybox::kUiCut.inkHeight + 16 + 12;

      // The week: one row a day, the windiest hour and the highest sea. Tap for the hours.
      lab::ink(screen, x, y, inner, toybox::kHairline);
      y += 8;
      const int colDay = x + 8;
      const int colWind = x + 104;
      const int colDir = x + 226;
      const int colSea = x + 322;
      const int headH = 34;
      cap(screen, colDay, y + (headH - kCap.inkHeight) / 2, inner - 190, "WIND MAX, SEA MAX", fui::TextAlign::Left,
          fui::Color::DarkGray);
      drawWindow(screen, x + inner - 176, y, 176, headH);
      y += headH + 6;
      const int rows = sea.dayCount;
      const int room = listBottom - y;
      const int rowH = room / (rows ? rows : 1) > 44 ? 44 : room / (rows ? rows : 1);
      for (int dIdx = 0; dIdx < rows; ++dIdx) {
        const nv::DayStats s = statsFor(dIdx);
        const int ry = y + dIdx * rowH;
        fui::ButtonProps row;
        row.label = "";
        row.action = ActionDay;
        row.value = static_cast<int16_t>(dIdx);
        row.styles = toybox::rowStyles();
        row.radius = lab::kRadius;
        screen.button(row, lab::rect(x, ry, inner, rowH - 4));
        const int ty = ry + (rowH - 4 - kCap.inkHeight) / 2;
        std::snprintf(b, sizeof(b), "%s %d", kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day);
        cap(screen, colDay, ty, 96, dIdx == 0 ? "TODAY" : b);
        if (s.windMax >= 0) {
          knots(s.windMax, a, sizeof(a));
          std::snprintf(b, sizeof(b), "%s KN F%d", a, nv::beaufort(s.windMax));
          cap(screen, colWind, ty, 120, b);
          arrow(screen, colDir + 12, ry + (rowH - 4) / 2, 24, s.windFrom, kInk);
          cap(screen, colDir + 32, ty, 60, nv::compass(s.windFrom));
        }
        metres(s.waveMax, a, sizeof(a));
        std::snprintf(b, sizeof(b), "%s M", a);
        cap(screen, colSea, ty, x + inner - colSea - 10, b, fui::TextAlign::Right);
      }
    }
  }

  // When it was fetched, by the place's own clock, whose wind it is, and the way to fetch again.
  std::snprintf(b, sizeof(b), "%.5s  %d %s", sea.now[0] ? sea.now + 11 : "--:--", h.day,
                kMonths[h.month > 12 ? 0 : h.month]);
  const int noteY = bottom - 46 - kCap.inkHeight - 8;
  cap(screen, x, noteY, inner, b, fui::TextAlign::Left, fui::Color::DarkGray);
  cap(screen, x, noteY, inner, sea.windIsMeteoblue ? "METEOBLUE" : "ECMWF", fui::TextAlign::Right,
      fui::Color::DarkGray);
  autoLabel(a, sizeof(a));
  const int gap = 8;
  const int third = (inner - gap * 2) / 3;
  lab::button(screen, lab::rect(x, bottom - 46, third, 46), "SOURCE", ActionSource, 0, useMeteoblue_);
  lab::button(screen, lab::rect(x + third + gap, bottom - 46, third, 46), a, ActionAuto, 0, autoMinutes_ > 0);
  lab::button(screen, lab::rect(x + (third + gap) * 2, bottom - 46, inner - (third + gap) * 2, 46), "REFRESH",
              ActionRefresh, 0);
}

// --- on its side ------------------------------------------------------------------
//
// The whole 800x480, with no band and no hints: what the buttons said is a
// strip of words along the foot. Three arrangements of the same week, one
// swipe sideways apart: a table, seven columns, and today by the hour.

void NavalActivity::wideStrip(toybox::Screen& screen, const int y, const int h) {
  const int width = screen.device().width;
  // <, the place, >, the hours, the source, auto, refresh.
  const int ws[7] = {46, 190, 46, 112, 130, 130, width - 654};
  const fui::ActionId actions[7] = {ActionPlace,  ActionPlace, ActionPlace,  ActionWindow,
                                    ActionSource, ActionAuto,  ActionRefresh};
  const int values[7] = {0, 1, 1, daylight_ ? 0 : 1, 0, 0, 0};
  int x = 0;
  for (int i = 0; i < 7; ++i) {
    fui::ButtonProps b;
    b.label = "";
    b.action = actions[i];
    b.value = static_cast<int16_t>(values[i]);
    b.styles = toybox::rowStyles();
    b.styles.normal.borderWidth = 0;
    b.minTouchSize = 0;
    screen.button(b, lab::rect(x, y, ws[i], h));
    x += ws[i];
  }
  lab::ink(screen, 0, y, width, h);
  char autoText[24];
  autoLabel(autoText, sizeof(autoText));
  const char* words[7] = {placeCount_ > 1 ? "<" : "",
                          places_[selected_].name,
                          placeCount_ > 1 ? ">" : "",
                          daylight_ ? "08-21" : "24 H",
                          sea_->windIsMeteoblue ? "METEOBLUE" : "ECMWF",
                          autoText,
                          "REFRESH"};
  const int ty = y + (h - kCap.inkHeight) / 2;
  x = 0;
  for (int i = 0; i < 7; ++i) {
    cap(screen, x + 4, ty, ws[i] - 8, words[i], i == 6 ? fui::TextAlign::Right : fui::TextAlign::Center,
        fui::Color::White);
    if (i > 2 && i < 7) lab::box(screen, x, y + 10, 1, h - 20, kPaper);
    x += ws[i];
  }
}

// A name on the left in grey and its value on the right in the body cut.
void NavalActivity::wideFact(toybox::Screen& screen, const int x, const int y, const int w, const char* name,
                             const char* value, const char* detail) {
  cap(screen, x, y + (toybox::kUiCut.inkHeight - kCap.inkHeight), 80, name, fui::TextAlign::Left, fui::Color::DarkGray);
  lab::value(screen, x + 82, y, 92, value);
  cap(screen, x + 172, y + (toybox::kUiCut.inkHeight - kCap.inkHeight), w - 172, detail, fui::TextAlign::Right);
}

void NavalActivity::wideFacts(toybox::Screen& screen, const int x, int y, const int w, const int pitch,
                              const nv::Hour& h) {
  char a[24];
  char b[32];
  char d[32];
  metres(h.wave, a, sizeof(a));
  std::snprintf(b, sizeof(b), "%s M", a);
  std::snprintf(d, sizeof(d), "%dS %s", static_cast<int>(std::lround(h.wavePeriod < 0 ? 0 : h.wavePeriod)),
                nv::compass(h.waveFrom));
  wideFact(screen, x, y, w, "WAVES", b, d);
  y += pitch;
  metres(h.swell, a, sizeof(a));
  std::snprintf(b, sizeof(b), "%s M", a);
  std::snprintf(d, sizeof(d), "%dS %s", static_cast<int>(std::lround(h.swellPeriod < 0 ? 0 : h.swellPeriod)),
                nv::compass(h.swellFrom));
  wideFact(screen, x, y, w, "SWELL", b, d);
  y += pitch;
  if (h.water > -50)
    std::snprintf(b, sizeof(b), "%d C", static_cast<int>(std::lround(h.water)));
  else
    std::snprintf(b, sizeof(b), "--");
  wideFact(screen, x, y, w, "WATER", b, h.wave >= 0 ? nv::seaStateName(nv::seaState(h.wave)) : "");
}

// The wind now: the number in the big cut with what it means beside it. In a
// narrow column (`underW` > 0) the direction and gusts go on a line of their
// own at `underX`, `underY`.
void NavalActivity::wideWind(toybox::Screen& screen, const int x, const int y, const int w, const nv::Hour& h,
                             const int underX, const int underY, const int underW) {
  char a[24];
  char b[48];
  knots(h.wind, a, sizeof(a));
  lab::label(screen, lab::rect(x, y, 130, toybox::kLargeCut.inkHeight), a, toybox::kDisplayFont, toybox::kLargeCut,
             fui::TextAlign::Left);
  if (h.wind < 0) return;
  fui::TextStyle s;
  s.font = toybox::kDisplayFont;
  const int tx = x + screen.target().measureText(toybox::kDisplayFont, a, s).width + 12;
  knots(h.gust, a, sizeof(a));
  if (underW > 0) {
    lab::value(screen, tx, y, w - (tx - x), "KN");
    std::snprintf(b, sizeof(b), "F%d", nv::beaufort(h.wind));
    lab::value(screen, tx, y + toybox::kLargeCut.inkHeight - toybox::kUiCut.inkHeight, w - (tx - x), b);
    std::snprintf(b, sizeof(b), "FROM %s", nv::compass(h.windFrom));
    cap(screen, underX, underY, underW, b);
    std::snprintf(b, sizeof(b), "GUSTS %s", a);
    cap(screen, underX, underY, underW, b, fui::TextAlign::Right);
    return;
  }
  std::snprintf(b, sizeof(b), "KN  F%d", nv::beaufort(h.wind));
  lab::value(screen, tx, y, w - (tx - x), b);
  std::snprintf(b, sizeof(b), "%s  GUSTS %s", nv::compass(h.windFrom), a);
  cap(screen, tx, y + toybox::kLargeCut.inkHeight - kCap.inkHeight, w - (tx - x), b);
}

void NavalActivity::dayHit(toybox::Screen& screen, const fui::Rect& r, const int d) {
  fui::ButtonProps b;
  b.label = "";
  b.action = ActionDay;
  b.value = static_cast<int16_t>(d);
  b.styles = toybox::rowStyles();
  b.styles.normal.borderWidth = 0;
  b.minTouchSize = 0;
  screen.button(b, r);
  lab::box(screen, r.x, r.y, r.width, r.height, kPaper);
}

void NavalActivity::buildWide(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int height = screen.device().height;
  const int stripH = 42;
  const int bodyH = height - stripH;
  const nv::Sea& sea = *sea_;
  const nv::Hour& h = sea.hours[nv::nowIndex(sea)];
  char a[24];
  char b[48];

  if (wideLayout_ == 0) {
    // Now on the left, the week as a table on the right.
    const int leftW = 318;
    for (int d = 0; d < sea.dayCount; ++d) {
      const int rowH = (bodyH - 40) / (sea.dayCount ? sea.dayCount : 1);
      dayHit(screen, lab::rect(leftW + 1, 40 + d * rowH, width - leftW - 1, rowH), d);
    }
    cap(screen, 18, 16, 200, "WIND NOW", fui::TextAlign::Left, fui::Color::DarkGray);
    drawRose(screen, 84, 112, 60, h);
    wideWind(screen, 166, 84, leftW - 174, h, 18, 190, leftW - 36);
    lab::ink(screen, 18, 224, leftW - 36, toybox::kHairline);
    wideFacts(screen, 18, 246, leftW - 36, 60, h);
    lab::ink(screen, leftW, 0, toybox::kHairline, bodyH);

    const int tx = leftW + 18;
    const int cols[5] = {tx, tx + 104, tx + 220, tx + 272, tx + 356};
    static const char* const kHeads[5] = {"DAY", "WIND", "GUST", "FROM", "SEA"};
    for (int c = 0; c < 5; ++c) cap(screen, cols[c], 14, 120, kHeads[c], fui::TextAlign::Left, fui::Color::DarkGray);
    const int rowH = (bodyH - 40) / (sea.dayCount ? sea.dayCount : 1);
    for (int d = 0; d < sea.dayCount; ++d) {
      const nv::DayStats s = statsFor(d);
      const int ry = 40 + d * rowH;
      lab::box(screen, leftW + 1, ry, width - leftW - 1, 1, fui::Paint::dither(fui::Color::DarkGray));
      const int vy = ry + (rowH - toybox::kUiCut.inkHeight) / 2;
      const int cy = ry + (rowH - kCap.inkHeight) / 2;
      std::snprintf(b, sizeof(b), "%s %d", kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day);
      cap(screen, cols[0], cy, 104, d == 0 ? "TODAY" : b);
      if (s.windMax >= 0) {
        knots(s.windMax, a, sizeof(a));
        lab::value(screen, cols[1], vy, 50, a);
        std::snprintf(b, sizeof(b), "KN F%d", nv::beaufort(s.windMax));
        cap(screen, cols[1] + 46, cy, 76, b);
        knots(s.gustMax, a, sizeof(a));
        cap(screen, cols[2], cy, 60, a);
        arrow(screen, cols[3] + 12, ry + rowH / 2, 28, s.windFrom, kInk);
        cap(screen, cols[3] + 32, cy, 52, nv::compass(s.windFrom));
      }
      metres(s.waveMax, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s M", a);
      lab::value(screen, cols[4], vy, width - cols[4] - 14, b, fui::TextAlign::Right);
    }
  } else if (wideLayout_ == 1) {
    // Now across the top, the week as seven columns under it.
    const int topH = 150;
    const int colW = width / kDaysShown;
    for (int d = 0; d < sea.dayCount && d < kDaysShown; ++d) {
      dayHit(screen, lab::rect(d * colW, topH + 1, colW, bodyH - topH - 1), d);
    }
    drawRose(screen, 76, 76, 58, h);
    cap(screen, 152, 18, 200, "WIND NOW", fui::TextAlign::Left, fui::Color::DarkGray);
    wideWind(screen, 152, 52, 300, h, 0, 0, 0);
    lab::ink(screen, 462, 16, toybox::kHairline, topH - 32);
    wideFacts(screen, 480, 18, width - 480 - 18, 44, h);
    lab::ink(screen, 0, topH, width, toybox::kHairline);
    for (int d = 0; d < sea.dayCount && d < kDaysShown; ++d) {
      const nv::DayStats s = statsFor(d);
      const int cx = d * colW;
      if (d > 0) lab::box(screen, cx, topH + 10, 1, bodyH - topH - 20, fui::Paint::dither(fui::Color::DarkGray));
      int y = topH + 14;
      std::snprintf(b, sizeof(b), "%s %d", kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day);
      cap(screen, cx, y, colW, d == 0 ? "TODAY" : b, fui::TextAlign::Center, fui::Color::DarkGray);
      y += kCap.inkHeight + 14;
      knots(s.windMax, a, sizeof(a));
      lab::label(screen, lab::rect(cx, y, colW, toybox::kLargeCut.inkHeight), a, toybox::kDisplayFont,
                 toybox::kLargeCut, fui::TextAlign::Center);
      y += toybox::kLargeCut.inkHeight + 10;
      if (s.windMax >= 0) {
        std::snprintf(b, sizeof(b), "KN F%d", nv::beaufort(s.windMax));
        cap(screen, cx, y, colW, b, fui::TextAlign::Center);
        y += kCap.inkHeight + 12;
        arrow(screen, cx + colW / 2 - 24, y + 13, 28, s.windFrom, kInk);
        cap(screen, cx + colW / 2 - 4, y + 4, 60, nv::compass(s.windFrom));
        y += 30 + 10;
      } else {
        y += kCap.inkHeight + 12 + 40;
      }
      metres(s.waveMax, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s M", a);
      lab::value(screen, cx, y, colW, b, fui::TextAlign::Center);
      y += toybox::kUiCut.inkHeight + 12;
      drawBars(screen, lab::rect(cx + 10, y, colW - 20, bodyH - 8 - y), d, 40, false);
    }
  } else {
    // Now on the left, today hour by hour on the right, the week as a strip under it.
    const int leftW = 312;
    const int weekH = 104;
    const int plotX = leftW + 22;
    const int plotW = width - plotX - 18;
    dayHit(screen, lab::rect(leftW + 1, 0, width - leftW - 1, bodyH - weekH), 0);
    const int cellW = (width - leftW) / kDaysShown;
    for (int d = 0; d < sea.dayCount && d < kDaysShown; ++d) {
      dayHit(screen, lab::rect(leftW + 1 + d * cellW, bodyH - weekH + 1, cellW, weekH - 1), d);
    }
    cap(screen, 18, 16, 200, "WIND NOW", fui::TextAlign::Left, fui::Color::DarkGray);
    drawRose(screen, 84, 112, 60, h);
    wideWind(screen, 166, 84, leftW - 174, h, 18, 190, leftW - 36);
    lab::ink(screen, 18, 224, leftW - 36, toybox::kHairline);
    wideFacts(screen, 18, 246, leftW - 36, 60, h);
    lab::ink(screen, leftW, 0, toybox::kHairline, bodyH);

    cap(screen, plotX, 16, plotW, daylight_ ? "TODAY 08-21" : "TODAY", fui::TextAlign::Left, fui::Color::DarkGray);
    cap(screen, plotX, 16, plotW, "KNOTS, GUSTS OUTLINED", fui::TextAlign::Right, fui::Color::DarkGray);
    const int plotY = 46;
    const int plotH = bodyH - weekH - plotY - 86;
    const float top = scaleOf(0);
    for (float line = 10; line <= top; line += 10) {
      const int ly = plotY + plotH - static_cast<int>(line / top * static_cast<float>(plotH));
      lab::box(screen, plotX, ly, plotW, 1, fui::Paint::dither(fui::Color::LightGray));
    }
    drawBars(screen, lab::rect(plotX, plotY, plotW, plotH), 0, top, true);
    lab::ink(screen, plotX, plotY + plotH, plotW, 2);
    {
      int first = 0;
      const int n = windowOf(0, first);
      const int slots = daylight_ ? kDayTo - kDayFrom + 1 : 24;
      const int base = daylight_ ? kDayFrom : 0;
      const int slot = plotW / slots;
      const int x0 = plotX + (plotW - slot * slots) / 2;
      const int step = daylight_ ? 1 : 3;
      for (int i = first; i < first + n; ++i) {
        const nv::Hour& hr = sea.hours[i];
        if ((hr.hour - base) % step != 0) continue;
        const int sx = x0 + (hr.hour - base) * slot;
        std::snprintf(b, sizeof(b), "%02d", hr.hour);
        cap(screen, sx, plotY + plotH + 10, slot * step, b, fui::TextAlign::Center, fui::Color::DarkGray);
        if (hr.wind >= 0) {
          knots(hr.wind, a, sizeof(a));
          cap(screen, sx, plotY + plotH + 36, slot * step, a, fui::TextAlign::Center);
          arrow(screen, sx + slot * step / 2, plotY + plotH + 70, 20, hr.windFrom, kInk);
        }
      }
    }
    lab::ink(screen, leftW, bodyH - weekH, width - leftW, toybox::kHairline);
    for (int d = 0; d < sea.dayCount && d < kDaysShown; ++d) {
      const nv::DayStats s = statsFor(d);
      const int cx = leftW + 1 + d * cellW;
      if (d > 0) lab::box(screen, cx, bodyH - weekH + 10, 1, weekH - 20, fui::Paint::dither(fui::Color::DarkGray));
      int y = bodyH - weekH + 12;
      cap(screen, cx, y, cellW, kWeekdays[nv::weekday(sea.year, s.month, s.day)], fui::TextAlign::Center,
          d == 0 ? fui::Color::Black : fui::Color::DarkGray);
      y += kCap.inkHeight + 10;
      knots(s.windMax, a, sizeof(a));
      lab::value(screen, cx, y, cellW, a, fui::TextAlign::Center);
      y += toybox::kUiCut.inkHeight + 10;
      metres(s.waveMax, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s M", a);
      cap(screen, cx, y, cellW, b, fui::TextAlign::Center);
    }
  }
  wideStrip(screen, bodyH, stripH);
}

// Whose wind it is, and the key meteoblue's needs.
void NavalActivity::buildSource(toybox::Screen& screen) {
  lab::chrome(screen, "WIND SOURCE");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 16;
  lab::button(screen, lab::rect(x, y, inner, 56), "OPEN-METEO  ECMWF", ActionChooseSource, 0, !useMeteoblue_);
  y += 56 + 10;
  lab::button(screen, lab::rect(x, y, inner, 56), "METEOBLUE", ActionChooseSource, 1, useMeteoblue_);
  y += 56 + 22;
  char line[64];
  if (key_[0]) {
    // That there is one and how long it is; never the key.
    std::snprintf(line, sizeof(line), "METEOBLUE KEY SAVED, %d CHARACTERS", static_cast<int>(std::strlen(key_)) % 100);
  } else {
    std::snprintf(line, sizeof(line), "NO METEOBLUE KEY YET");
  }
  cap(screen, x, y, inner, line);
  y += kCap.inkHeight + 14;
  const int half = (inner - 8) / 2;
  lab::button(screen, lab::rect(x, y, key_[0] ? half : inner, 46), key_[0] ? "CHANGE KEY" : "ENTER KEY", ActionEnterKey,
              0);
  if (key_[0]) lab::button(screen, lab::rect(x + half + 8, y, half, 46), "REMOVE KEY", ActionRemoveKey, 0);
  y += 46 + 22;
  para(screen, x, y, inner,
       "METEOBLUE GIVES THE WIND ONLY. WAVES, SWELL, WATER AND GUSTS STAY OPEN-METEO'S. EACH FETCH SPENDS "
       "8000 OF YOUR METEOBLUE CREDITS; ON AUTO IT ASKS AT MOST EVERY 3 HOURS. THE KEY STAYS ON THIS CARD. "
       "IT CAN ALSO BE A TEXT FILE: /.crosspoint/naval/meteoblue.key",
       10);
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  lab::button(screen, lab::rect(x, bottom - 46, inner, 46), "DONE", ActionDismiss, 0);
}

// A day on its side: what it comes to on the left, its hours on the right.
void NavalActivity::buildWideDay(toybox::Screen& screen) {
  const int width = screen.device().width;
  const int height = screen.device().height;
  const int stripH = 42;
  const int bodyH = height - stripH;
  const nv::Sea& sea = *sea_;
  const nv::DayStats s = statsFor(dayOpen_);
  char a[24];
  char b[48];
  const int leftW = 250;
  std::snprintf(b, sizeof(b), "%s %d %s", kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day,
                kMonths[s.month > 12 ? 0 : s.month]);
  lab::value(screen, 18, 18, leftW - 36, b);
  cap(screen, 18, 18 + toybox::kUiCut.inkHeight + 10, leftW - 36, places_[selected_].name, fui::TextAlign::Left,
      fui::Color::DarkGray);
  cap(screen, 18, 104, leftW - 36, daylight_ ? "WIND MAX 08-21" : "WIND MAX", fui::TextAlign::Left,
      fui::Color::DarkGray);
  knots(s.windMax, a, sizeof(a));
  lab::label(screen, lab::rect(18, 132, 130, toybox::kLargeCut.inkHeight), a, toybox::kDisplayFont, toybox::kLargeCut,
             fui::TextAlign::Left);
  if (s.windMax >= 0) {
    fui::TextStyle st;
    st.font = toybox::kDisplayFont;
    const int tx = 18 + screen.target().measureText(toybox::kDisplayFont, a, st).width + 12;
    lab::value(screen, tx, 132, leftW - tx - 8, "KN");
    std::snprintf(b, sizeof(b), "F%d", nv::beaufort(s.windMax));
    lab::value(screen, tx, 132 + toybox::kLargeCut.inkHeight - toybox::kUiCut.inkHeight, leftW - tx - 8, b);
    lab::ink(screen, 18, 212, leftW - 36, toybox::kHairline);
    int y = 232;
    const int pitch = 50;
    cap(screen, 18, y + 7, 90, "FROM", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::value(screen, 110, y, leftW - 128, nv::compass(s.windFrom));
    y += pitch;
    knots(s.gustMax, a, sizeof(a));
    cap(screen, 18, y + 7, 90, "GUSTS", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::value(screen, 110, y, leftW - 128, a);
    y += pitch;
    metres(s.waveMax, a, sizeof(a));
    std::snprintf(b, sizeof(b), "%s M", a);
    cap(screen, 18, y + 7, 90, "SEA", fui::TextAlign::Left, fui::Color::DarkGray);
    lab::value(screen, 110, y, leftW - 128, b);
  }
  lab::ink(screen, leftW, 0, toybox::kHairline, bodyH);

  // The hours: bars, then under each the hour, the knots, the arrow and the waves.
  const int plotX = leftW + 22;
  const int plotW = width - plotX - 18;
  cap(screen, plotX, 16, plotW, "KNOTS, GUSTS OUTLINED", fui::TextAlign::Left, fui::Color::DarkGray);
  cap(screen, plotX, 16, plotW, "WAVES M", fui::TextAlign::Right, fui::Color::DarkGray);
  const int plotY = 46;
  const int plotH = bodyH - plotY - 132;
  const float top = scaleOf(dayOpen_);
  for (float line = 10; line <= top; line += 10) {
    const int ly = plotY + plotH - static_cast<int>(line / top * static_cast<float>(plotH));
    lab::box(screen, plotX, ly, plotW, 1, fui::Paint::dither(fui::Color::LightGray));
  }
  drawBars(screen, lab::rect(plotX, plotY, plotW, plotH), dayOpen_, top, true);
  lab::ink(screen, plotX, plotY + plotH, plotW, 2);
  int first = 0;
  const int n = windowOf(dayOpen_, first);
  const int slots = daylight_ ? kDayTo - kDayFrom + 1 : 24;
  const int base = daylight_ ? kDayFrom : 0;
  const int slot = plotW / slots;
  const int x0 = plotX + (plotW - slot * slots) / 2;
  const int step = daylight_ ? 1 : 3;
  for (int i = first; i < first + n; ++i) {
    const nv::Hour& hr = sea.hours[i];
    if ((hr.hour - base) % step != 0) continue;
    const int sx = x0 + (hr.hour - base) * slot;
    const int sw = slot * step;
    std::snprintf(b, sizeof(b), "%02d", hr.hour);
    cap(screen, sx, plotY + plotH + 10, sw, b, fui::TextAlign::Center, fui::Color::DarkGray);
    if (hr.wind >= 0) {
      knots(hr.wind, a, sizeof(a));
      cap(screen, sx, plotY + plotH + 36, sw, a, fui::TextAlign::Center);
      arrow(screen, sx + sw / 2, plotY + plotH + 72, 20, hr.windFrom, kInk);
    }
    // A wave height is four characters: every other hour, so they do not run together.
    if (hr.wave >= 0 && (step > 1 || (hr.hour - base) % 2 == 0)) {
      metres(hr.wave, a, sizeof(a));
      cap(screen, sx - slot / 2, plotY + plotH + 98, sw + slot, a, fui::TextAlign::Center);
    }
  }

  // The strip: the day before, back to the week, the hours, the day after.
  const int ws[4] = {230, 170, 170, width - 570};
  const fui::ActionId actions[4] = {ActionStepDay, ActionDismiss, ActionWindow, ActionStepDay};
  const int values[4] = {0, 0, daylight_ ? 0 : 1, 1};
  const char* words[4] = {"< DAY BEFORE", "WEEK", daylight_ ? "08-21" : "24 H", "DAY AFTER >"};
  int x = 0;
  for (int i = 0; i < 4; ++i) {
    fui::ButtonProps p;
    p.label = "";
    p.action = actions[i];
    p.value = static_cast<int16_t>(values[i]);
    p.styles = toybox::rowStyles();
    p.styles.normal.borderWidth = 0;
    p.minTouchSize = 0;
    screen.button(p, lab::rect(x, bodyH, ws[i], stripH));
    x += ws[i];
  }
  lab::ink(screen, 0, bodyH, width, stripH);
  x = 0;
  for (int i = 0; i < 4; ++i) {
    cap(screen, x + 14, bodyH + (stripH - kCap.inkHeight) / 2, ws[i] - 28, words[i],
        i == 0 ? fui::TextAlign::Left : (i == 3 ? fui::TextAlign::Right : fui::TextAlign::Center), fui::Color::White);
    if (i > 0) lab::box(screen, x, bodyH + 10, 1, stripH - 20, kPaper);
    x += ws[i];
  }
}

void NavalActivity::buildDay(toybox::Screen& screen) {
  if (wide_) {
    buildWideDay(screen);
    return;
  }
  const nv::Sea& sea = *sea_;
  int first = 0;
  const int n = windowOf(dayOpen_, first);
  const nv::DayStats s = statsFor(dayOpen_);
  char title[32];
  std::snprintf(title, sizeof(title), "%s %d %s", kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day,
                kMonths[s.month > 12 ? 0 : s.month]);
  lab::chrome(screen, title);
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  int y = toybox::kChromeHeight + 12;
  char a[24];
  char b[48];

  const int headH = 34;
  cap(screen, x, y + (headH - kCap.inkHeight) / 2, inner - 190, places_[selected_].name, fui::TextAlign::Left,
      fui::Color::DarkGray);
  drawWindow(screen, x + inner - 176, y, 176, headH);
  y += headH + 12;

  // The day's wind hour by hour: a bar for the wind inside a taller outline for the gusts.
  const int plotH = 120;
  const float top = scaleOf(dayOpen_);
  for (float line = 10; line <= top; line += 10) {
    const int ly = y + plotH - static_cast<int>(line / top * static_cast<float>(plotH));
    lab::box(screen, x, ly, inner, 1, fui::Paint::dither(fui::Color::LightGray));
    std::snprintf(a, sizeof(a), "%d", static_cast<int>(line));
    if (line == top || line == 10)
      cap(screen, x + inner - 36, ly + 3, 36, a, fui::TextAlign::Right, fui::Color::DarkGray);
  }
  drawBars(screen, lab::rect(x, y, inner, plotH), dayOpen_, top, true);
  lab::ink(screen, x, y + plotH, inner, 2);
  {
    const int slots = daylight_ ? kDayTo - kDayFrom + 1 : 24;
    const int slot = inner / slots;
    const int x0 = x + (inner - slot * slots) / 2;
    const int every = daylight_ ? 2 : 6;
    for (int k = 0; k < slots; k += every) {
      std::snprintf(a, sizeof(a), "%02d", (daylight_ ? kDayFrom : 0) + k);
      cap(screen, x0 + k * slot - 8, y + plotH + 8, slot + 16, a, fui::TextAlign::Center, fui::Color::DarkGray);
    }
  }
  y += plotH + 8 + kCap.inkHeight + 14;

  // The hours as a table: every other one of the sailing day, every third of the whole.
  const int step = daylight_ ? 2 : 3;
  const int base = daylight_ ? kDayFrom : 0;
  const int tableRows = daylight_ ? 7 : 8;
  static const int kCols[6] = {0, 50, 178, 240, 322, 368};
  static const char* const kHeads[6] = {"H", "WIND", "GUST", "WAVE", "PER", "SWELL"};
  for (int c = 0; c < 5; ++c) cap(screen, x + kCols[c], y, 110, kHeads[c], fui::TextAlign::Left, fui::Color::DarkGray);
  cap(screen, x + kCols[5], y, inner - kCols[5], kHeads[5], fui::TextAlign::Right, fui::Color::DarkGray);
  y += kCap.inkHeight + 8;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 4;
  const int rowH = (bottom - 54 - y) / tableRows;
  for (int i = first; i < first + n; ++i) {
    const nv::Hour& h = sea.hours[i];
    if ((h.hour - base) % step != 0) continue;
    const int row = (h.hour - base) / step;
    if (row >= tableRows) continue;
    const int ry = y + row * rowH;
    const int ty = ry + (rowH - kCap.inkHeight) / 2;
    if (row > 0) lab::box(screen, x, ry, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
    std::snprintf(b, sizeof(b), "%02d", h.hour);
    cap(screen, x + kCols[0], ty, 44, b);
    if (h.wind >= 0) {
      arrow(screen, x + kCols[1] + 12, ry + rowH / 2, 24, h.windFrom, kInk);
      knots(h.wind, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s %s", a, nv::compass(h.windFrom));
      cap(screen, x + kCols[1] + 30, ty, 96, b);
      knots(h.gust, a, sizeof(a));
      cap(screen, x + kCols[2], ty, 54, a);
    }
    metres(h.wave, a, sizeof(a));
    std::snprintf(b, sizeof(b), "%s M", a);
    cap(screen, x + kCols[3], ty, 82, b);
    if (h.wavePeriod >= 0) {
      std::snprintf(b, sizeof(b), "%dS", static_cast<int>(std::lround(h.wavePeriod)));
      cap(screen, x + kCols[4], ty, 48, b);
    }
    metres(h.swell, a, sizeof(a));
    cap(screen, x + kCols[5], ty, inner - kCols[5], a, fui::TextAlign::Right);
  }

  const int half = (inner - 8) / 2;
  lab::button(screen, lab::rect(x, bottom - 46, half, 46), "DAY BEFORE", ActionStepDay, 0);
  lab::button(screen, lab::rect(x + half + 8, bottom - 46, half, 46), "DAY AFTER", ActionStepDay, 1);
}

void NavalActivity::buildStatus(toybox::Screen& screen) {
  lab::chrome(screen, "NAVAL WEATHER");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int y = screen.device().height / 2 - 110;
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), statusHeadline_, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Left);
  para(screen, x, y + toybox::kDisplayCut.inkHeight + 22, inner, statusDetail_, 5);
  if (statusDismissable_) lab::button(screen, lab::rect(x, y + 230, inner, 48), "OK", ActionDismiss, 0);
}

void NavalActivity::render(RenderLock&&) {
  const bool board = onItsSide();
  renderer.setOrientation(board ? GfxRenderer::Orientation::LandscapeCounterClockwise
                                : GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, 32767, 32767);  // open, as the renderer starts: a screen-sized clip set upright cuts a page drawn wide
  renderer.clearScreen();
  // A size up in the small slot; on its side the title slot is the big number's.
  toybox::Faces faces;
  faces.small = toybox::kButtonFontId;
  if (board) faces.title = toybox::kLargeFontId;
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, faces);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (view_ == View::Status || !sea_) {
    buildStatus(screen);
  } else if (view_ == View::AutoPicker) {
    meteoui::buildAutoPicker(screen, autoMinutes_);
  } else if (view_ == View::Source) {
    buildSource(screen);
  } else if (view_ == View::Day && haveSea_) {
    buildDay(screen);
  } else {
    buildNow(screen);
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Naval");
  if (!board) {
    const auto labels = mappedInput.mapLabels("Back", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  if (behindDue_) autorefresh::drawUpdatingMark(renderer);
  renderer.displayBuffer();
  quiet_.paintPending = false;
}
