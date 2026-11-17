#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/presenter/PresenterCore.h"

using namespace presenter;

int main() {
  // Keys.
  assert(usageFor(Key::Next, KeySet::Arrows) == 0x4F);
  assert(usageFor(Key::Previous, KeySet::Arrows) == 0x50);
  assert(usageFor(Key::Next, KeySet::Pages) == 0x4E);
  assert(usageFor(Key::Previous, KeySet::Pages) == 0x4B);
  assert(usageFor(Key::Blank, KeySet::Pages) == 0x05);
  uint8_t report[kReportBytes];
  fillReport(report, 0x4F);
  assert(report[0] == 0 && report[1] == 0 && report[2] == 0x4F && report[3] == 0 && report[7] == 0);
  fillReport(report, 0);
  for (const uint8_t b : report) assert(b == 0);

  // The report map is balanced and declares the id the reports are sent on.
  int open = 0, ids = 0;
  for (size_t i = 0; i + 1 < sizeof(kReportMap); i += 2) {
    if (kReportMap[i] == 0xA1) ++open;
    if (kReportMap[i] == 0x85 && kReportMap[i + 1] == kReportId) ++ids;
  }
  assert(open == 1 && ids == 1 && kReportMap[sizeof(kReportMap) - 1] == 0xC0);

  // The clock, across a millis() wrap.
  Clock clock;
  assert(clock.untouched());
  clock.start(0xFFFFFF00u);
  assert(clock.elapsedMs(0x00000100u) == 0x200);
  clock.pause(0x00000100u);
  assert(!clock.untouched() && clock.elapsedMs(999999) == 0x200);
  clock.start(5000);
  clock.start(9000);  // already running: ignored
  assert(clock.elapsedMs(6000) == 0x200 + 1000);
  clock.reset();
  assert(clock.untouched() && clock.elapsedMs(1) == 0);

  auto at = [](int sec, int limit, bool left, Warn w) {
    return faceFor(static_cast<uint32_t>(sec) * 1000, limit, left, w);
  };

  // No limit: minutes used, never a warning.
  Face f = at(0, 0, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Open && f.minutes == 0);
  f = at(3 * 3600, 0, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Open && f.minutes == 180);

  // Twenty minutes, counting down.
  f = at(0, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Plenty && f.minutes == 20 && f.counted);
  f = at(1, 20, true, Warn::FiveAndTwo);
  assert(f.minutes == 20);
  f = at(60, 20, true, Warn::FiveAndTwo);
  assert(f.minutes == 19);
  f = at(15 * 60 - 1, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Plenty && f.minutes == 6);
  f = at(15 * 60, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Five && f.minutes == 5);
  f = at(18 * 60 - 1, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Five && f.minutes == 3);
  f = at(18 * 60, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Two && f.minutes == 2);
  f = at(20 * 60 - 1, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Two && f.minutes == 1 && std::strcmp(captionFor(f), "MINUTE LEFT: WRAP UP") == 0);
  f = at(20 * 60, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Over && f.minutes == 0);
  f = at(23 * 60 + 30, 20, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Over && f.minutes == 3);

  // Showing time used: the figure turns to time left once a warning is up.
  f = at(10 * 60 + 5, 20, false, Warn::FiveAndTwo);
  assert(f.phase == Phase::Plenty && f.minutes == 10 && !f.counted);
  f = at(16 * 60, 20, false, Warn::FiveAndTwo);
  assert(f.phase == Phase::Five && f.minutes == 4 && f.counted);

  // Fewer warnings.
  f = at(16 * 60, 20, true, Warn::TwoOnly);
  assert(f.phase == Phase::Plenty);
  f = at(19 * 60, 20, true, Warn::TwoOnly);
  assert(f.phase == Phase::Two);
  f = at(19 * 60, 20, true, Warn::None);
  assert(f.phase == Phase::Plenty && f.minutes == 1);
  f = at(21 * 60, 20, true, Warn::None);
  assert(f.phase == Phase::Over);

  // A five-minute talk does not open on its own five-minute warning.
  f = at(0, 5, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Plenty && f.minutes == 5);
  f = at(3 * 60, 5, true, Warn::FiveAndTwo);
  assert(f.phase == Phase::Two);

  std::puts("presenter: ok");
  return 0;
}
