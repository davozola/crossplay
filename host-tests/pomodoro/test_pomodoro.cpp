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

static void pickersAcceptOnlyTheirChoices() {
  Timer t;
  CHECK(t.setFocus(5));
  CHECK(t.focusMinutes == 5);
  CHECK(t.phaseLengthMs() == 5 * kMin);
  CHECK(t.setFocus(60));
  CHECK(!t.setFocus(7));  // not offered: unchanged
  CHECK(t.focusMinutes == 60);
  CHECK(t.setBreak(15));
  CHECK(!t.setBreak(0));
  CHECK(t.breakMinutes == 15);
  for (int i = 0; i < kFocusChoiceCount; ++i) CHECK(t.setFocus(kFocusChoices[i]));
  for (int i = 0; i < kBreakChoiceCount; ++i) CHECK(t.setBreak(kBreakChoices[i]));
}

void leftAndComeBack() {
  // Ten minutes in, away for five: fifteen gone, ten left, still running.
  Timer t;
  t.start(1000);
  const uint32_t gone = t.elapsedMs(1000 + 10 * 60000u);
  CHECK(gone == 10 * 60000u);
  Timer back;
  back.restore(Phase::Focus, 0);
  back.resume(true, gone + 5 * 60000u, 500);
  CHECK(back.state() == State::Running);
  CHECK(back.remainingMinutes(500) == 10);
  CHECK(!back.tick(500));
  // Away past the end: the first tick finishes it and counts the session.
  Timer late;
  late.restore(Phase::Focus, 0);
  late.resume(true, 40 * 60000u, 500);
  CHECK(late.tick(500));
  CHECK(late.phase() == Phase::ShortBreak);
  CHECK(late.totalDone() == 1);
  // Paused stays where it was however long it was left.
  Timer held;
  held.restore(Phase::Focus, 0);
  held.resume(false, 7 * 60000u, 99);
  CHECK(held.state() == State::Paused);
  CHECK(held.remainingMinutes(123456) == 18);
}

int main() {
  leftAndComeBack();
  freshTimerReadsFullLength();
  minutesRoundUp();
  pauseFreezesTheClock();
  fullCycle();
  skipDoesNotCount();
  resetKeepsTotal();
  millisWrap();
  pickersAcceptOnlyTheirChoices();
  if (failures) {
    std::printf("%d pomodoro check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("pomodoro: all checks passed\n");
  return EXIT_SUCCESS;
}
