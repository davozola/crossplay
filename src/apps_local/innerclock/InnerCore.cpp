#include "InnerCore.h"

#include <cstdio>

namespace innerclock {

void Round::setTarget(const int index) {
  if (state_ == State::Running) return;
  targetIndex_ = index >= 0 && index < kTargetCount ? index : 0;
}

void Round::start(const uint32_t nowMs) {
  startMs_ = nowMs;
  elapsedMs_ = 0;
  newBest_ = false;
  state_ = State::Running;
}

bool Round::stop(const uint32_t nowMs, Record& record) {
  if (state_ != State::Running) return false;
  elapsedMs_ = nowMs - startMs_;  // unsigned: survives the millis() wrap
  state_ = State::Result;
  const int32_t d = diffMs();
  const int32_t off = d < 0 ? -d : d;
  int32_t& best = record.bestMs[targetIndex_];
  newBest_ = best < 0 || off < best;
  if (newBest_) best = off;
  ++record.tries;
  for (int i = kRecent - 1; i > 0; --i) record.recent[i] = record.recent[i - 1];
  record.recent[0] = Attempt{static_cast<uint8_t>(target()), d};
  if (record.recentCount < kRecent) ++record.recentCount;
  return true;
}

void formatSeconds(const uint32_t ms, char* out, const int outSize) {
  const uint32_t centis = ms / 10u;
  std::snprintf(out, static_cast<size_t>(outSize), "%lu.%02lu", static_cast<unsigned long>(centis / 100u),
                static_cast<unsigned long>(centis % 100u));
}

void formatDiff(const int32_t ms, char* out, const int outSize) {
  const uint32_t mag = static_cast<uint32_t>(ms < 0 ? -static_cast<int64_t>(ms) : ms);
  const uint32_t centis = mag / 10u;
  const char* sign = centis == 0 ? "" : (ms < 0 ? "-" : "+");
  std::snprintf(out, static_cast<size_t>(outSize), "%s%lu.%02lu", sign, static_cast<unsigned long>(centis / 100u),
                static_cast<unsigned long>(centis % 100u));
}

const char* verdict(const int32_t diffMs) {
  const int32_t off = diffMs < 0 ? -diffMs : diffMs;
  if (off < 50) return "SPOT ON";
  if (off < 150) return "VERY CLOSE";
  if (off < 400) return "CLOSE";
  if (off < 1000) return diffMs < 0 ? "A BIT EARLY" : "A BIT LATE";
  return diffMs < 0 ? "WAY TOO EARLY" : "WAY TOO LATE";
}

}  // namespace innerclock
