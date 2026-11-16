#pragma once

// When a sleeping device should wake itself, for the jobs this fork runs
// with nobody there: changing the picture on the glass (the photo frame) and
// noting the room's temperature (CASA). Live has its own schedule; this takes
// Live's number and returns the soonest of the three.
//
// Freestanding: the state lives in RTC memory on the device and in a plain
// struct in the tests. Times are seconds on whatever clock the caller has; it
// only has to keep counting across a sleep, it does not have to be right.

#include <cstdint>

namespace wakeplan {

constexpr uint32_t kMagic = 0x57414B32;  // "WAK2"
// A wake this close to a job's time does the job rather than waking again.
constexpr int64_t kSlack = 20;
// Never arm for less than this: a wake costs a boot.
constexpr uint32_t kFloor = 30;

struct State {
  uint32_t magic;
  uint32_t frameEvery;  // seconds between pictures, 0 = off
  uint32_t casaEvery;   // seconds between readings, 0 = off
  int64_t frameDue;     // 0 = not scheduled
  int64_t casaDue;
};

struct Due {
  bool frame;
  bool casa;
};

// Called on the way into a sleep, with what the settings say now. Schedules
// what is not scheduled and returns the seconds to arm the timer for, 0 for
// none. `liveSeconds` is Live's own request, 0 when it has none.
uint32_t arm(State& state, uint32_t frameEvery, uint32_t casaEvery, int64_t now, uint32_t liveSeconds);

// The same answer from the state alone, for a sleep that begins before the
// card can be read. Changes nothing.
uint32_t rearm(const State& state, int64_t now, uint32_t liveSeconds);

// Called when the timer ended the sleep: which jobs are due. A due job is
// unscheduled, so the next arm() gives it a fresh interval.
Due onTimer(State& state, int64_t now);

}  // namespace wakeplan
