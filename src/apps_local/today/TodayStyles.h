#pragma once

// HOY's faces. The same day's data drawn five ways, to be swiped between: two
// in the firmware's own pixel hand, and three that leave it for the reader's
// serif and sans and for drawn dials.

#include <ctime>

#include "../ui/ToyboxScreen.h"
#include "TodayStore.h"

class GfxRenderer;

namespace todaystyles {

constexpr int kStyles = 5;
const char* name(int style);

// What a part of the page leads to when it is tapped.
enum Target : uint8_t { None = 0, Clock, Weather, Markets, Sea, Event, Refresh };
// A tapped part arrives as this action plus its Target.
constexpr freeink::ui::ActionId kTargetAction = 40;

struct Page {
  const todaystore::Snapshot& data;
  bool clockSet = false;
  struct tm now = {};
  const char* eventText = "";  // what happened on a day like this; empty when not fetched
  int eventYear = 0;
};

// Draws the page in a style, with its tappable parts and the foot they share:
// when the data is from, which style this is, and the ring that fetches again.
void draw(int style, toybox::Screen& screen, GfxRenderer& renderer, const Page& page);

}  // namespace todaystyles
