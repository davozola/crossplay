#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "InnerCore.h"

using namespace innerclock;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

static bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

static void roundAndRecord() {
  Record rec;
  Round r;
  CHECK(r.target() == 5);
  CHECK(!r.stop(100, rec));  // nothing running
  r.start(1000);
  CHECK(r.state() == State::Running);
  r.setTarget(1);  // ignored while running
  CHECK(r.target() == 5);
  CHECK(r.stop(6230, rec));
  CHECK(r.elapsedMs() == 5230);
  CHECK(r.diffMs() == 230);
  CHECK(r.newBest());
  CHECK(rec.bestMs[0] == 230 && rec.bestMs[1] == -1);
  r.start(0);
  CHECK(r.stop(4700, rec));
  CHECK(r.diffMs() == -300);
  CHECK(!r.newBest());
  CHECK(rec.bestMs[0] == 230);
  CHECK(rec.recentCount == 2 && rec.recent[0].diffMs == -300 && rec.recent[1].diffMs == 230);
  r.clear();
  r.setTarget(1);
  CHECK(r.target() == 10);
  r.start(0xFFFFFFFFu - 2000);  // across the millis() wrap
  CHECK(r.stop(7990, rec));
  CHECK(r.elapsedMs() == 9991);
  CHECK(rec.bestMs[1] == 9);
  for (int i = 0; i < 10; ++i) {
    r.start(0);
    r.stop(10000, rec);
  }
  CHECK(rec.recentCount == kRecent);
  CHECK(rec.tries == 13);
}

static void formatting() {
  char b[16];
  formatSeconds(5234, b, sizeof(b));
  CHECK(eq(b, "5.23"));
  formatSeconds(4999, b, sizeof(b));
  CHECK(eq(b, "4.99"));  // truncated, not rounded to 5.00
  formatSeconds(10000, b, sizeof(b));
  CHECK(eq(b, "10.00"));
  formatDiff(234, b, sizeof(b));
  CHECK(eq(b, "+0.23"));
  formatDiff(-1405, b, sizeof(b));
  CHECK(eq(b, "-1.40"));
  formatDiff(-9, b, sizeof(b));
  CHECK(eq(b, "0.00"));
  CHECK(eq(verdict(30), "SPOT ON"));
  CHECK(eq(verdict(-600), "A BIT EARLY"));
  CHECK(eq(verdict(2500), "WAY TOO LATE"));
}

int main() {
  roundAndRecord();
  formatting();
  if (failures) {
    std::printf("%d inner clock check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("innerclock: all checks passed\n");
  return EXIT_SUCCESS;
}
