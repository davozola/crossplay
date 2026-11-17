#pragma once

// When a sleeping device should wake itself, for the jobs this fork runs
// with nobody there: changing the picture on the glass (the photo frame) and
// noting the room's temperature (CASA). Live has its own schedule; this takes
// Live's number and returns the soonest of the three.
//
// Freestanding: the state lives in RTC memory on the device and in a plain
// struct in the tests. It keeps no clock times. Each job has the seconds it
// still has to wait, and a timer that ends a sleep is known to have run for
// exactly what it was armed with, so the plan counts down by its own alarms
// and does not care what the system clock does across a sleep.

#include <cstdint>

namespace wakeplan {

constexpr uint32_t kMagic = 0x57414B33;  // "WAK3"
// A job this close to its time when the timer fires is done on that wake
// rather than on one of its own a moment later.
constexpr uint32_t kSlack = 20;
// Never arm for less than this: a wake costs a boot.
constexpr uint32_t kFloor = 30;

struct State {
  uint32_t magic;
  uint32_t frameEvery;  // seconds between pictures, 0 = off
  uint32_t casaEvery;   // seconds between readings, 0 = off
  uint32_t frameLeft;   // seconds of sleep until the next picture, 0 = not scheduled
  uint32_t casaLeft;
  uint32_t armed;  // what the timer was last set to, 0 = not set
};

struct Due {
  bool frame;
  bool casa;
};

// Called on the way into a sleep, with what the settings say now. Schedules
// what is not scheduled and returns the seconds to arm the timer for, 0 for
// none. `liveSeconds` is Live's own request, 0 when it has none. `fresh` is a
// sleep a person began: the sleep screen has just drawn a new picture, so the
// frame's interval starts over.
uint32_t arm(State& state, uint32_t frameEvery, uint32_t casaEvery, uint32_t liveSeconds, bool fresh);

// The same answer from the state alone, for a sleep that begins before the
// card can be read. Records what the timer is armed for and nothing else.
uint32_t rearm(State& state, uint32_t liveSeconds);

// Called when the timer ended the sleep: counts the armed time off every job
// and says which are due. A due job starts its next interval at once.
Due onTimer(State& state);

}  // namespace wakeplan
