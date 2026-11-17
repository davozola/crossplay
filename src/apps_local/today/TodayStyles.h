#pragma once

// HOY's faces. The same day's data drawn ten ways, to be swiped between: two
// in the firmware's own pixel hand, and eight that leave it for the reader's
// serif and sans and for drawn dials.

#include <ctime>

#include "../ui/ToyboxScreen.h"
#include "TodayStore.h"

class GfxRenderer;

namespace todaystyles {

constexpr int kStyles = 10;
const char* name(int style);

// What a part of the page leads to when it is tapped.
// Auto is the foot's left corner: how often the page fetches by itself.
enum Target : uint8_t { None = 0, Clock, Weather, Markets, Sea, Event, Refresh, Auto };
// A tapped part arrives as this action plus its Target.
constexpr freeink::ui::ActionId kTargetAction = 40;

struct Page {
  const todaystore::Snapshot& data;
  bool clockSet = false;
  struct tm now = {};
  // What happened on days like this one, the best of each kind; none when not fetched.
  struct Event {
    const char* text = "";
    int year = 0;
  };
  static constexpr int kEvents = 3;
  Event events[kEvents];
  int eventCount = 0;
  // Minutes between fetches the page makes by itself, 0 for never.
  int autoMinutes = 0;
};

// Draws the page in a style, with its tappable parts and the foot they share:
// when the data is from, which style this is, and the ring that fetches again.
void draw(int style, toybox::Screen& screen, GfxRenderer& renderer, const Page& page);

}  // namespace todaystyles
