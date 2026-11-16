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
    CHECK(arm(s, 0, 0, 1000, 0) == 0);
    CHECK(arm(s, 0, 0, 1000, 900) == 900);
    CHECK(s.frameDue == 0 && s.casaDue == 0);
  }
  {  // one job: armed for its interval, and due when the timer fires
    State s = {};
    CHECK(arm(s, 1800, 0, 1000, 0) == 1800);
    Due d = onTimer(s, 2800);
    CHECK(d.frame && !d.casa);
    CHECK(s.frameDue == 0);
    CHECK(arm(s, 1800, 0, 2805, 0) == 1800);  // a fresh interval after the job
  }
  {  // a wake that is somebody else's leaves the job where it was
    State s = {};
    arm(s, 3600, 0, 0, 600);
    CHECK(rearm(s, 0, 600) == 600);
    Due d = onTimer(s, 600);
    CHECK(!d.frame && !d.casa);
    CHECK(arm(s, 3600, 0, 600, 0) == 3000);
  }
  {  // two jobs: the sooner wins, and the later keeps its time
    State s = {};
    CHECK(arm(s, 3600, 1800, 0, 0) == 1800);
    Due d = onTimer(s, 1800);
    CHECK(!d.frame && d.casa);
    CHECK(arm(s, 3600, 1800, 1800, 0) == 1800);
    d = onTimer(s, 3600);
    CHECK(d.frame && d.casa);
  }
  {  // the slack: a few seconds early still counts
    State s = {};
    arm(s, 900, 0, 0, 0);
    CHECK(onTimer(s, 885).frame);
  }
  {  // a job overdue at re-arm time asks for the floor, not for zero
    State s = {};
    arm(s, 900, 0, 0, 0);
    CHECK(rearm(s, 5000, 0) == kFloor);
  }
  {  // the clock jumped forward by years: rescheduled from now
    State s = {};
    arm(s, 900, 0, 100, 0);
    CHECK(arm(s, 900, 0, 1700000000, 0) == 900);
  }
  {  // the clock jumped back: a due time further off than one interval is redone
    State s = {};
    arm(s, 900, 0, 1700000000, 0);
    CHECK(arm(s, 900, 0, 100, 0) == 900);
  }
  {  // turning a job off unschedules it
    State s = {};
    arm(s, 900, 900, 0, 0);
    CHECK(arm(s, 0, 900, 10, 0) == 890);
    CHECK(s.frameDue == 0);
  }
  {  // state from another build is ignored
    State s = {};
    s.magic = 123;
    s.frameDue = 5;
    CHECK(rearm(s, 0, 77) == 77);
    CHECK(!onTimer(s, 100).frame);
  }
  if (failures == 0) std::printf("wake: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
