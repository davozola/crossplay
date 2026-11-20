#include "WeatherActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

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
#include "../ui/ToyboxTheme.h"
#include "WeatherSave.h"
#include "WeatherWide.h"

namespace wx = meteo;
namespace fui = freeink::ui;

namespace {

using meteo::kCacheDir;
// Kept beside the save and not in it: HOY reads the save's layout as it is.
constexpr char kAutoPath[] = "/.crosspoint/weather-auto.cfg";
constexpr size_t kMaxBody = 48 * 1024;  // seven days of hours is ~10 KB
constexpr size_t kQueryMax = 40;

using meteo::kSavePath;
using meteo::kSaveVersion;
using meteo::SaveState;

}  // namespace

std::unique_ptr<Activity> MeteoActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MeteoActivity>(renderer, mappedInput);
}

void MeteoActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  loadAuto();
  haveForecast_ = loadCache();
  view_ = placeCount_ == 0 ? View::Cities : View::Forecast;
  if (placeCount_ > 0 && layout_ > 0) {
    loadBoard();
    view_ = View::Board;
  }
  lastFetchMs_ = static_cast<uint32_t>(millis());
  quiet_.touched();
  requestUpdate();
  // Left on auto, it comes back on auto, with a fetch straight away.
  if (autoMinutes_ > 0 && placeCount_ > 0)
    startJob(view_ == View::Board && !(wide_ && haveForecast_) ? Job::Board : Job::Forecast);
}

void MeteoActivity::requestUpdate(const bool immediate) {
  quiet_.paintPending = true;
  Activity::requestUpdate(immediate);
}

void MeteoActivity::onExit() {
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

// --- persistence ---------------------------------------------------------------

void MeteoActivity::loadState() {
  placeCount_ = 0;
  selected_ = 0;
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("WTHR", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(SaveState)) != sizeof(SaveState)) return;
  placeCount_ = s->count <= wx::kMaxPlaces ? s->count : 0;
  for (int i = 0; i < placeCount_; ++i) {
    places_[i] = s->places[i];
    places_[i].name[wx::kNameChars - 1] = 0;
    places_[i].region[wx::kRegionChars - 1] = 0;
  }
  selected_ = s->selected < placeCount_ ? s->selected : 0;
  fahrenheit_ = s->fahrenheit != 0;
}

void MeteoActivity::loadAuto() {
  autoMinutes_ = 0;
  layout_ = 0;
  wide_ = false;
  wideLayout_ = 0;
  if (!Storage.exists(kAutoPath)) return;
  HalFile file;
  uint8_t saved[4] = {0, 0, 0, 0};
  if (!Storage.openFileForRead("WTHR", kAutoPath, file) || file.read(saved, 4) < 1) return;
  for (const int choice : meteoui::kAutoChoices) {
    if (choice == saved[0]) autoMinutes_ = choice;
  }
  layout_ = saved[1] <= 2 ? saved[1] : 0;
  wide_ = saved[2] == 1;
  wideLayout_ = saved[3] < meteowide::kFaces ? saved[3] : 0;
}

void MeteoActivity::saveAuto() {
  HalFile file;
  if (!Storage.openFileForWrite("WTHR", kAutoPath, file)) return;
  const uint8_t saved[4] = {static_cast<uint8_t>(autoMinutes_), static_cast<uint8_t>(layout_),
                            static_cast<uint8_t>(wide_ ? 1 : 0), static_cast<uint8_t>(wideLayout_)};
  file.write(saved, 4);
  file.flush();
}

bool MeteoActivity::fetchQuietly(const int index) {
  if (index < 0 || index >= placeCount_) return false;
  char url[512];
  std::string body;
  body.reserve(12 * 1024);
  wx::forecastUrl(places_[index], url, sizeof(url));
  if (!HttpDownloader::fetchUrl(url, body) || body.size() > kMaxBody) {
    LOG_ERR("WTHR", "quiet fetch failed: status %d", HttpDownloader::lastStatus());
    return false;
  }
  auto fresh = makeUniqueNoThrow<wx::Forecast>();
  if (!fresh || !wx::parseForecast(body.data(), body.size(), *fresh)) return false;
  saveCacheFor(places_[index], body.data(), body.size());
  if (index == selected_) {
    forecast_ = *fresh;
    haveForecast_ = true;
  }
  return true;
}

