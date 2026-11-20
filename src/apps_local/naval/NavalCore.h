#pragma once

// NAVAL WEATHER's model: Open-Meteo's marine answer (waves, swell, water) and
// the wind out of its forecast answer, merged hour by hour, with the scales a
// sailor reads them in. Freestanding.

#include <cstddef>
#include <cstdint>

namespace naval {

constexpr int kDays = 7;
constexpr int kHours = kDays * 24;
constexpr float kMissing = -1;

struct Hour {
  int8_t month = 0;
  int8_t day = 0;
  int8_t hour = 0;
  int8_t dayIndex = 0;  // 0 for the first date in the answer
  // Wind, knots, and where it blows from.
  float wind = kMissing;
  float gust = kMissing;
  int16_t windFrom = 0;
  // The sea, metres and seconds, and where it comes from.
  float wave = kMissing;
  float wavePeriod = kMissing;
  int16_t waveFrom = 0;
  float swell = kMissing;
  float swellPeriod = kMissing;
  int16_t swellFrom = 0;
  float windWave = kMissing;
  float water = -100;  // degrees C; below -50 is missing
};

struct Sea {
  Hour hours[kHours];
  int count = 0;
  int dayCount = 0;
  int16_t year = 0;
  char now[20] = "";  // the place's own clock when it was fetched
  bool hasWind = false;
  bool windIsMeteoblue = false;  // the wind was laid over from meteoblue's answer
};

void marineUrl(float lat, float lon, char* out, size_t cap);
void windUrl(float lat, float lon, char* out, size_t cap);

// Reads the marine answer into a fresh `out`. False when it holds no hours or
// no waves at all, which is what an inland point gets.
bool parseMarine(const char* json, size_t len, Sea& out);
// Adds the wind to hours already there, matched by date and hour.
bool parseWind(const char* json, size_t len, Sea& sea);

// meteoblue's basic-1h for a point. The key is the owner's and goes nowhere
// but this URL.
void meteoblueUrl(float lat, float lon, const char* key, char* out, size_t cap);
// Lays meteoblue's wind speed and direction over hours already there, matched
// by date and hour, in knots whatever unit the answer says it is in. A gust
// left from the other model that the new wind overtakes is dropped. False
// when the answer holds no wind for any hour here.
bool parseMeteoblueWind(const char* json, size_t len, Sea& sea);
// A key as typed or read from a file: spaces and line ends off, and only what
// a key is made of. False when nothing usable is left.
bool tidyKey(const char* in, char* out, size_t cap);

// The hour `now` falls in, or 0.
int nowIndex(const Sea& sea);
// The first hour of day `d` and how many it has.
int hoursOfDay(const Sea& sea, int d, int& first);

struct DayStats {
  float windMax = kMissing;
  float gustMax = kMissing;
  int16_t windFrom = 0;  // at the windiest hour
  float waveMax = kMissing;
  float waveMin = kMissing;
  int8_t month = 0;
  int8_t day = 0;
};
// Over the hours `fromHour`..`toHour` of the day, both included: the whole
// day unasked, or the hours somebody would actually be out in.
DayStats statsOf(const Sea& sea, int d, int fromHour = 0, int toHour = 23);

int beaufort(float knots);
const char* beaufortName(int force);
// The Douglas scale's word for a sea of this height.
int seaState(float metres);
const char* seaStateName(int state);
// "NNE" for a bearing.
const char* compass(int degrees);
// Sakamoto: 0 is Sunday.
int weekday(int year, int month, int day);

}  // namespace naval
