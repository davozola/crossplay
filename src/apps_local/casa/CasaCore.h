#pragma once

// CASA's model: the room's temperature and humidity now, and a day and a half
// of readings to draw. Freestanding: the sensor and the card are CasaStore's.

#include <cstddef>
#include <cstdint>

namespace casa {

struct Sample {
  uint32_t epoch;     // seconds, wall clock
  int16_t tempCenti;  // hundredths of a degree Celsius
  uint16_t humCenti;  // hundredths of a percent
};
static_assert(sizeof(Sample) == 8, "Sample is written to the card as it lies in memory");

constexpr int kLogCap = 288;       // two days at one every ten minutes
constexpr uint32_t kMinGap = 540;  // readings closer than this are one reading
constexpr uint32_t kDay = 86400;
// Before this the clock was never set, and a reading has no place on a day.
constexpr uint32_t kClockSetAfter = 1704067200;  // 2024-01-01

struct Log {
  Sample samples[kLogCap];  // oldest first
  int count;
};

// Adds a reading. Refused when the clock is unset or the last one is too
// recent. A reading older than the last means the clock was put back: the log
// starts again, since its times no longer order.
bool append(Log& log, const Sample& sample);

// The log as bytes for the card and back. fromBytes returns false, leaving an
// empty log, for anything it did not write.
constexpr size_t kBytesCap = 4 + sizeof(Sample) * kLogCap;
size_t toBytes(const Log& log, uint8_t* out, size_t cap);
bool fromBytes(const uint8_t* data, size_t size, Log& log);

struct Range {
  int count;
  int16_t minTemp, maxTemp;
  uint16_t minHum, maxHum;
};
// Over the readings at or after `since`.
Range rangeSince(const Log& log, uint32_t since);

// How much it changed against the reading nearest `seconds` ago. False when
// nothing lies within a quarter of that span of the moment asked for.
bool changeOver(const Log& log, const Sample& now, uint32_t seconds, int& tempCenti, int& humCenti);

// Magnus: the temperature at which this air would start to bead on a window.
float dewPoint(float tempC, float humidityPct);

enum class Air : uint8_t { Dry, Good, Humid };
Air airOf(float humidityPct);
const char* airEs(Air air);

// "21.4", "-3.0": one decimal, rounded.
void formatTenths(int centi, char* out, size_t cap);

// A vertical scale with whole numbers at both ends that holds [lo, hi] with
// room to spare and never spans less than `minSpan`.
struct Scale {
  int lo, hi;
};
Scale scaleFor(int loCenti, int hiCenti, int minSpan);

}  // namespace casa
