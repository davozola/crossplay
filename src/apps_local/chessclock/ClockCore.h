#pragma once

// A two-player game clock with Fischer increment. Freestanding: the activity
// owns millis() and hands the time in, so host-tests/chessclock can play a
// whole game in microseconds.
//
// Side 0 is the bottom half of the screen, side 1 the top. As on a real clock,
// you press your own side when you have moved, which stops yours, adds your
// increment, and starts your opponent's. Before the first press nothing runs:
// whoever presses first starts the other clock, so black presses to start
// white's.

#include <cstdint>

namespace chessclock {

struct Preset {
  uint16_t minutes;
  uint8_t increment;  // seconds added after each of your moves
};

constexpr Preset kPresets[] = {{1, 0}, {3, 2}, {5, 0}, {10, 0}, {15, 10}, {30, 0}};
constexpr int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);
constexpr int kDefaultPreset = 2;

enum class State : uint8_t { Ready, Running, Paused, Flagged };

class Clock {
 public:
  void reset(int preset);

  int preset() const { return preset_; }
  State state() const { return state_; }
  // Whose clock is (or, when paused, was) running; -1 before the first press.
  int active() const { return active_; }
  // Which side ran out, or -1.
  int flagged() const { return flagged_; }
  int moves(int side) const { return moves_[side & 1]; }

  uint32_t remainingMs(int side, uint32_t nowMs) const;

  // A press on `side`'s half. Returns true when it changed anything, so the
  // caller knows to repaint. Ignored when it is not that side's turn.
  bool press(int side, uint32_t nowMs);
  // Pause and resume. Pausing a clock that is not running does nothing.
  bool togglePause(uint32_t nowMs);
  // Call every loop pass. True exactly once, on the pass a flag falls.
  bool tick(uint32_t nowMs);

  // For saving and restoring a paused game.
  void restore(int preset, int active, const uint32_t remaining[2], const uint16_t moves[2], State state);
  uint32_t storedMs(int side) const { return remaining_[side & 1]; }

 private:
  void settle(uint32_t nowMs);

  int preset_ = kDefaultPreset;
  State state_ = State::Ready;
  int active_ = -1;
  int flagged_ = -1;
  uint32_t remaining_[2] = {0, 0};
  uint16_t moves_[2] = {0, 0};
  // When the active side's clock last started counting.
  uint32_t since_ = 0;
};

// What a side's display shows, and so when it needs repainting.
//
// With plenty on the clock it shows whole minutes, rounded up; in the last
// minute, or always when `alwaysSeconds` is set, minutes and seconds, the
// seconds rounded up so a clock never reads 0:00 while it is still running.
struct Shown {
  int minutes = 0;
  int seconds = 0;
  bool withSeconds = false;
};

Shown shown(uint32_t remainingMs, bool alwaysSeconds);
inline bool operator==(const Shown& a, const Shown& b) {
  return a.minutes == b.minutes && a.seconds == b.seconds && a.withSeconds == b.withSeconds;
}
inline bool operator!=(const Shown& a, const Shown& b) { return !(a == b); }

}  // namespace chessclock
