#pragma once

// The device side of WakePlan: reads what the frame and CASA are set to,
// keeps the plan in memory that survives deep sleep, and does the jobs.
// main.cpp calls the three functions at the top; the Wallpapers app sets the
// frame's interval through the rest.

#include <cstdint>

namespace wakejobs {

// On the way into a sleep, while the card can still be read. `attended` is a
// sleep a person began: the room is noted on the way down. Returns the seconds
// to arm the timer for (0 = none), given Live's own request.
uint32_t armSeconds(uint32_t liveSeconds, bool attended);

// The same for a sleep that begins after the card was put away.
uint32_t rearmSeconds(uint32_t liveSeconds);

// The timer ended a sleep. Takes the room's reading if that is due, and
// returns true when the picture on the glass should change.
bool onTimerWake();

// The photo frame: how often a sleeping device changes its picture.
constexpr int kFrameChoices = 7;
int frameChoice();
void setFrameChoice(int choice);
const char* frameLabel(int choice);  // "OFF", "15M", "1H" ...
uint32_t frameSeconds(int choice);

}  // namespace wakejobs