void MeteoActivity::loadBoard() {
  auto scratch = makeUniqueNoThrow<wx::Forecast>();
  for (int i = 0; i < placeCount_; ++i) {
    cards_[i] = meteoui::BoardCard{};
    if (!scratch || !loadCacheFor(places_[i], *scratch)) continue;
    meteoui::BoardCard& c = cards_[i];
    c.have = true;
    c.code = static_cast<int16_t>(scratch->current.code);
    c.day = scratch->current.isDay;
    c.temp = scratch->current.temp;
    if (scratch->dayCount > 0) {
      c.hasDay = true;
      c.max = scratch->days[0].max;
      c.min = scratch->days[0].min;
      c.precip = scratch->days[0].precip;
    }
    if (std::strlen(scratch->current.time) >= 16)
      std::snprintf(c.time, sizeof(c.time), "%.5s", scratch->current.time + 11);
  }
}

bool MeteoActivity::onItsSide() const {
  if (!wide_ || step_ != Step::Idle) return false;
  // One city either way: on its side the board gives way to the chosen city.
  return (view_ == View::Board || view_ == View::Forecast) && haveForecast_ && placeCount_ > 0;
}

void MeteoActivity::setLayout(const int layout) {
  if (onItsSide()) {
    // On its side the same swipe goes round its own three pages.
    // One on from the last of the upright three is one back.
    const int by = (layout - layout_) == 1 ? 1 : meteowide::kFaces - 1;
    wideLayout_ = (wideLayout_ + by) % meteowide::kFaces;
    saveAuto();
    requestUpdate();
    return;
  }
  layout_ = ((layout % 3) + 3) % 3;
  saveAuto();
  if (layout_ == 0) {
    view_ = View::Forecast;
  } else {
    loadBoard();
    view_ = View::Board;
  }
  requestUpdate();
}

void MeteoActivity::saveState() {
  auto s = makeUniqueNoThrow<SaveState>();
  if (!s) return;
  s->count = static_cast<uint8_t>(placeCount_);
  s->selected = static_cast<uint8_t>(selected_);
  s->fahrenheit = fahrenheit_ ? 1 : 0;
  for (int i = 0; i < placeCount_; ++i) s->places[i] = places_[i];
  HalFile file;
  if (!Storage.openFileForWrite("WTHR", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(s.get()), sizeof(SaveState));
  file.flush();
}

void MeteoActivity::cachePath(const wx::Place& place, char* out, const size_t cap) const {
  meteo::cachePathOf(place, out, cap);
}

bool MeteoActivity::loadCache() {
  if (placeCount_ == 0) return false;
  return loadCacheFor(places_[selected_], forecast_);
}

bool MeteoActivity::loadCacheFor(const wx::Place& place, wx::Forecast& out) const {
  char path[64];
  cachePath(place, path, sizeof(path));
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("WTHR", path, file)) return false;
  const size_t size = file.fileSize();
  if (size == 0 || size > kMaxBody) return false;
  auto body = makeUniqueNoThrow<char[]>(size);
  if (!body) {
    LOG_ERR("WTHR", "OOM: %u bytes for cache", static_cast<unsigned>(size));
    return false;
  }
  if (file.read(body.get(), size) != static_cast<int>(size)) return false;
  return wx::parseForecast(body.get(), size, out);
}

// The list shows each city's last forecast, read back from the card once on
// the way in rather than on every frame.
void MeteoActivity::openCities() {
  auto scratch = makeUniqueNoThrow<wx::Forecast>();
  for (int i = 0; i < placeCount_; ++i) {
    glance_[i] = meteoui::CityGlance{};
    if (!scratch || !loadCacheFor(places_[i], *scratch)) continue;
    glance_[i].have = true;
    glance_[i].code = static_cast<int16_t>(scratch->current.code);
    glance_[i].day = scratch->current.isDay;
    glance_[i].temp = scratch->current.temp;
  }
  view_ = View::Cities;
}

bool MeteoActivity::saveCache(const char* body, const size_t len) {
  return saveCacheFor(places_[selected_], body, len);
}

bool MeteoActivity::saveCacheFor(const wx::Place& place, const char* body, const size_t len) {
  Storage.ensureDirectoryExists(kCacheDir);
  char path[64];
  cachePath(place, path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("WTHR", path, file)) return false;
  return file.write(reinterpret_cast<const uint8_t*>(body), len) == len;
}

// --- network -------------------------------------------------------------------

