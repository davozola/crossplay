#include <cmath>
#include <cstdio>
#include <cstring>

#include "CasaCore.h"

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

using namespace casa;

static const uint32_t T0 = 1790000000;

int main() {
  {  // append: gaps, an unset clock, a clock put back
    static Log log = {};
    CHECK(!append(log, {1000, 2100, 5000}));
    CHECK(append(log, {T0, 2100, 5000}));
    CHECK(!append(log, {T0 + 60, 2110, 5000}));
    CHECK(append(log, {T0 + 600, 2120, 5100}));
    CHECK(log.count == 2);
    CHECK(append(log, {T0 - 5000, 2000, 4000}));
    CHECK(log.count == 1 && log.samples[0].tempCenti == 2000);
  }
  {  // a full log drops its oldest
    static Log log = {};
    for (int i = 0; i < kLogCap + 10; ++i) {
      CHECK(append(log, {T0 + static_cast<uint32_t>(i) * 600, static_cast<int16_t>(i), 5000}));
    }
    CHECK(log.count == kLogCap);
    CHECK(log.samples[0].tempCenti == 10);
    CHECK(log.samples[kLogCap - 1].tempCenti == kLogCap + 9);
  }
  {  // bytes round trip, and what is refused
    static Log log = {};
    static Log back = {};
    append(log, {T0, 2150, 4810});
    append(log, {T0 + 600, -320, 9999});
    uint8_t bytes[kBytesCap];
    const size_t n = toBytes(log, bytes, sizeof(bytes));
    CHECK(n == 4 + 16);
    CHECK(fromBytes(bytes, n, back));
    CHECK(back.count == 2 && back.samples[1].tempCenti == -320 && back.samples[1].humCenti == 9999);
    CHECK(!fromBytes(bytes, n - 1, back) && back.count == 0);
    bytes[1] = 9;
    CHECK(!fromBytes(bytes, n, back));
    bytes[1] = 1;
    bytes[2] = 0xFF;
    bytes[3] = 0xFF;
    CHECK(!fromBytes(bytes, n, back));
    CHECK(toBytes(log, bytes, 10) == 0);
  }
  {  // range and change
    static Log log = {};
    append(log, {T0, 2000, 5000});
    append(log, {T0 + 1800, 2100, 5200});
    append(log, {T0 + 3600, 2250, 4700});
    append(log, {T0 + 5400, 2200, 4800});
    Range r = rangeSince(log, T0 + 1000);
    CHECK(r.count == 3 && r.minTemp == 2100 && r.maxTemp == 2250 && r.minHum == 4700 && r.maxHum == 5200);
    CHECK(rangeSince(log, T0 + 99999).count == 0);
    int dt = 0;
    int dh = 0;
    const Sample now = {T0 + 5400, 2200, 4800};
    CHECK(changeOver(log, now, 3600, dt, dh));
    CHECK(dt == 100 && dh == -400);
    CHECK(!changeOver(log, now, 86400, dt, dh));
  }
  {  // dew point against the usual table: 20 C at 50 % is about 9.3 C
    CHECK(std::fabs(dewPoint(20.0f, 50.0f) - 9.3f) < 0.2f);
    CHECK(std::fabs(dewPoint(25.0f, 100.0f) - 25.0f) < 0.1f);
    CHECK(airOf(20) == Air::Dry && airOf(50) == Air::Good && airOf(80) == Air::Humid);
  }
  {  // one decimal
    char b[12];
    formatTenths(2144, b, sizeof(b));
    CHECK(std::strcmp(b, "21.4") == 0);
    formatTenths(2146, b, sizeof(b));
    CHECK(std::strcmp(b, "21.5") == 0);
    formatTenths(-304, b, sizeof(b));
    CHECK(std::strcmp(b, "-3.0") == 0);
    formatTenths(-4, b, sizeof(b));
    CHECK(std::strcmp(b, "0.0") == 0);
  }
  {  // scales hold the data and keep their span
    Scale s = scaleFor(2130, 2190, 4);
    CHECK(s.hi - s.lo == 4 && s.lo * 100 <= 2130 && s.hi * 100 >= 2190);
    s = scaleFor(1810, 2590, 4);
    CHECK(s.lo == 18 && s.hi == 26);
    s = scaleFor(-250, -120, 4);
    CHECK(s.lo <= -3 && s.hi >= -1 && s.hi - s.lo == 4);
  }
  if (failures == 0) std::printf("casa: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
