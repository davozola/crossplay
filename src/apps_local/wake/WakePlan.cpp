#include "WakePlan.h"

namespace wakeplan {

namespace {

// A due time is kept only while it is still ahead and no further off than
// one interval: anything else means the clock moved or the interval changed.
void schedule(int64_t& due, const uint32_t every, const int64_t now) {
  if (every == 0) {
    due = 0;
    return;
  }
  if (due <= now || due - now > static_cast<int64_t>(every)) due = now + static_cast<int64_t>(every);
}

uint32_t until(const int64_t due, const int64_t now) {
  if (due == 0) return 0;
  const int64_t left = due - now;
  return left < static_cast<int64_t>(kFloor) ? kFloor : static_cast<uint32_t>(left);
}

uint32_t soonest(const uint32_t a, const uint32_t b) {
  if (a == 0) return b;
  if (b == 0) return a;
  return a < b ? a : b;
}

}  // namespace

uint32_t arm(State& state, const uint32_t frameEvery, const uint32_t casaEvery, const int64_t now,
             const uint32_t liveSeconds) {
  if (state.magic != kMagic) {
    state = State{};
    state.magic = kMagic;
  }
  state.frameEvery = frameEvery;
  state.casaEvery = casaEvery;
  schedule(state.frameDue, frameEvery, now);
  schedule(state.casaDue, casaEvery, now);
  return rearm(state, now, liveSeconds);
}

uint32_t rearm(const State& state, const int64_t now, const uint32_t liveSeconds) {
  if (state.magic != kMagic) return liveSeconds;
  return soonest(liveSeconds, soonest(until(state.frameDue, now), until(state.casaDue, now)));
}

Due onTimer(State& state, const int64_t now) {
  Due due = {false, false};
  if (state.magic != kMagic) return due;
  if (state.frameDue != 0 && now + kSlack >= state.frameDue) {
    due.frame = true;
    state.frameDue = 0;
  }
  if (state.casaDue != 0 && now + kSlack >= state.casaDue) {
    due.casa = true;
    state.casaDue = 0;
  }
  return due;
}

}  // namespace wakeplan