void MeteoActivity::startJob(const Job job) {
  // With a forecast already showing and a network to join alone, the page
  // stays: the fetch happens behind it.
  const bool showing = view_ == View::Board || ((view_ == View::Forecast || view_ == View::Day) && haveForecast_);
  if (job != Job::Search && showing && step_ == Step::Idle &&
      (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone())) {
    quietDue_ = true;
    quietPainted_ = false;
    requestUpdate();
    return;
  }
  job_ = job;
  step_ = Step::Joining;
  showStatus("JOINING WI-FI", "", false, view_ == View::Status ? statusBack_ : view_);
  wifiActivated_ = true;
  // Connected already, or a network saved to join by itself: the join happens
  // inside the job, with no picker in front of the page.
  if (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone()) {
    step_ = Step::Ready;
    return;
  }
  {
    // The saved network is joined behind the page; the picker is for when there is none.
    ActivityResultHandler afterWifi = [this](const ActivityResult& result) {
      if (result.isCancelled) {
        step_ = Step::Idle;
        job_ = Job::None;
        showStatus("NO WI-FI", "THE FORECAST NEEDS A NETWORK. THE LAST ONE FETCHED STAYS HERE.", true, statusBack_);
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

void MeteoActivity::runJob() {
  step_ = Step::Idle;
  const Job job = job_;
  job_ = Job::None;
  if (job == Job::Board) {
    // Every city on the board, one after another on the one connection.
    const int shown = placeCount_ < boardSize() ? placeCount_ : boardSize();
    int ok = 0;
    for (int i = 0; i < shown; ++i) ok += fetchQuietly(i) ? 1 : 0;
    lastFetchMs_ = static_cast<uint32_t>(millis());
    loadBoard();
    if (ok == 0) {
      showStatus("NO ANSWER", "OPEN-METEO DID NOT ANSWER. CHECK THE WI-FI AND TRY AGAIN.", true, View::Board);
    } else {
      view_ = View::Board;
    }
    requestUpdate();
    return;
  }
  char url[512];
  std::string body;
  body.reserve(job == Job::Forecast ? 12 * 1024 : 2 * 1024);
  if (job == Job::Search) {
    wx::geocodeUrl(query_, url, sizeof(url));
  } else {
    wx::forecastUrl(places_[selected_], url, sizeof(url));
  }
  if (!HttpDownloader::fetchUrl(url, body) || body.size() > kMaxBody) {
    LOG_ERR("WTHR", "fetch failed: status %d, %u bytes", HttpDownloader::lastStatus(),
            static_cast<unsigned>(body.size()));
    showStatus("NO ANSWER", "OPEN-METEO DID NOT ANSWER. CHECK THE WI-FI AND TRY AGAIN.", true,
               job == Job::Search ? View::Cities : View::Forecast);
    requestUpdate();
    return;
  }
  if (job == Job::Search) {
    resultCount_ = wx::parseGeocode(body.data(), body.size(), results_, wx::kMaxResults);
    view_ = View::Results;
    requestUpdate();
    return;
  }
  auto fresh = makeUniqueNoThrow<wx::Forecast>();
  if (!fresh || !wx::parseForecast(body.data(), body.size(), *fresh)) {
    LOG_ERR("WTHR", "unreadable forecast, %u bytes", static_cast<unsigned>(body.size()));
    showStatus("BAD ANSWER", "THE FORECAST CAME BACK IN A SHAPE THIS APP CANNOT READ.", true, View::Forecast);
    requestUpdate();
    return;
  }
  forecast_ = *fresh;
  haveForecast_ = true;
  lastFetchMs_ = static_cast<uint32_t>(millis());
  saveCache(body.data(), body.size());
  view_ = View::Forecast;
  requestUpdate();
}

void MeteoActivity::askCity() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, "CITY NAME", "", kQueryMax);
  if (!keyboard) {
    showStatus("NO MEMORY", "THERE WAS NOT ENOUGH MEMORY TO OPEN THE KEYBOARD.", true, View::Cities);
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    interactionsReady_ = false;
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    const auto& entered = std::get<KeyboardResult>(result.data);
    // Trimmed; an empty search goes nowhere.
    size_t a = 0;
    size_t b = entered.text.size();
    while (a < b && entered.text[a] == ' ') ++a;
    while (b > a && entered.text[b - 1] == ' ') --b;
    if (a == b) {
      requestUpdate();
      return;
    }
    std::snprintf(query_, sizeof(query_), "%.*s", static_cast<int>(b - a), entered.text.data() + a);
    view_ = View::Cities;
    startJob(Job::Search);
    requestUpdate();
  });
}

void MeteoActivity::showStatus(const char* headline, const char* detail, const bool dismissable, const View back) {
  statusHeadline_ = headline;
  statusDetail_ = detail;
  statusDismissable_ = dismissable;
  statusBack_ = back;
  view_ = View::Status;
}

// --- input ---------------------------------------------------------------------

void MeteoActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = job_ == Job::Search ? "SEARCHING" : "FETCHING";
    statusDetail_ = job_ == Job::Search ? query_
                                        : (job_ == Job::Board ? "EVERY CITY ON THE BOARD"
                                                              : (placeCount_ ? places_[selected_].name : ""));
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    {
      autorefresh::Radio radio;
      if (radio.online()) {
        runJob();
      } else {
        step_ = Step::Idle;
        job_ = Job::None;
        showStatus("NO WI-FI", "COULD NOT JOIN THE SAVED NETWORK. THE LAST FORECAST FETCHED STAYS HERE.", true,
                   statusBack_);
        requestUpdate();
      }
    }
    lastFetchMs_ = static_cast<uint32_t>(millis());
    quiet_.touched();
    return;
  }

  if (quietDue_ && step_ == Step::Idle) {
    if (!quietPainted_) {
      // The tag has to be on the panel before the radio blocks.
      quietPainted_ = true;
      requestUpdateAndWait();
      return;
    }
    wifiActivated_ = true;
    {
      autorefresh::Radio radio;
      if (radio.online()) {
        if (view_ == View::Board && !onItsSide()) {
          const int shown = placeCount_ < boardSize() ? placeCount_ : boardSize();
          bool any = false;
          for (int i = 0; i < shown; ++i) any = fetchQuietly(i) || any;
          if (any) loadBoard();
        } else {
          fetchQuietly(selected_);
        }
      }
    }
    quietDue_ = false;
    quietPainted_ = false;
    lastFetchMs_ = static_cast<uint32_t>(millis());
    quiet_.touched();
    requestUpdate();
    return;
  }

  // Auto-refresh: quietly, with no status page. The radio comes up for the
  // round and goes down after it; in between the page naps.
  quiet_.noteInput(mappedInput);
  if (autoMinutes_ > 0 && step_ == Step::Idle &&
      (view_ == View::Forecast || view_ == View::Day || view_ == View::Board)) {
    const uint32_t intervalMs = static_cast<uint32_t>(autoMinutes_) * 60000u;
    const uint32_t now = static_cast<uint32_t>(millis());
    if (now - lastFetchMs_ >= intervalMs) {
      lastFetchMs_ = now;
      wifiActivated_ = true;
      autorefresh::Radio radio;
      if (!radio.online()) {
        autorefresh::retrySoon(now, intervalMs, lastFetchMs_);
      } else {
        bool any = false;
        if (view_ == View::Board) {
          const int shown = placeCount_ < boardSize() ? placeCount_ : boardSize();
          for (int i = 0; i < shown; ++i) any = fetchQuietly(i) || any;
          if (any) loadBoard();
        } else {
          any = fetchQuietly(selected_);
        }
        if (any) requestUpdate();
      }
      return;
    }
    quiet_.rest(lastFetchMs_, intervalMs);
  }

  // A swipe either way is the next arrangement: one city, three, five.
  int pressX = 0;
  int pressY = 0;
  if (mappedInput.wasScreenTouchPressed(pressX, pressY)) pressX_ = pressX;
  if (step_ == Step::Idle && placeCount_ > 0 && (view_ == View::Forecast || view_ == View::Board)) {
    const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
    // Up or down turns the page on its side and back.
    if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
      wide_ = !wide_;
      saveAuto();
      requestUpdate();
      return;
    }
    if (swipe == MappedInputManager::SwipeDir::Left) {
      setLayout(layout_ + 1);
      return;
    }
    if (swipe == MappedInputManager::SwipeDir::Right) {
      setLayout(layout_ + 2);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    // A rightward swipe from mid-page arrives as Back: it is the arrangement before.
    if (mappedInput.wasBackGesture() && pressX_ >= 28 && placeCount_ > 0 &&
        (view_ == View::Forecast || view_ == View::Board)) {
      setLayout(layout_ + 2);
      return;
    }
    switch (view_) {
      case View::AutoPicker:
        view_ = layout_ > 0 ? View::Board : View::Forecast;
        if (view_ == View::Board) loadBoard();
        break;
      case View::Board:
        shelf::leave(renderer, mappedInput);
        return;
      case View::Forecast:
        // Opened from the board, a city goes back to it.
        if (layout_ > 0) {
          loadBoard();
          view_ = View::Board;
          break;
        }
        shelf::leave(renderer, mappedInput);
        return;
      case View::Day:
        view_ = View::Forecast;
        break;
      case View::Cities:
        if (placeCount_ == 0) {
          shelf::leave(renderer, mappedInput);
          return;
        }
        view_ = layout_ > 0 ? View::Board : View::Forecast;
        if (view_ == View::Board) loadBoard();
        break;
      case View::Results:
        view_ = View::Cities;
        break;
      case View::Status:
        view_ = statusBack_;
        break;
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

void MeteoActivity::route(const fui::ActionEvent& event) {
  namespace ui = meteoui;
  switch (event.action) {
    case ui::ActionCities:
      openCities();
      break;
    case ui::ActionRefresh:
      if (placeCount_ == 0) return;
      startJob(view_ == View::Board && !(wide_ && haveForecast_) ? Job::Board : Job::Forecast);
      break;
    case ui::ActionBoardOpen:
      if (event.value < 0 || event.value >= placeCount_) return;
      selected_ = event.value;
      saveState();
      haveForecast_ = loadCache();
      view_ = View::Forecast;
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
      saveAuto();
      view_ = layout_ > 0 ? View::Board : View::Forecast;
      if (view_ == View::Board) loadBoard();
      lastFetchMs_ = static_cast<uint32_t>(millis());
      // Turning it on fetches straight away, which is also what joins the
      // network the timer will need.
      if (autoMinutes_ > 0 && !wifiActivated_ && placeCount_ > 0) {
        startJob(view_ == View::Board && !(wide_ && haveForecast_) ? Job::Board : Job::Forecast);
        return;
      }
      break;
    }
    case ui::ActionSelect:
      if (event.value < 0 || event.value >= placeCount_) return;
      selected_ = event.value;
      saveState();
      haveForecast_ = loadCache();
      view_ = View::Forecast;
      // A city with nothing on the card yet is fetched straight away.
      if (!haveForecast_) startJob(Job::Forecast);
      break;
    case ui::ActionMoveUp: {
      if (event.value < 1 || event.value >= placeCount_) return;
      // The order is the order of the boards, so the first three and five are a choice.
      const wx::Place moved = places_[event.value];
      places_[event.value] = places_[event.value - 1];
      places_[event.value - 1] = moved;
      if (selected_ == event.value) {
        selected_ = event.value - 1;
      } else if (selected_ == event.value - 1) {
        selected_ = event.value;
      }
      saveState();
      openCities();
      break;
    }
    case ui::ActionRemove: {
      if (event.value < 0 || event.value >= placeCount_) return;
      char path[64];
      cachePath(places_[event.value], path, sizeof(path));
      Storage.remove(path);
      for (int i = event.value; i + 1 < placeCount_; ++i) places_[i] = places_[i + 1];
      --placeCount_;
      if (selected_ >= placeCount_) selected_ = placeCount_ > 0 ? placeCount_ - 1 : 0;
      saveState();
      haveForecast_ = loadCache();
      openCities();
      break;
    }
    case ui::ActionAdd:
    case ui::ActionSearch:
      askCity();
      return;
    case ui::ActionUnits:
      if ((event.value == 1) == fahrenheit_) return;
      fahrenheit_ = event.value == 1;
      saveState();
      break;
    case ui::ActionPick: {
      if (event.value < 0 || event.value >= resultCount_) return;
      const wx::Place& picked = results_[event.value];
      int at = -1;
      for (int i = 0; i < placeCount_; ++i) {
        if (wx::samePlace(places_[i], picked)) at = i;
      }
      if (at < 0) {
        if (placeCount_ >= wx::kMaxPlaces) return;
        at = placeCount_++;
        places_[at] = picked;
      }
      selected_ = at;
      saveState();
      haveForecast_ = loadCache();
      view_ = View::Forecast;
      startJob(Job::Forecast);
      break;
    }
    case ui::ActionDismiss:
      view_ = statusBack_;
      break;
    case ui::ActionDay:
      if (!haveForecast_ || event.value < 0 || event.value >= forecast_.dayCount) return;
      dayOpen_ = event.value;
      view_ = View::Day;
      break;
    default:
      return;
  }
  requestUpdate();
}

void MeteoActivity::render(RenderLock&&) {
  const bool side = onItsSide();
  renderer.setOrientation(side ? GfxRenderer::Orientation::LandscapeCounterClockwise
                               : GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, 32767, 32767);  // open, as the renderer starts: a screen-sized clip set upright cuts a page drawn wide
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  const auto bindTitle = [](void* ctx, const bool huge) {
    static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                       huge ? toybox::kHugeFontId : toybox::kDisplayFontId);
  };
  const auto wideForecast = [&] {
    meteoui::ForecastModel m;
    m.place = &places_[selected_];
    m.forecast = &forecast_;
    m.fahrenheit = fahrenheit_;
    m.autoMinutes = autoMinutes_;
    meteowide::draw(renderer, m, wideLayout_);
    // The foot of every face: cities, auto, refresh.
    const int16_t top = static_cast<int16_t>(device.height - meteowide::kFootHeight);
    const int16_t sideW = meteowide::kFootSide;
    frame.hit(fui::makeRect(0, top, sideW, meteowide::kFootHeight), meteoui::ActionCities);
    frame.hit(fui::makeRect(sideW, top, static_cast<int16_t>(device.width - sideW * 2), meteowide::kFootHeight),
              meteoui::ActionAuto);
    frame.hit(fui::makeRect(static_cast<int16_t>(device.width - sideW), top, sideW, meteowide::kFootHeight),
              meteoui::ActionRefresh);
  };

  switch (view_) {
    case View::Forecast: {
      if (side) {
        wideForecast();
        break;
      }
      meteoui::ForecastModel m;
      m.place = placeCount_ ? &places_[selected_] : nullptr;
      m.forecast = haveForecast_ ? &forecast_ : nullptr;
      m.fahrenheit = fahrenheit_;
      m.autoMinutes = autoMinutes_;
      m.faces.ctx = &target;
      m.faces.bindTitle = bindTitle;
      meteoui::buildForecast(surface, m);
      break;
    }
    case View::AutoPicker:
      meteoui::buildAutoPicker(surface, autoMinutes_);
      break;
    case View::Board: {
      if (side) {
        wideForecast();
        break;
      }
      meteoui::BoardModel m;
      m.places = places_;
      m.cards = cards_;
      m.count = placeCount_;
      m.size = boardSize();
      m.fahrenheit = fahrenheit_;
      m.autoMinutes = autoMinutes_;
      m.faces.ctx = &target;
      m.faces.bindTitle = [](void* ctx, const bool huge) {
        static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                           huge ? toybox::kHugeFontId : toybox::kDisplayFontId);
      };
      meteoui::buildBoard(surface, m);
      break;
    }
    case View::Day: {
      meteoui::DayModel m;
      m.place = placeCount_ ? &places_[selected_] : nullptr;
      m.forecast = &forecast_;
      m.day = dayOpen_;
      m.fahrenheit = fahrenheit_;
      m.faces.ctx = &target;
      m.faces.bindTitle = [](void* ctx, const bool huge) {
        static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                           huge ? toybox::kHugeFontId : toybox::kDisplayFontId);
      };
      meteoui::buildDay(surface, m);
      break;
    }
    case View::Cities: {
      meteoui::CitiesModel m;
      m.places = places_;
      m.glance = glance_;
      m.count = placeCount_;
      m.selected = selected_;
      m.fahrenheit = fahrenheit_;
      meteoui::buildCities(surface, m);
      break;
    }
    case View::Results: {
      meteoui::ResultsModel m;
      m.query = query_;
      m.results = results_;
      m.count = resultCount_;
      meteoui::buildResults(surface, m);
      break;
    }
    case View::Status: {
      meteoui::StatusModel m;
      m.kind = statusDismissable_ ? meteoui::StatusKind::Problem : meteoui::StatusKind::Working;
      m.headline = statusHeadline_;
      m.detail = statusDetail_;
      m.dismissable = statusDismissable_;
      meteoui::buildStatus(surface, m);
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Weather");
  if (!side) {
    const auto labels = mappedInput.mapLabels("Back", "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  if (quietDue_) autorefresh::drawUpdatingMark(renderer);
  renderer.displayBuffer();
  quiet_.paintPending = false;
}
