#pragma once

// INNER CLOCK: tap to start, tap again when you think 5 (or 10) seconds have
// passed, and see how far off you were. Freestanding: the activity hands the
// time in, so host-tests/innerclock checks the arithmetic and the formatting.

#include <cstdint>

namespace innerclock {

constexpr int kTargets[] = {5, 10};
constexpr int kTargetCount = 2;
constexpr int kRecent = 5;

enum class State : uint8_t { Ready, Running, Result };

struct Attempt {
  uint8_t target = 5;
  // Elapsed minus target, in milliseconds. Negative is early.
  int32_t diffMs = 0;
};

// The record kept on the card.
struct Record {
  // Closest attempt so far for each target, as |diff| in ms; -1 when none.
  int32_t bestMs[kTargetCount] = {-1, -1};
  uint32_t tries = 0;
  int recentCount = 0;
  // Newest first.
  Attempt recent[kRecent];
};

class Round {
 public:
  State state() const { return state_; }
  int target() const { return kTargets[targetIndex_]; }
  int targetIndex() const { return targetIndex_; }
  void setTarget(int index);

  void start(uint32_t nowMs);
  // Ends the round and files it in `record`. Returns false (and changes
  // nothing) unless a round was running.
  bool stop(uint32_t nowMs, Record& record);
  // Back to Ready, keeping the target.
  void clear() { state_ = State::Ready; }

  uint32_t elapsedMs() const { return elapsedMs_; }
  int32_t diffMs() const { return static_cast<int32_t>(elapsedMs_) - target() * 1000; }
  bool newBest() const { return newBest_; }

 private:
  State state_ = State::Ready;
  int targetIndex_ = 0;
  uint32_t startMs_ = 0;
  uint32_t elapsedMs_ = 0;
  bool newBest_ = false;
};

// "5.23" from 5234 ms. Truncated to hundredths, never rounded up, so a tap at
// 4.999 s does not claim a bullseye. `out` needs 16.
void formatSeconds(uint32_t ms, char* out, int outSize);
// "+0.23" / "-0.41" / "0.00".
void formatDiff(int32_t ms, char* out, int outSize);

// A word for how close it was.
const char* verdict(int32_t diffMs);

}  // namespace innerclock
