#include <cstdio>
#include <cstdlib>

#include "PomodoroCore.h"

using namespace pomodoro;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

constexpr uint32_t kMin = 60u * 1000u;

static void freshTimerReadsFullLength() {
  Timer t;
  CHECK(t.phase() == Phase::Focus);
  CHECK(t.state() == State::Ready);
  CHECK(t.remainingMinutes(0) == 25);
  CHECK(t.progressPermille(0) == 0);
}

static void minutesRoundUp() {
  Timer t;
  t.start(1000);
  CHECK(t.remainingMinutes(1000 + 1) == 25);
  CHECK(t.remainingMinutes(1000 + kMin) == 24);
  CHECK(t.remainingMinutes(1000 + 25 * kMin - 1) == 1);
  CHECK(t.progressPermille(1000 + 12 * kMin + 30000) == 500);
}

static void pauseFreezesTheClock() {
  Timer t;
  t.start(0);
  t.pause(10 * kMin);
  CHECK(t.state() == State::Paused);
  CHECK(t.remainingMinutes(999 * kMin) == 15);
  t.toggle(100 * kMin);  // resume an hour and a half later
  CHECK(t.state() == State::Running);
  CHECK(t.remainingMinutes(100 * kMin) == 15);
  CHECK(!t.tick(114 * kMin));
  CHECK(t.tick(115 * kMin));
}

static void fullCycle() {
  Timer t;
  uint32_t now = 0;
  for (int i = 0; i < kSessionsPerCycle; ++i) {
    CHECK(t.phase() == Phase::Focus);
    t.start(now);
    now += 25 * kMin;
    CHECK(t.tick(now));
    CHECK(!t.tick(now));  // fires once
    CHECK(t.state() == State::Ready);
    CHECK(t.doneInCycle() == i + 1);
    if (i < kSessionsPerCycle - 1) {
      CHECK(t.phase() == Phase::ShortBreak);
      CHECK(t.remainingMinutes(now) == 5);
    } else {
      CHECK(t.phase() == Phase::LongBreak);
      CHECK(t.remainingMinutes(now) == 15);
    }
    t.start(now);
    now += t.phaseLengthMs();
    CHECK(t.tick(now));
  }
  CHECK(t.phase() == Phase::Focus);
  CHECK(t.doneInCycle() == 0);
  CHECK(t.totalDone() == 4);
}

static void skipDoesNotCount() {
  Timer t;
  t.start(0);
  t.skip();
  CHECK(t.phase() == Phase::ShortBreak);
  CHECK(t.totalDone() == 0);
  CHECK(t.doneInCycle() == 1);
  t.skip();
  CHECK(t.phase() == Phase::Focus);
}

static void resetKeepsTotal() {
  Timer t;
  t.start(0);
  CHECK(t.tick(25 * kMin));
  t.reset();
  CHECK(t.phase() == Phase::Focus);
  CHECK(t.doneInCycle() == 0);
  CHECK(t.totalDone() == 1);
}

static void millisWrap() {
  Timer t;
  const uint32_t nearWrap = 0xFFFFFFFFu - 2 * kMin;
  t.start(nearWrap);
  CHECK(t.remainingMinutes(nearWrap + 10 * kMin) == 15);  // wrapped past zero
  CHECK(t.tick(nearWrap + 25 * kMin));
}

static void settingsCycleAndWrap() {
  Timer t;
  t.cycleFocus();
  CHECK(t.focusMinutes == 30);
  for (int i = 0; i < kFocusChoiceCount; ++i) t.cycleFocus();
  CHECK(t.focusMinutes == 30);
  t.focusMinutes = 7;  // not a choice, e.g. an old save
  t.cycleFocus();
  CHECK(t.focusMinutes == kFocusChoices[0]);
  t.cycleBreak();
  CHECK(t.breakMinutes == 10);
}

int main() {
  freshTimerReadsFullLength();
  minutesRoundUp();
  pauseFreezesTheClock();
  fullCycle();
  skipDoesNotCount();
  resetKeepsTotal();
  millisWrap();
  settingsCycleAndWrap();
  if (failures) {
    std::printf("%d pomodoro check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("pomodoro: all checks passed\n");
  return EXIT_SUCCESS;
}
