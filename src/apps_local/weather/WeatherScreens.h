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

struct ForecastModel {
  const weather::Place* place = nullptr;
  const weather::Forecast* forecast = nullptr;  // null: nothing fetched yet
  bool fahrenheit = false;
};

struct CitiesModel {
  const weather::Place* places = nullptr;
  int count = 0;
  int selected = 0;
  bool fahrenheit = false;
};

struct ResultsModel {
  const char* query = "";
  const weather::Place* results = nullptr;
  int count = 0;
};

struct StatusModel {
  const char* title = "WEATHER";
  const char* headline = "";
  const char* detail = "";
  bool dismissable = false;
};

void buildForecast(toybox::Screen& screen, const ForecastModel& model);
void buildCities(toybox::Screen& screen, const CitiesModel& model);
void buildResults(toybox::Screen& screen, const ResultsModel& model);
void buildStatus(toybox::Screen& screen, const StatusModel& model);

// The sky glyph in a `size` square centred on (cx, cy). Exposed for the shelf
// census and the host renderer.
void skyIcon(toybox::Screen& screen, weather::Sky sky, bool day, int cx, int cy, int size);

}  // namespace weatherui
