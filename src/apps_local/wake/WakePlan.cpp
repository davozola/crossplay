#include "WakePlan.h"

namespace wakeplan {

namespace {

// A job keeps the wait it has unless it has none, or more than one interval:
// the second means the interval was shortened since.
void schedule(uint32_t& left, const uint32_t every, const bool restart) {
  if (every == 0) {
    left = 0;
    return;
  }
  if (restart || left == 0 || left > every) left = every;
}

uint32_t floored(const uint32_t left) { return left == 0 ? 0 : (left < kFloor ? kFloor : left); }

uint32_t soonest(const uint32_t a, const uint32_t b) {
  if (a == 0) return b;
  if (b == 0) return a;
  return a < b ? a : b;
}

// Counts `elapsed` off a job; true when that brings it due, and then it is
// given its next interval.
bool count(uint32_t& left, const uint32_t every, const uint32_t elapsed) {
  if (left == 0) return false;
  if (left <= elapsed + kSlack) {
    left = every;
    return true;
  }
  left -= elapsed;
  return false;
}

}  // namespace

uint32_t arm(State& state, const uint32_t frameEvery, const uint32_t casaEvery, const uint32_t liveSeconds,
             const bool fresh) {
  if (state.magic != kMagic) {
    state = State{};
    state.magic = kMagic;
  }
  state.frameEvery = frameEvery;
  state.casaEvery = casaEvery;
  schedule(state.frameLeft, frameEvery, fresh);
  schedule(state.casaLeft, casaEvery, false);
  return rearm(state, liveSeconds);
}

uint32_t rearm(State& state, const uint32_t liveSeconds) {
  if (state.magic != kMagic) return liveSeconds;
  state.armed = soonest(liveSeconds, soonest(floored(state.frameLeft), floored(state.casaLeft)));
  return state.armed;
}

Due onTimer(State& state) {
  Due due = {false, false};
  if (state.magic != kMagic) return due;
  const uint32_t elapsed = state.armed;
  state.armed = 0;
  if (elapsed == 0) return due;
  due.frame = count(state.frameLeft, state.frameEvery, elapsed);
  due.casa = count(state.casaLeft, state.casaEvery, elapsed);
  return due;
}

}  // namespace wakeplan
