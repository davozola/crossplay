#include "TodayStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <memory>
#include <string>

#include "../../network/HttpDownloader.h"
#include "../markets/MarketsStore.h"
#include "../naval/NavalPaths.h"
#include "../weather/WeatherSave.h"

namespace todaystore {

namespace {

constexpr size_t kMaxBody = 48 * 1024;

bool readFile(const char* path, std::unique_ptr<char[]>& body, size_t& size) {
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("HOY", path, file)) return false;
  size = file.fileSize();
  if (size == 0 || size > kMaxBody) return false;
  body = makeUniqueNoThrow<char[]>(size);
  if (!body) {
    LOG_ERR("HOY", "OOM: %u bytes for a cached answer", static_cast<unsigned>(size));
    return false;
  }
  return file.read(body.get(), size) == static_cast<int>(size);
}

void writeFile(const char* path, const std::string& body) {
  HalFile file;
  if (!Storage.openFileForWrite("HOY", path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
  file.flush();
}

void loadPlace(Snapshot& out) {
  out.hasPlace = false;
  if (!Storage.exists(weather::kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("HOY", weather::kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != weather::kSaveVersion) return;
  auto s = makeUniqueNoThrow<weather::SaveState>();
  if (!s) {
    LOG_ERR("HOY", "OOM: places");
    return;
  }
  if (file.read(reinterpret_cast<uint8_t*>(s.get()), sizeof(weather::SaveState)) != sizeof(weather::SaveState)) return;
  if (s->count == 0 || s->count > weather::kMaxPlaces) return;
  out.place = s->places[s->selected < s->count ? s->selected : 0];
  out.place.name[weather::kNameChars - 1] = 0;
  out.place.region[weather::kRegionChars - 1] = 0;
  out.fahrenheit = s->fahrenheit == 1;
  out.hasPlace = true;
}

}  // namespace

void load(Snapshot& out) {
  loadPlace(out);
  out.hasForecast = false;
  out.hasSea = false;
  if (out.hasPlace) {
    char path[80];
    std::unique_ptr<char[]> body;
    size_t size = 0;
    weather::cachePathOf(out.place, path, sizeof(path));
    out.hasForecast = readFile(path, body, size) && weather::parseForecast(body.get(), size, out.forecast);
    body.reset();
    naval::cachePathOf(out.place.lat, out.place.lon, "sea", path, sizeof(path));
    out.hasSea = readFile(path, body, size) && naval::parseMarine(body.get(), size, out.sea);
    body.reset();
    if (out.hasSea) {
      naval::cachePathOf(out.place.lat, out.place.lon, "wind", path, sizeof(path));
      if (readFile(path, body, size)) naval::parseWind(body.get(), size, out.sea);
    }
  }
  for (int i = 0; i < markets::kAssets; ++i) {
    if (!marketsstore::loadCached(static_cast<markets::Asset>(i), out.quotes[i])) out.quotes[i] = markets::Quote{};
  }
}

int fetchAll(Snapshot& snapshot, char* report, const size_t reportCap) {
  int kinds = 0;
  const char* weatherWord = "NO PLACE";
  const char* seaWord = "NO PLACE";
  if (snapshot.hasPlace) {
    char url[512];
    char path[80];
    std::string body;
    body.reserve(16 * 1024);

    // The forecast.
    Storage.ensureDirectoryExists(weather::kCacheDir);
    weather::forecastUrl(snapshot.place, url, sizeof(url));
    weatherWord = "NO ANSWER";
    if (HttpDownloader::fetchUrl(url, body) && body.size() <= kMaxBody) {
      auto fresh = makeUniqueNoThrow<weather::Forecast>();
      if (fresh && weather::parseForecast(body.data(), body.size(), *fresh)) {
        snapshot.forecast = *fresh;
        snapshot.hasForecast = true;
        weather::cachePathOf(snapshot.place, path, sizeof(path));
        writeFile(path, body);
        weatherWord = "OK";
        ++kinds;
      }
    }

    // The sea, then the wind over it. A place inland has neither.
    Storage.ensureDirectoryExists(naval::kCacheDir);
    body.clear();
    naval::marineUrl(snapshot.place.lat, snapshot.place.lon, url, sizeof(url));
    seaWord = "NO ANSWER";
    if (HttpDownloader::fetchUrl(url, body) && body.size() <= kMaxBody) {
      auto fresh = makeUniqueNoThrow<naval::Sea>();
      if (fresh && naval::parseMarine(body.data(), body.size(), *fresh)) {
        naval::cachePathOf(snapshot.place.lat, snapshot.place.lon, "sea", path, sizeof(path));
        writeFile(path, body);
        body.clear();
        naval::windUrl(snapshot.place.lat, snapshot.place.lon, url, sizeof(url));
        if (HttpDownloader::fetchUrl(url, body) && body.size() <= kMaxBody &&
            naval::parseWind(body.data(), body.size(), *fresh)) {
          naval::cachePathOf(snapshot.place.lat, snapshot.place.lon, "wind", path, sizeof(path));
          writeFile(path, body);
        }
        snapshot.sea = *fresh;
        snapshot.hasSea = true;
        seaWord = "OK";
        ++kinds;
      } else {
        seaWord = "NO SEA HERE";
      }
    }
  }
  char markets[96];
  const int quotes = marketsstore::fetchAll(snapshot.quotes, markets, sizeof(markets));
  if (quotes > 0) ++kinds;
  std::snprintf(report, reportCap, "WEATHER: %s.  SEA: %s.  %.60s", weatherWord, seaWord, markets);
  return kinds;
}

}  // namespace todaystore
