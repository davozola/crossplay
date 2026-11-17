#include <cstdio>

#include "WakePlan.h"

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

using namespace wakeplan;

int main() {
  {  // nothing on: Live's number passes through, and 0 stays 0
    State s = {};
    CHECK(arm(s, 0, 0, 0, true) == 0);
    CHECK(arm(s, 0, 0, 900, true) == 900);
  }
  {  // the frame alone: armed for its interval, due when that timer fires, and again after
    State s = {};
    CHECK(arm(s, 3600, 0, 0, true) == 3600);
    Due d = onTimer(s);
    CHECK(d.frame && !d.casa);
    // The unattended sleep that follows the repaint arms a whole interval again.
    CHECK(arm(s, 3600, 0, 0, false) == 3600);
    CHECK(onTimer(s).frame);
    CHECK(arm(s, 3600, 0, 0, false) == 3600);
    CHECK(onTimer(s).frame);
  }
  {  // no clock is read: twelve hours of hourly wakes are twelve pictures
    State s = {};
    arm(s, 3600, 0, 0, true);
    int pictures = 0;
    for (int hour = 0; hour < 12; ++hour) {
      if (onTimer(s).frame) ++pictures;
      arm(s, 3600, 0, 0, false);
    }
    CHECK(pictures == 12);
  }
  {  // Live wakes sooner: the frame is not due then, and keeps the rest of its wait
    State s = {};
    CHECK(arm(s, 3600, 0, 600, true) == 600);
    CHECK(!onTimer(s).frame);
    CHECK(rearm(s, 0) == 3000);
    CHECK(onTimer(s).frame);
  }
  {  // two jobs: the sooner arms the timer, and each comes due on its own count
    State s = {};
    CHECK(arm(s, 3600, 1800, 0, true) == 1800);
    Due d = onTimer(s);
    CHECK(d.casa && !d.frame);
    CHECK(rearm(s, 0) == 1800);
    d = onTimer(s);
    CHECK(d.casa && d.frame);
  }
  {  // a job within the slack of the alarm is done on that wake
    State s = {};
    arm(s, 900, 890, 0, true);
    const Due d = onTimer(s);
    CHECK(d.casa && d.frame);
  }
  {  // a person's sleep starts the frame's interval over; an unattended one does not
    State s = {};
    arm(s, 3600, 0, 600, true);
    onTimer(s);
    CHECK(arm(s, 3600, 0, 0, false) == 3000);
    CHECK(arm(s, 3600, 0, 0, true) == 3600);
  }
  {  // a shorter interval chosen since takes effect at the next sleep
    State s = {};
    arm(s, 3600, 0, 0, true);
    CHECK(arm(s, 900, 0, 0, false) == 900);
  }
  {  // turning a job off unschedules it
    State s = {};
    arm(s, 900, 900, 0, true);
    CHECK(arm(s, 0, 900, 0, false) == 900);
    const Due d = onTimer(s);
    CHECK(d.casa && !d.frame);
  }
  {  // a wait shorter than the floor is still a whole floor of sleep
    State s = {};
    arm(s, 900, 0, 875, true);
    CHECK(!onTimer(s).frame);
    CHECK(rearm(s, 0) == kFloor);
  }
  {  // a timer nothing armed, and a state nothing wrote, are nobody's job
    State s = {};
    CHECK(rearm(s, 77) == 77);
    CHECK(!onTimer(s).frame);
    arm(s, 900, 0, 0, true);
    onTimer(s);
    CHECK(!onTimer(s).frame);
  }
  if (failures == 0) std::printf("wake: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
