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
#include "../weather/WeatherSave.h"
#include "../weather/WeatherScreens.h"
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

// A titled cell: a caption, a value in the body cut and a line of detail.
void cell(toybox::Screen& screen, const int x, const int y, const int w, const char* title, const char* value,
          const char* detail) {
  lab::caption(screen, x, y, w, title, fui::TextAlign::Left, fui::Color::DarkGray);
  lab::value(screen, x, y + toybox::kTileCut.inkHeight + 8, w, value);
  lab::caption(screen, x, y + toybox::kTileCut.inkHeight + 8 + toybox::kUiCut.inkHeight + 8, w, detail);
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
    haveSea_ = loadCache();
  }
  loadAuto();
  lastFetchMs_ = static_cast<uint32_t>(millis());
  quiet_.touched();
  requestUpdate();
  // Left on auto, it comes back on auto, with a fetch straight away.
  if (autoMinutes_ > 0 && placeCount_ > 0) startFetch();
}

void NavalActivity::onExit() {
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
  if (!Storage.exists(weather::kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("NAVL", weather::kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != weather::kSaveVersion) return;
  auto s = makeUniqueNoThrow<weather::SaveState>();
  if (!s) {
    LOG_ERR("NAVL", "OOM: places");
    return;
  }
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(weather::SaveState)) != sizeof(weather::SaveState)) return;
  placeCount_ = s->count <= weather::kMaxPlaces ? s->count : 0;
  for (int i = 0; i < placeCount_; ++i) {
    places_[i] = s->places[i];
    places_[i].name[weather::kNameChars - 1] = 0;
    places_[i].region[weather::kRegionChars - 1] = 0;
  }
  selected_ = s->selected < placeCount_ ? s->selected : 0;
}

