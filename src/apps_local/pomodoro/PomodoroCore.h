#pragma once

// The Pomodoro rules, with no Arduino in them: the activity owns millis() and
// hands the time in, so host-tests/pomodoro can drive a whole day of sessions
// in microseconds.
//
// One cycle is four focus sessions; a short break follows each of the first
// three and a long break follows the fourth. A finished phase never starts the
// next one by itself -- the device is usually face up on a desk and nobody has
// asked it to begin anything -- it lands on the next phase, stopped, and waits
// for a tap.

#include <cstdint>

namespace pomodoro {

enum class Phase : uint8_t { Focus, ShortBreak, LongBreak };
enum class State : uint8_t { Ready, Running, Paused };

constexpr int kSessionsPerCycle = 4;

// The lengths the two pickers offer, in minutes.
constexpr int kFocusChoices[] = {5, 10, 15, 20, 25, 30, 45, 50, 60};
constexpr int kFocusChoiceCount = sizeof(kFocusChoices) / sizeof(kFocusChoices[0]);
constexpr int kBreakChoices[] = {3, 5, 10, 15};
constexpr int kBreakChoiceCount = sizeof(kBreakChoices) / sizeof(kBreakChoices[0]);
constexpr int kDefaultFocus = 25;
constexpr int kDefaultBreak = 5;
// The long break is the short one times three: 5 -> 15, 10 -> 30.
constexpr int kLongBreakFactor = 3;

class Timer {
 public:
  int focusMinutes = kDefaultFocus;
  int breakMinutes = kDefaultBreak;

  Phase phase() const { return phase_; }
  State state() const { return state_; }
  // Focus sessions finished in the current cycle, 0..3. Wraps after a long break.
  int doneInCycle() const { return doneInCycle_; }
  // Focus sessions finished since the counter was last cleared.
  uint32_t totalDone() const { return totalDone_; }
  void setTotalDone(uint32_t n) { totalDone_ = n; }
  // Puts back where a saved cycle was, stopped.
  void restore(Phase phase, int doneInCycle);
  // How much of the phase has gone by: what is saved when the app is left.
  uint32_t elapsedMs(uint32_t nowMs) const;
  // After restore(): the clock picks up `elapsedMs` into the phase, running or
  // paused. The caller has already added the time spent away, so a phase that
  // ended meanwhile is Running past its length and the next tick() finishes it.
  void resume(bool running, uint32_t elapsedMs, uint32_t nowMs);

  uint32_t phaseLengthMs() const;
  uint32_t remainingMs(uint32_t nowMs) const;
  // Minutes left, rounded UP, so a fresh 25 reads 25 and the final seconds read 1.
  int remainingMinutes(uint32_t nowMs) const;
  // 0..1000, how much of the phase has elapsed.
  int progressPermille(uint32_t nowMs) const;

  void start(uint32_t nowMs);
  void pause(uint32_t nowMs);
  // Start, pause or resume, whichever the one big button means right now.
  void toggle(uint32_t nowMs);

  // True exactly once, on the call where a running phase reaches zero. The
  // timer has then already moved to the next phase, Ready.
  bool tick(uint32_t nowMs);

  // Abandons the current phase and moves to the next one, Ready. Skipping a
  // focus session does not count it.
  void skip();
  // Back to the first focus session of a fresh cycle, Ready. Keeps totalDone.
  void reset();

  // False, and nothing changes, for a length the pickers do not offer.
  bool setFocus(int minutes);
  bool setBreak(int minutes);

 private:
  void advance(bool countFocus);

  Phase phase_ = Phase::Focus;
  State state_ = State::Ready;
  int doneInCycle_ = 0;
  uint32_t totalDone_ = 0;
  // While Running: the millis() the phase would have started at had it never
  // been paused. While Paused: how much of it had elapsed.
  uint32_t startMs_ = 0;
  uint32_t elapsedAtPauseMs_ = 0;
};

const char* phaseLabel(Phase phase);

}  // namespace pomodoro
