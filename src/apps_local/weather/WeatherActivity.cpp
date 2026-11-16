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
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"

namespace wx = weather;
namespace fui = freeink::ui;

namespace {

constexpr char kSavePath[] = "/.crosspoint/weather.sav";
constexpr char kCacheDir[] = "/.crosspoint/weather";
constexpr uint8_t kSaveVersion = 1;
constexpr size_t kMaxBody = 48 * 1024;  // seven days of hours is ~10 KB
constexpr size_t kQueryMax = 40;

struct SaveState {
  uint8_t count;
  uint8_t selected;
  uint8_t fahrenheit;
  wx::Place places[wx::kMaxPlaces];
};

}  // namespace

std::unique_ptr<Activity> WeatherActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<WeatherActivity>(renderer, mappedInput);
}

void WeatherActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  haveForecast_ = loadCache();
  view_ = placeCount_ == 0 ? View::Cities : View::Forecast;
  requestUpdate();
}

void WeatherActivity::onExit() {
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

void WeatherActivity::loadState() {
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

void WeatherActivity::saveState() {
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

void WeatherActivity::cachePath(const wx::Place& place, char* out, const size_t cap) const {
  std::snprintf(out, cap, "%s/%d_%d.json", kCacheDir, static_cast<int>(place.lat * 100),
                static_cast<int>(place.lon * 100));
}

bool WeatherActivity::loadCache() {
  if (placeCount_ == 0) return false;
  return loadCacheFor(places_[selected_], forecast_);
}

bool WeatherActivity::loadCacheFor(const wx::Place& place, wx::Forecast& out) const {
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
void WeatherActivity::openCities() {
  auto scratch = makeUniqueNoThrow<wx::Forecast>();
  for (int i = 0; i < placeCount_; ++i) {
    glance_[i] = weatherui::CityGlance{};
    if (!scratch || !loadCacheFor(places_[i], *scratch)) continue;
    glance_[i].have = true;
    glance_[i].code = static_cast<int16_t>(scratch->current.code);
    glance_[i].day = scratch->current.isDay;
    glance_[i].temp = scratch->current.temp;
  }
  view_ = View::Cities;
}

bool WeatherActivity::saveCache(const char* body, const size_t len) {
  Storage.ensureDirectoryExists(kCacheDir);
  char path[64];
  cachePath(places_[selected_], path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("WTHR", path, file)) return false;
  return file.write(reinterpret_cast<const uint8_t*>(body), len) == len;
}

// --- network -------------------------------------------------------------------

void WeatherActivity::startJob(const Job job) {
  job_ = job;
  step_ = Step::Joining;
  showStatus("JOINING WI-FI", "", false, view_ == View::Status ? statusBack_ : view_);
  wifiActivated_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    step_ = Step::Ready;
    return;
  }
  startActivityForResult(
      std::make_unique<WifiSelectionActivity>(renderer, mappedInput), [this](const ActivityResult& result) {
        if (result.isCancelled) {
          step_ = Step::Idle;
          job_ = Job::None;
          showStatus("NO WI-FI", "THE FORECAST NEEDS A NETWORK. THE LAST ONE FETCHED STAYS HERE.", true, statusBack_);
        } else {
          step_ = Step::Ready;
        }
        requestUpdate();
      });
}

void WeatherActivity::runJob() {
  step_ = Step::Idle;
  const Job job = job_;
  job_ = Job::None;
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
  saveCache(body.data(), body.size());
  view_ = View::Forecast;
  requestUpdate();
}

void WeatherActivity::askCity() {
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

void WeatherActivity::showStatus(const char* headline, const char* detail, const bool dismissable, const View back) {
  statusHeadline_ = headline;
  statusDetail_ = detail;
  statusDismissable_ = dismissable;
  statusBack_ = back;
  view_ = View::Status;
}

// --- input ---------------------------------------------------------------------

void WeatherActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = job_ == Job::Search ? "SEARCHING" : "FETCHING";
    statusDetail_ = job_ == Job::Search ? query_ : (placeCount_ ? places_[selected_].name : "");
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    runJob();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    switch (view_) {
      case View::Forecast:
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
        view_ = View::Forecast;
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

void WeatherActivity::route(const fui::ActionEvent& event) {
  namespace ui = weatherui;
  switch (event.action) {
    case ui::ActionCities:
      openCities();
      break;
    case ui::ActionRefresh:
      if (placeCount_ == 0) return;
      startJob(Job::Forecast);
      break;
    case ui::ActionSelect:
      if (event.value < 0 || event.value >= placeCount_) return;
      selected_ = event.value;
      saveState();
      haveForecast_ = loadCache();
      view_ = View::Forecast;
      // A city with nothing on the card yet is fetched straight away.
      if (!haveForecast_) startJob(Job::Forecast);
      break;
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

void WeatherActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen surface(frame);

  switch (view_) {
    case View::Forecast: {
      weatherui::ForecastModel m;
      m.place = placeCount_ ? &places_[selected_] : nullptr;
      m.forecast = haveForecast_ ? &forecast_ : nullptr;
      m.fahrenheit = fahrenheit_;
      m.faces.ctx = &target;
      m.faces.bindTitle = [](void* ctx, const bool huge) {
        static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                           huge ? toybox::kHugeFontId : toybox::kDisplayFontId);
      };
      weatherui::buildForecast(surface, m);
      break;
    }
    case View::Day: {
      weatherui::DayModel m;
      m.place = placeCount_ ? &places_[selected_] : nullptr;
      m.forecast = &forecast_;
      m.day = dayOpen_;
      m.fahrenheit = fahrenheit_;
      m.faces.ctx = &target;
      m.faces.bindTitle = [](void* ctx, const bool huge) {
        static_cast<fui::GfxRendererTarget*>(ctx)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                           huge ? toybox::kHugeFontId : toybox::kDisplayFontId);
      };
      weatherui::buildDay(surface, m);
      break;
    }
    case View::Cities: {
      weatherui::CitiesModel m;
      m.places = places_;
      m.glance = glance_;
      m.count = placeCount_;
      m.selected = selected_;
      m.fahrenheit = fahrenheit_;
      weatherui::buildCities(surface, m);
      break;
    }
    case View::Results: {
      weatherui::ResultsModel m;
      m.query = query_;
      m.results = results_;
      m.count = resultCount_;
      weatherui::buildResults(surface, m);
      break;
    }
    case View::Status: {
      weatherui::StatusModel m;
      m.kind = statusDismissable_ ? weatherui::StatusKind::Problem : weatherui::StatusKind::Working;
      m.headline = statusHeadline_;
      m.detail = statusDetail_;
      m.dismissable = statusDismissable_;
      weatherui::buildStatus(surface, m);
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Weather");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
