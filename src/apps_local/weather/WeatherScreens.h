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
  ActionDay,  // value: day index, to open its sheet
  ActionAuto,
  ActionMoveUp,      // value: saved place index, to swap with the one above
  ActionBoardOpen,   // value: saved place index, to open it full screen
  ActionChooseAuto,  // value: minutes, 0 for off
};

// The minutes the auto-refresh picker offers; 0 is off. A forecast moves more
// slowly than a market, so the steps are longer than MARKETS'.
constexpr int kAutoChoices[] = {0, 15, 30, 60, 180};
constexpr int kAutoChoiceCount = sizeof(kAutoChoices) / sizeof(kAutoChoices[0]);

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
  int autoMinutes = 0;
  // On its side: 0 the day as a curve, 1 the week as bars, 2 now as instruments.
  int wideLayout = 0;
  FaceSwap faces;
};

struct DayModel {
  const weather::Place* place = nullptr;
  const weather::Forecast* forecast = nullptr;
  int day = 0;
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
void buildDay(toybox::Screen& screen, const DayModel& model);
void buildCities(toybox::Screen& screen, const CitiesModel& model);
void buildResults(toybox::Screen& screen, const ResultsModel& model);
void buildStatus(toybox::Screen& screen, const StatusModel& model);
void buildAutoPicker(toybox::Screen& screen, int currentMinutes);

// A saved city's cached forecast, as much of it as a tile on the board shows.
struct BoardCard {
  bool have = false;
  bool day = true;
  int16_t code = 0;
  float temp = 0;
  float max = 0;
  float min = 0;
  int8_t precip = 0;
  bool hasDay = false;
  char time[6] = "";  // "HH:MM" the forecast is from
};

// Several cities at once: the first `size` saved ones, a tile each.
struct BoardModel {
  const weather::Place* places = nullptr;
  const BoardCard* cards = nullptr;
  int count = 0;  // saved places
  int size = 3;   // tiles on the page: 3 or 5
  bool fahrenheit = false;
  int autoMinutes = 0;
  FaceSwap faces;
};
void buildBoard(toybox::Screen& screen, const BoardModel& model);

// One city with the panel on its side, in the arrangement the model names.
void buildWideForecast(toybox::Screen& screen, const ForecastModel& model);

// The sky glyph in a `size` square centred on (cx, cy).
void skyIcon(toybox::Screen& screen, weather::Sky sky, bool day, int cx, int cy, int size);

}  // namespace weatherui
