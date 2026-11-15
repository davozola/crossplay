#include "ClockCore.h"

namespace chessclock {

void Clock::reset(const int preset) {
  preset_ = preset >= 0 && preset < kPresetCount ? preset : kDefaultPreset;
  const uint32_t start = static_cast<uint32_t>(kPresets[preset_].minutes) * 60u * 1000u;
  remaining_[0] = remaining_[1] = start;
  moves_[0] = moves_[1] = 0;
  state_ = State::Ready;
  active_ = -1;
  flagged_ = -1;
  since_ = 0;
}

void Clock::settle(const uint32_t nowMs) {
  if (state_ != State::Running || active_ < 0) return;
  const uint32_t spent = nowMs - since_;  // unsigned: survives the millis() wrap
  remaining_[active_] = spent >= remaining_[active_] ? 0 : remaining_[active_] - spent;
  since_ = nowMs;
}

uint32_t Clock::remainingMs(const int side, const uint32_t nowMs) const {
  const int s = side & 1;
  if (state_ != State::Running || s != active_) return remaining_[s];
  const uint32_t spent = nowMs - since_;
  return spent >= remaining_[s] ? 0 : remaining_[s] - spent;
}

bool Clock::press(const int side, const uint32_t nowMs) {
  const int s = side & 1;
  switch (state_) {
    case State::Ready:
      // The first press starts the other side; it is not a move.
      active_ = 1 - s;
      state_ = State::Running;
      since_ = nowMs;
      return true;
    case State::Running: {
      if (s != active_) return false;
      settle(nowMs);
      if (remaining_[s] == 0) return false;  // tick() will flag it
      remaining_[s] += static_cast<uint32_t>(kPresets[preset_].increment) * 1000u;
      ++moves_[s];
      active_ = 1 - s;
      since_ = nowMs;
      return true;
    }
    case State::Paused:
    case State::Flagged:
      return false;
  }
  return false;
}

bool Clock::togglePause(const uint32_t nowMs) {
  if (state_ == State::Running) {
    settle(nowMs);
    state_ = State::Paused;
    return true;
  }
  if (state_ == State::Paused) {
    state_ = State::Running;
    since_ = nowMs;
    return true;
  }
  return false;
}

bool Clock::tick(const uint32_t nowMs) {
  if (state_ != State::Running || active_ < 0) return false;
  if (remainingMs(active_, nowMs) > 0) return false;
  settle(nowMs);
  state_ = State::Flagged;
  flagged_ = active_;
  return true;
}

void Clock::restore(const int preset, const int active, const uint32_t remaining[2], const uint16_t moves[2],
                    const State state) {
  reset(preset);
  remaining_[0] = remaining[0];
  remaining_[1] = remaining[1];
  moves_[0] = moves[0];
  moves_[1] = moves[1];
  active_ = active == 0 || active == 1 ? active : -1;
  // A restored clock never comes back running: millis() means nothing across
  // a sleep, so it waits paused (or ready, before the first press).
  state_ = active_ < 0 ? State::Ready : (state == State::Flagged ? State::Flagged : State::Paused);
  if (state_ == State::Flagged) flagged_ = remaining_[0] == 0 ? 0 : (remaining_[1] == 0 ? 1 : -1);
  if (state_ == State::Flagged && flagged_ < 0) state_ = State::Paused;
}

Shown shown(const uint32_t remainingMs, const bool alwaysSeconds) {
  Shown s;
  const uint32_t totalSeconds = (remainingMs + 999u) / 1000u;
  if (alwaysSeconds || totalSeconds <= 60u) {
    s.withSeconds = true;
    s.minutes = static_cast<int>(totalSeconds / 60u);
    s.seconds = static_cast<int>(totalSeconds % 60u);
    return s;
  }
  s.minutes = static_cast<int>((remainingMs + 59999u) / 60000u);
  return s;
}

}  // namespace chessclock
