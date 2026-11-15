#include "PomodoroCore.h"

namespace pomodoro {

namespace {

template <int N>
int nextChoice(const int (&choices)[N], int current) {
  for (int i = 0; i < N; ++i) {
    if (choices[i] == current) return choices[(i + 1) % N];
  }
  return choices[0];
}

}  // namespace

uint32_t Timer::phaseLengthMs() const {
  int minutes = focusMinutes;
  if (phase_ == Phase::ShortBreak) minutes = breakMinutes;
  if (phase_ == Phase::LongBreak) minutes = breakMinutes * kLongBreakFactor;
  return static_cast<uint32_t>(minutes) * 60u * 1000u;
}

uint32_t Timer::remainingMs(const uint32_t nowMs) const {
  uint32_t elapsed = 0;
  if (state_ == State::Running) elapsed = nowMs - startMs_;  // unsigned: survives the millis() wrap
  if (state_ == State::Paused) elapsed = elapsedAtPauseMs_;
  const uint32_t length = phaseLengthMs();
  return elapsed >= length ? 0 : length - elapsed;
}

int Timer::remainingMinutes(const uint32_t nowMs) const {
  const uint32_t ms = remainingMs(nowMs);
  return static_cast<int>((ms + 59999u) / 60000u);
}

int Timer::progressPermille(const uint32_t nowMs) const {
  const uint32_t length = phaseLengthMs();
  if (length == 0) return 1000;
  const uint32_t done = length - remainingMs(nowMs);
  return static_cast<int>(static_cast<uint64_t>(done) * 1000u / length);
}

void Timer::start(const uint32_t nowMs) {
  if (state_ == State::Running) return;
  const uint32_t already = state_ == State::Paused ? elapsedAtPauseMs_ : 0;
  startMs_ = nowMs - already;
  elapsedAtPauseMs_ = 0;
  state_ = State::Running;
}

void Timer::pause(const uint32_t nowMs) {
  if (state_ != State::Running) return;
  elapsedAtPauseMs_ = nowMs - startMs_;
  state_ = State::Paused;
}

void Timer::toggle(const uint32_t nowMs) {
  if (state_ == State::Running) {
    pause(nowMs);
  } else {
    start(nowMs);
  }
}

bool Timer::tick(const uint32_t nowMs) {
  if (state_ != State::Running) return false;
  if (remainingMs(nowMs) > 0) return false;
  advance(true);
  return true;
}

void Timer::skip() { advance(false); }

void Timer::reset() {
  phase_ = Phase::Focus;
  state_ = State::Ready;
  doneInCycle_ = 0;
  elapsedAtPauseMs_ = 0;
}

void Timer::restore(const Phase phase, const int doneInCycle) {
  phase_ = phase == Phase::ShortBreak || phase == Phase::LongBreak ? phase : Phase::Focus;
  doneInCycle_ = doneInCycle < 0 ? 0 : (doneInCycle > kSessionsPerCycle ? kSessionsPerCycle : doneInCycle);
  // A long break is only owed after a full cycle; anything else is a cycle in
  // progress and the long break would be a lie.
  if (phase_ == Phase::LongBreak && doneInCycle_ < kSessionsPerCycle) phase_ = Phase::ShortBreak;
  state_ = State::Ready;
  elapsedAtPauseMs_ = 0;
}

void Timer::advance(const bool countFocus) {
  state_ = State::Ready;
  elapsedAtPauseMs_ = 0;
  if (phase_ == Phase::Focus) {
    if (countFocus) ++totalDone_;
    // A skipped focus still moves the cycle along: the break you skipped to is
    // the break you were owed, and the dots are where you are, not a score.
    ++doneInCycle_;
    if (doneInCycle_ >= kSessionsPerCycle) {
      phase_ = Phase::LongBreak;
    } else {
      phase_ = Phase::ShortBreak;
    }
  } else {
    if (phase_ == Phase::LongBreak) doneInCycle_ = 0;
    phase_ = Phase::Focus;
  }
}

void Timer::cycleFocus() { focusMinutes = nextChoice(kFocusChoices, focusMinutes); }
void Timer::cycleBreak() { breakMinutes = nextChoice(kBreakChoices, breakMinutes); }

const char* phaseLabel(const Phase phase) {
  switch (phase) {
    case Phase::ShortBreak:
      return "SHORT BREAK";
    case Phase::LongBreak:
      return "LONG BREAK";
    case Phase::Focus:
    default:
      return "FOCUS";
  }
}

}  // namespace pomodoro