void NavalActivity::cachePath(const char* which, char* out, const size_t cap) const {
  const weather::Place& p = places_[selected_];
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

void NavalActivity::startFetch() {
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
  const weather::Place& p = places_[selected_];
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
  for (const int choice : weatherui::kAutoChoices) {
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
    if (event.action != weatherui::ActionChooseAuto) return;
    int chosen = -1;
    for (const int choice : weatherui::kAutoChoices) {
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

void NavalActivity::buildNow(toybox::Screen& screen) {
  lab::chrome(screen, "NAVAL WEATHER");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  int y = toybox::kChromeHeight + 10;

  // The place, with a step either side when there is more than one.
  const weather::Place& place = places_[selected_];
  if (placeCount_ > 1) {
    lab::button(screen, lab::rect(x, y, 52, 46), "<", ActionPlace, 0, false, false);
    lab::button(screen, lab::rect(x + inner - 52, y, 52, 46), ">", ActionPlace, 1, false, false);
  }
  lab::value(screen, x + 58, y + (46 - toybox::kUiCut.inkHeight) / 2, inner - 116, place.name, fui::TextAlign::Center);
  y += 46 + 10;

  if (!haveSea_) {
    lab::paragraph(screen, x, y + 40, inner,
                   "NOTHING FETCHED FOR THIS PLACE YET. REFRESH WHILE THERE IS WI-FI AND THE WEEK STAYS ON THE CARD.",
                   3);
    const int half = (inner - 8) / 2;
    lab::button(screen, lab::rect(x, bottom - 46, half, 46), "SOURCE", ActionSource, 0, useMeteoblue_);
    lab::button(screen, lab::rect(x + half + 8, bottom - 46, inner - half - 8, 46), "REFRESH", ActionRefresh, 0);
    return;
  }

  const nv::Sea& sea = *sea_;
  const int now = nv::nowIndex(sea);
  const nv::Hour& h = sea.hours[now];
  char a[24];
  char b[48];

  // Wind: a rose on the left with the arrow flying downwind, the numbers on the right.
  const int roseR = 62;
  const int roseX = x + roseR + 6;
  const int roseY = y + roseR + 4;
  toybox::disc(screen, static_cast<int16_t>(roseX), static_cast<int16_t>(roseY), static_cast<int16_t>(roseR),
               fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(roseX), static_cast<int16_t>(roseY), static_cast<int16_t>(roseR - 3),
               fui::Color::White);
  for (int t = 0; t < 8; ++t) {
    const float ang = static_cast<float>(t) * 0.7853982f;
    const int len = t % 2 == 0 ? 9 : 5;
    const int tx = roseX + static_cast<int>(std::sin(ang) * static_cast<float>(roseR - 3 - len / 2));
    const int ty = roseY - static_cast<int>(std::cos(ang) * static_cast<float>(roseR - 3 - len / 2));
    lab::ink(screen, tx - 2, ty - 2, 4, 4);
  }
  lab::caption(screen, roseX - 10, roseY - roseR + 12, 20, "N", fui::TextAlign::Center);
  if (h.swell > 0) arrow(screen, roseX, roseY, 56, h.swellFrom, fui::Paint::dither(fui::Color::DarkGray));
  if (h.wind >= 0) arrow(screen, roseX, roseY, 74, h.windFrom, kInk);

  const int tx = roseX + roseR + 22;
  const int tw = x + inner - tx;
  lab::caption(screen, tx, y, tw, "WIND NOW", fui::TextAlign::Left, fui::Color::DarkGray);
  knots(h.wind, a, sizeof(a));
  std::snprintf(b, sizeof(b), "%s KN", a);
  lab::label(screen, lab::rect(tx, y + 22, tw, toybox::kDisplayCut.inkHeight), b, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Left);
  if (h.wind >= 0) {
    const int force = nv::beaufort(h.wind);
    std::snprintf(b, sizeof(b), "F%d  %s", force, nv::beaufortName(force));
    lab::caption(screen, tx, y + 72, tw, b);
    std::snprintf(b, sizeof(b), "FROM %s", nv::compass(h.windFrom));
    lab::caption(screen, tx, y + 94, tw, b);
    knots(h.gust, a, sizeof(a));
    std::snprintf(b, sizeof(b), "GUSTS %s KN", a);
    lab::caption(screen, tx, y + 116, tw, b);
  } else {
    lab::caption(screen, tx, y + 72, tw, "NO WIND IN THE LAST FETCH");
  }
  y += roseR * 2 + 16;

  // The sea in four cells.
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 12;
  const int cw = inner / 4;
  metres(h.wave, a, sizeof(a));
  std::snprintf(b, sizeof(b), "%s M", a);
  char d[32];
  std::snprintf(d, sizeof(d), "%dS %s", static_cast<int>(std::lround(h.wavePeriod < 0 ? 0 : h.wavePeriod)),
                nv::compass(h.waveFrom));
  cell(screen, x, y, cw, "WAVES", b, d);
  metres(h.swell, a, sizeof(a));
  std::snprintf(b, sizeof(b), "%s M", a);
  std::snprintf(d, sizeof(d), "%dS %s", static_cast<int>(std::lround(h.swellPeriod < 0 ? 0 : h.swellPeriod)),
                nv::compass(h.swellFrom));
  cell(screen, x + cw, y, cw, "SWELL", b, d);
  metres(h.windWave, a, sizeof(a));
  std::snprintf(b, sizeof(b), "%s M", a);
  cell(screen, x + cw * 2, y, cw, "WIND SEA", b, h.wave >= 0 ? nv::seaStateName(nv::seaState(h.wave)) : "");
  if (h.water > -50)
    std::snprintf(b, sizeof(b), "%d C", static_cast<int>(std::lround(h.water)));
  else
    std::snprintf(b, sizeof(b), "--");
  cell(screen, x + cw * 3, y, cw, "WATER", b, "SURFACE");
  y += toybox::kTileCut.inkHeight * 2 + toybox::kUiCut.inkHeight + 16 + 12;

  // The week: one row a day, the windiest hour and the highest sea. Tap for the hours.
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 8;
  const int colDay = x + 8;
  const int colWind = x + 88;
  const int colDir = x + 200;
  const int colSea = x + 280;
  lab::caption(screen, colDay, y, 80, "DAY", fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, colWind, y, 130, "WIND MAX", fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, colDir, y, 80, "FROM", fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, colSea, y, inner - 280, "SEA MAX", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 6;
  const int rows = sea.dayCount;
  const int rowH = (bottom - 76 - y) / (rows ? rows : 1) > 44 ? 44 : (bottom - 76 - y) / (rows ? rows : 1);
  for (int dIdx = 0; dIdx < rows; ++dIdx) {
    const nv::DayStats s = nv::statsOf(sea, dIdx);
    const int ry = y + dIdx * rowH;
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionDay;
    row.value = static_cast<int16_t>(dIdx);
    row.styles = toybox::rowStyles();
    row.radius = lab::kRadius;
    screen.button(row, lab::rect(x, ry, inner, rowH - 4));
    const int ty = ry + (rowH - 4 - toybox::kTileCut.inkHeight) / 2;
    std::snprintf(b, sizeof(b), "%s %d", dIdx == 0 ? "TODAY" : kWeekdays[nv::weekday(sea.year, s.month, s.day)], s.day);
    lab::caption(screen, colDay, ty, 84, dIdx == 0 ? "TODAY" : b);
    if (s.windMax >= 0) {
      knots(s.windMax, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s KN F%d", a, nv::beaufort(s.windMax));
      lab::caption(screen, colWind, ty, 130, b);
      arrow(screen, colDir + 10, ry + (rowH - 4) / 2, 22, s.windFrom, kInk);
      lab::caption(screen, colDir + 30, ty, 50, nv::compass(s.windFrom));
    }
    metres(s.waveMax, a, sizeof(a));
    std::snprintf(b, sizeof(b), "%s M %s", a, s.waveMax >= 0 ? nv::seaStateName(nv::seaState(s.waveMax)) : "");
    lab::caption(screen, colSea, ty, x + inner - colSea - 6, b);
  }

  // When it was fetched, by the place's own clock, and the way to fetch again.
  std::snprintf(b, sizeof(b), "%.5s  %d %s", sea.now[0] ? sea.now + 11 : "--:--", h.day,
                kMonths[h.month > 12 ? 0 : h.month]);
  // Above the buttons, so the three of them have the row.
  lab::caption(screen, x, bottom - 46 - toybox::kTileCut.inkHeight - 8, inner, b, fui::TextAlign::Left,
               fui::Color::DarkGray);
  lab::caption(screen, x, bottom - 46 - toybox::kTileCut.inkHeight - 8, inner,
               sea.windIsMeteoblue ? "WIND: METEOBLUE" : "WIND: ECMWF", fui::TextAlign::Right, fui::Color::DarkGray);
  if (autoMinutes_ <= 0) {
    std::snprintf(a, sizeof(a), "AUTO OFF");
  } else if (autoMinutes_ >= 60) {
    std::snprintf(a, sizeof(a), "AUTO %d H", autoMinutes_ / 60);
  } else {
    std::snprintf(a, sizeof(a), "AUTO %d M", autoMinutes_);
  }
  const int gap = 8;
  const int third = (inner - gap * 2) / 3;
  lab::button(screen, lab::rect(x, bottom - 46, third, 46), "SOURCE", ActionSource, 0, useMeteoblue_);
  lab::button(screen, lab::rect(x + third + gap, bottom - 46, third, 46), a, ActionAuto, 0, autoMinutes_ > 0);
  lab::button(screen, lab::rect(x + (third + gap) * 2, bottom - 46, inner - (third + gap) * 2, 46), "REFRESH",
              ActionRefresh, 0);
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
  lab::caption(screen, x, y, inner, line);
  y += toybox::kTileCut.inkHeight + 14;
  const int half = (inner - 8) / 2;
  lab::button(screen, lab::rect(x, y, key_[0] ? half : inner, 46), key_[0] ? "CHANGE KEY" : "ENTER KEY", ActionEnterKey,
              0);
  if (key_[0]) lab::button(screen, lab::rect(x + half + 8, y, half, 46), "REMOVE KEY", ActionRemoveKey, 0);
  y += 46 + 22;
  lab::paragraph(screen, x, y, inner,
                 "METEOBLUE GIVES THE WIND ONLY. WAVES, SWELL, WATER AND GUSTS STAY OPEN-METEO'S. EACH FETCH SPENDS "
                 "8000 OF YOUR METEOBLUE CREDITS; ON AUTO IT ASKS AT MOST EVERY 3 HOURS. THE KEY STAYS ON THIS CARD. "
                 "IT CAN ALSO BE A TEXT FILE: /.crosspoint/naval/meteoblue.key",
                 8);
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  lab::button(screen, lab::rect(x, bottom - 46, inner, 46), "DONE", ActionDismiss, 0);
}

void NavalActivity::buildDay(toybox::Screen& screen) {
  const nv::Sea& sea = *sea_;
  int first = 0;
  const int n = nv::hoursOfDay(sea, dayOpen_, first);
  const nv::DayStats s = nv::statsOf(sea, dayOpen_);
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

  lab::caption(screen, x, y, inner, places_[selected_].name, fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, x, y, inner, "WIND SOLID, GUSTS OUTLINED, KNOTS", fui::TextAlign::Right, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 10;

  // The day's wind hour by hour: a bar for the wind inside a taller outline for the gusts.
  const int plotH = 130;
  float top = 20;
  for (int i = first; i < first + n; ++i) {
    if (sea.hours[i].gust > top) top = sea.hours[i].gust;
    if (sea.hours[i].wind > top) top = sea.hours[i].wind;
  }
  top = std::ceil(top / 10) * 10;
  const int slot = inner / 24;
  const int plotX = x + (inner - slot * 24) / 2;
  for (float line = 10; line <= top; line += 10) {
    const int ly = y + plotH - static_cast<int>(line / top * static_cast<float>(plotH));
    lab::box(screen, x, ly, inner, 1, fui::Paint::dither(fui::Color::LightGray));
    std::snprintf(a, sizeof(a), "%d", static_cast<int>(line));
    if (line == top || line == 10)
      lab::caption(screen, x + inner - 30, ly + 3, 30, a, fui::TextAlign::Right, fui::Color::DarkGray);
  }
  for (int i = first; i < first + n; ++i) {
    const nv::Hour& h = sea.hours[i];
    const int bx = plotX + h.hour * slot + 2;
    if (h.gust > 0) {
      const int gh = static_cast<int>(h.gust / top * static_cast<float>(plotH));
      lab::box(screen, bx, y + plotH - gh, slot - 4, gh, kInk, 2);
      lab::box(screen, bx + 1, y + plotH - gh + 1, slot - 6, gh - 1, kPaper, 1);
    }
    if (h.wind > 0) {
      const int wh = static_cast<int>(h.wind / top * static_cast<float>(plotH));
      lab::box(screen, bx, y + plotH - wh, slot - 4, wh, kInk, 2);
    }
  }
  lab::ink(screen, x, y + plotH, inner, 2);
  for (int hr = 0; hr < 24; hr += 6) {
    std::snprintf(a, sizeof(a), "%02d", hr);
    lab::caption(screen, plotX + hr * slot - 4, y + plotH + 8, 30, a, fui::TextAlign::Left, fui::Color::DarkGray);
  }
  y += plotH + 8 + toybox::kTileCut.inkHeight + 12;

  // Every third hour as a line of the table.
  static const int kCols[6] = {0, 62, 190, 262, 342, 404};
  static const char* const kHeads[6] = {"HOUR", "WIND KN", "GUST", "WAVES", "PER", "SWELL"};
  for (int c = 0; c < 6; ++c)
    lab::caption(screen, x + kCols[c], y, 110, kHeads[c], fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 6;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 4;
  const int rowH = (bottom - 54 - y) / 8;
  for (int i = first; i < first + n; ++i) {
    const nv::Hour& h = sea.hours[i];
    if (h.hour % 3 != 0) continue;
    const int ry = y + (h.hour / 3) * rowH;
    const int ty = ry + (rowH - toybox::kTileCut.inkHeight) / 2;
    if (h.hour > 0) lab::box(screen, x, ry, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
    std::snprintf(b, sizeof(b), "%02d:00", h.hour);
    lab::caption(screen, x + kCols[0], ty, 60, b);
    if (h.wind >= 0) {
      arrow(screen, x + kCols[1] + 10, ry + rowH / 2, 22, h.windFrom, kInk);
      knots(h.wind, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s %s", a, nv::compass(h.windFrom));
      lab::caption(screen, x + kCols[1] + 28, ty, 96, b);
      knots(h.gust, a, sizeof(a));
      lab::caption(screen, x + kCols[2], ty, 60, a);
    }
    metres(h.wave, a, sizeof(a));
    std::snprintf(b, sizeof(b), "%s M", a);
    lab::caption(screen, x + kCols[3], ty, 78, b);
    if (h.wavePeriod >= 0) {
      std::snprintf(b, sizeof(b), "%dS", static_cast<int>(std::lround(h.wavePeriod)));
      lab::caption(screen, x + kCols[4], ty, 60, b);
    }
    metres(h.swell, a, sizeof(a));
    lab::caption(screen, x + kCols[5], ty, inner - kCols[5], a);
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
  lab::paragraph(screen, x, y + toybox::kDisplayCut.inkHeight + 22, inner, statusDetail_, 4);
  if (statusDismissable_) lab::button(screen, lab::rect(x, y + 200, inner, 48), "OK", ActionDismiss, 0);
}

void NavalActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (view_ == View::Status || !sea_) {
    buildStatus(screen);
  } else if (view_ == View::AutoPicker) {
    weatherui::buildAutoPicker(screen, autoMinutes_);
  } else if (view_ == View::Source) {
    buildSource(screen);
  } else if (view_ == View::Day && haveSea_) {
    buildDay(screen);
  } else {
    buildNow(screen);
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Naval");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
  quiet_.paintPending = false;
}
