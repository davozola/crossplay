#pragma once

// HOY's data: what WEATHER, NAVAL WEATHER and MARKETS last put on the card,
// read back, and all of it fetched again in one go. It keeps nothing of its
// own: every file it writes is one of theirs, so a refresh here is a refresh
// there.

#include <cstddef>

#include "../markets/MarketsCore.h"
#include "../naval/NavalCore.h"
#include "../weather/WeatherCore.h"

namespace todaystore {

struct Snapshot {
  bool hasPlace = false;
  bool fahrenheit = false;
  weather::Place place;
  bool hasForecast = false;
  weather::Forecast forecast;
  bool hasSea = false;
  naval::Sea sea;
  markets::Quote quotes[markets::kAssets];
};

// The place WEATHER has selected, and whatever is cached for it and for the
// three quotes.
void load(Snapshot& out);
// Fetches everything, blocking; Wi-Fi must be up. Returns how many of the
// three kinds (weather, sea, markets) brought something, and a line on each.
int fetchAll(Snapshot& snapshot, char* report, size_t reportCap);

}  // namespace todaystore
