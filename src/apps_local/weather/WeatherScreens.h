#pragma once

// WEATHER on screen: the forecast, the saved cities, the search results and a
// status page, each a freestanding builder over toybox::Screen.

#include "../ui/ToyboxScreen.h"
#include "WeatherCore.h"

namespace weatherui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionCities = 1,
  ActionRefresh,
  ActionSelect,  // value: saved place index
  ActionRemove,  // value: saved place index
  ActionAdd,
  ActionUnits,
  ActionPick,  // value: search result index
  ActionSearch,
  ActionDismiss,
};

// The builder cannot reach the renderer, and the hero temperature is set in a
// cut bigger than the three the target holds. The activity lends it this to
// swap the title slot to the huge cut for that one string and back.
struct FaceSwap {
  void* ctx = nullptr;
  void (*bindTitle)(void* ctx, bool huge) = nullptr;
};

struct ForecastModel {
  const weather::Place* place = nullptr;
  const weather::Forecast* forecast = nullptr;  // null: nothing fetched yet
  bool fahrenheit = false;
  FaceSwap faces;
};

// What a saved city's cached forecast says now, for its row in the list.
struct CityGlance {
  bool have = false;
  bool day = true;
  int16_t code = 0;
  float temp = 0;
};

struct CitiesModel {
  const weather::Place* places = nullptr;
  const CityGlance* glance = nullptr;  // one per place, or null
  int count = 0;
  int selected = 0;
  bool fahrenheit = false;
};

struct ResultsModel {
  const char* query = "";
  const weather::Place* results = nullptr;
  int count = 0;
};

enum class StatusKind : uint8_t { Working, Problem };

struct StatusModel {
  StatusKind kind = StatusKind::Working;
  const char* headline = "";
  const char* detail = "";
  bool dismissable = false;
};

void buildForecast(toybox::Screen& screen, const ForecastModel& model);
void buildCities(toybox::Screen& screen, const CitiesModel& model);
void buildResults(toybox::Screen& screen, const ResultsModel& model);
void buildStatus(toybox::Screen& screen, const StatusModel& model);

// The sky glyph in a `size` square centred on (cx, cy).
void skyIcon(toybox::Screen& screen, weather::Sky sky, bool day, int cx, int cy, int size);

}  // namespace weatherui
