#pragma once

// WEATHER's save file: the places it keeps. In a header of its own because
// NAVAL WEATHER reads the same list rather than keeping a second one.

#include <cstdint>

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

}  // namespace weather
