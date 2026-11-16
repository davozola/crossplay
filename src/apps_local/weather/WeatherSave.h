#pragma once

// WEATHER's save file: the places it keeps. In a header of its own because
// NAVAL WEATHER reads the same list rather than keeping a second one.

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "WeatherCore.h"

namespace weather {

constexpr char kSavePath[] = "/.crosspoint/weather.sav";
constexpr uint8_t kSaveVersion = 1;

// Written after one byte of kSaveVersion.
struct SaveState {
  uint8_t count;
  uint8_t selected;
  uint8_t fahrenheit;
  Place places[kMaxPlaces];
};

// Where a place's last forecast is kept, as Open-Meteo sent it. HOY reads and
// writes the same file, so the path is said once, here.
constexpr char kCacheDir[] = "/.crosspoint/weather";
inline void cachePathOf(const Place& place, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%d_%d.json", kCacheDir, static_cast<int>(place.lat * 100),
                static_cast<int>(place.lon * 100));
}

}  // namespace weather
