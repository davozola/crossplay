#include <cstdio>
#include <cstdlib>

#include "ClockCore.h"

using namespace chessclock;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

constexpr uint32_t kS = 1000;

static void firstPressStartsTheOtherSide() {
  Clock c;
  c.reset(2);  // 5+0
  CHECK(c.state() == State::Ready);
  CHECK(c.remainingMs(0, 0) == 300 * kS);
  CHECK(c.press(1, 1000));  // black presses, white (bottom) starts
  CHECK(c.active() == 0);
  CHECK(c.moves(1) == 0);
  CHECK(c.remainingMs(0, 11000) == 290 * kS);
  CHECK(c.remainingMs(1, 11000) == 300 * kS);
  CHECK(!c.press(1, 11000));  // not black's turn
}

static void incrementAfterYourMove() {
  Clock c;
  c.reset(1);  // 3+2
  c.press(1, 0);
  CHECK(c.press(0, 10 * kS));  // white used 10s, gets +2
  CHECK(c.remainingMs(0, 10 * kS) == 172 * kS);
  CHECK(c.moves(0) == 1);
  CHECK(c.active() == 1);
  CHECK(c.press(1, 15 * kS));
  CHECK(c.remainingMs(1, 15 * kS) == 177 * kS);
}

static void pauseFreezesBoth() {
  Clock c;
  c.reset(2);
  c.press(1, 0);
  CHECK(c.togglePause(60 * kS));
  CHECK(c.state() == State::Paused);
  CHECK(c.remainingMs(0, 999 * kS) == 240 * kS);
  CHECK(!c.press(0, 999 * kS));
  CHECK(c.togglePause(1000 * kS));
  CHECK(c.remainingMs(0, 1010 * kS) == 230 * kS);
}

static void flagFallsOnce() {
  Clock c;
  c.reset(0);  // 1+0
  c.press(1, 0);
  CHECK(!c.tick(59 * kS));
  CHECK(c.tick(60 * kS));
  CHECK(!c.tick(61 * kS));
  CHECK(c.state() == State::Flagged);
  CHECK(c.flagged() == 0);
  CHECK(c.remainingMs(0, 70 * kS) == 0);
  CHECK(!c.press(0, 70 * kS));
}

static void millisWrap() {
  Clock c;
  c.reset(2);
  const uint32_t start = 0xFFFFFFFFu - 5 * kS;
  c.press(1, start);
  CHECK(c.remainingMs(0, start + 20 * kS) == 280 * kS);
}

static void restoreComesBackPaused() {
  Clock c;
  const uint32_t rem[2] = {100 * kS, 50 * kS};
  const uint16_t mv[2] = {12, 11};
  c.restore(4, 1, rem, mv, State::Running);
  CHECK(c.state() == State::Paused);
  CHECK(c.active() == 1);
  CHECK(c.moves(0) == 12);
  CHECK(c.togglePause(0));
  CHECK(c.remainingMs(1, 10 * kS) == 40 * kS);
  Clock d;
  d.restore(4, -1, rem, mv, State::Running);
  CHECK(d.state() == State::Ready);
}

static void shownRules() {
  // Plenty of time: whole minutes, rounded up.
  Shown s = shown(299 * kS + 1, false);
  CHECK(!s.withSeconds && s.minutes == 5);
  s = shown(240 * kS, false);
  CHECK(!s.withSeconds && s.minutes == 4);
  s = shown(61 * kS, false);
  CHECK(!s.withSeconds && s.minutes == 2);
  // Last minute: seconds, rounded up, never 0:00 while running.
  s = shown(60 * kS, false);
  CHECK(s.withSeconds && s.minutes == 1 && s.seconds == 0);
  s = shown(1, false);
  CHECK(s.withSeconds && s.minutes == 0 && s.seconds == 1);
  s = shown(0, false);
  CHECK(s.withSeconds && s.seconds == 0);
  // Always seconds.
  s = shown(125 * kS + 500, true);
  CHECK(s.withSeconds && s.minutes == 2 && s.seconds == 6);
}

int main() {
  firstPressStartsTheOtherSide();
  incrementAfterYourMove();
  pauseFreezesBoth();
  flagFallsOnce();
  millisWrap();
  restoreComesBackPaused();
  shownRules();
  if (failures) {
    std::printf("%d chess clock check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("chessclock: all checks passed\n");
  return EXIT_SUCCESS;
}
