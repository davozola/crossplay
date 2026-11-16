#pragma once

// MARKETS on screen: the three-card dashboard, one instrument in detail, the
// portfolio with its picker and keypad, the auto-refresh picker and a status
// page, as freestanding builders.

#include "../ui/ToyboxScreen.h"
#include "MarketsCore.h"

namespace marketsui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionOpen = 1,  // value: markets::Asset
  ActionRefresh,
  ActionAuto,
  ActionChooseAuto,  // value: minutes, 0 for off
  ActionSpan,        // value: Span
  ActionDismiss,
  ActionPortfolio,
  ActionChange,    // the open card shows something else
  ActionPick,      // value: an instrument from the catalogue
  ActionPosition,  // value: a holding's index, to change its amount
  ActionAddPosition,
  ActionKey,  // value: the character on the key
  ActionSaveAmount,
  ActionRemovePosition,
  ActionPeriod,  // value: markets::Period, for the portfolio's change
  ActionSearch,  // type a ticker or a name
  ActionFound,   // value: a search result, or -1 for what was typed as it stands
};

// The slice of history the detail chart shows.
enum class Span : uint8_t { Week = 0, Month = 1, Quarter = 2, Year = 3 };

// The minutes the auto-refresh picker offers; 0 is off.
constexpr int kAutoChoices[] = {0, 5, 15, 30, 60};
constexpr int kAutoChoiceCount = sizeof(kAutoChoices) / sizeof(kAutoChoices[0]);

// The builder cannot reach the renderer, and prices are set in cuts bigger
// than the three the target holds. The activity lends it this to put one of
// them in the title slot for a string and take it back.
enum class Face : uint8_t { Display, Large, Huge };
struct FaceSwap {
  void* ctx = nullptr;
  void (*bindTitle)(void* ctx, Face face) = nullptr;
};

struct DashboardModel {
  const markets::Quote* quotes = nullptr;  // kAssets of them
  int autoMinutes = 0;
  FaceSwap faces;
};

struct DetailModel {
  markets::Asset asset = markets::Asset::Bitcoin;
  const markets::Quote* quote = nullptr;
  Span span = Span::Month;
  FaceSwap faces;
};

struct StatusModel {
  const char* headline = "";
  const char* detail = "";
  bool dismissable = false;
};

struct PortfolioModel {
  const markets::Portfolio* portfolio = nullptr;
  const markets::Priced* priced = nullptr;  // one for each holding
  double euro = 0;                          // a euro in dollars; 0 when unknown
  markets::Period period = markets::Period::Day;
  FaceSwap faces;
};

// The catalogue as a page of buttons: every instrument for a card, only what
// can be held for the portfolio.
struct PickerModel {
  const char* title = "";
  bool ownableOnly = false;
  int current = -1;                          // drawn selected
  const markets::Portfolio* held = nullptr;  // these are left out
};

struct AmountModel {
  int instrument = 0;
  const char* text = "";  // what has been typed
  bool held = false;      // already in the portfolio: REMOVE is offered
  FaceSwap faces;
};

// What a search brought back, and the offer to take what was typed as a symbol.
struct ResultsModel {
  const char* query = "";
  const markets::Found* found = nullptr;
  int count = 0;  // -1 when the lookup did not answer
  bool asTyped = false;
};

void buildResults(toybox::Screen& screen, const ResultsModel& model);
void buildPortfolio(toybox::Screen& screen, const PortfolioModel& model);
void buildPicker(toybox::Screen& screen, const PickerModel& model);
void buildAmount(toybox::Screen& screen, const AmountModel& model);

void buildDashboard(toybox::Screen& screen, const DashboardModel& model);
void buildDetail(toybox::Screen& screen, const DetailModel& model);
void buildAutoPicker(toybox::Screen& screen, int currentMinutes);
void buildStatus(toybox::Screen& screen, const StatusModel& model);

// How many closes a span covers for an instrument: a week is five sessions on
// an exchange and seven days for what trades around the clock.
int sessionsIn(Span span, markets::Asset asset, const markets::Quote& quote);

}  // namespace marketsui
