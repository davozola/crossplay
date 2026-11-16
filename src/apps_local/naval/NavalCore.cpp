#include "NavalCore.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../json/FlatJson.h"

namespace naval {

using namespace flatjson;

namespace {

int digits(const char* p, const int n) {
  int v = 0;
  for (int i = 0; i < n && p[i] >= '0' && p[i] <= '9'; ++i) v = v * 10 + (p[i] - '0');
  return v;
}

int16_t bearing(const Span& el) {
  float f = 0;
  if (!number(el, f)) return 0;
  return static_cast<int16_t>(((static_cast<int>(std::lround(f)) % 360) + 360) % 360);
}

}  // namespace

void marineUrl(const float lat, const float lon, char* out, const size_t cap) {
  std::snprintf(out, cap,
                "https://marine-api.open-meteo.com/v1/marine?latitude=%.4f&longitude=%.4f&current=wave_height"
                "&hourly=wave_height,wave_direction,wave_period,wind_wave_height,swell_wave_height"
                ",swell_wave_direction,swell_wave_period,sea_surface_temperature&timezone=auto&forecast_days=%d",
                static_cast<double>(lat), static_cast<double>(lon), kDays);
}

void windUrl(const float lat, const float lon, char* out, const size_t cap) {
  std::snprintf(out, cap,
                "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=wind_speed_10m"
                "&hourly=wind_speed_10m,wind_direction_10m,wind_gusts_10m&wind_speed_unit=kn"
                "&timezone=auto&forecast_days=%d",
                static_cast<double>(lat), static_cast<double>(lon), kDays);
}

bool parseMarine(const char* json, const size_t len, Sea& out) {
  out = Sea{};
  const Span doc = root(json, len);
  if (!doc.ok()) return false;
  const Span hourly = member(doc, "hourly");
  if (!hourly.ok()) return false;
  Hour* hours = out.hours;
  Sea* sea = &out;
  const int count = forEach(member(hourly, "time"), kHours, [hours, sea](const int i, const Span& el) {
    char iso[20];
    string(el, iso, sizeof(iso));
    if (i == 0) sea->year = static_cast<int16_t>(digits(iso, 4));
    hours[i].month = static_cast<int8_t>(digits(iso + 5, 2));
    hours[i].day = static_cast<int8_t>(digits(iso + 8, 2));
    const char* t = std::strchr(iso, 'T');
    hours[i].hour = static_cast<int8_t>(t ? digits(t + 1, 2) : 0);
    // A new date is a new day; the answer runs in order.
    if (i == 0) {
      hours[i].dayIndex = 0;
    } else {
      const bool same = hours[i].day == hours[i - 1].day && hours[i].month == hours[i - 1].month;
      hours[i].dayIndex = static_cast<int8_t>(hours[i - 1].dayIndex + (same ? 0 : 1));
    }
  });
  if (count == 0) return false;
  out.count = count;
  out.dayCount = hours[count - 1].dayIndex + 1;
  if (out.dayCount > kDays) out.dayCount = kDays;
  forEach(member(hourly, "wave_height"), count, [hours](const int i, const Span& el) { number(el, hours[i].wave); });
  forEach(member(hourly, "wave_period"), count,
          [hours](const int i, const Span& el) { number(el, hours[i].wavePeriod); });
  forEach(member(hourly, "wave_direction"), count,
          [hours](const int i, const Span& el) { hours[i].waveFrom = bearing(el); });
  forEach(member(hourly, "wind_wave_height"), count,
          [hours](const int i, const Span& el) { number(el, hours[i].windWave); });
  forEach(member(hourly, "swell_wave_height"), count,
          [hours](const int i, const Span& el) { number(el, hours[i].swell); });
  forEach(member(hourly, "swell_wave_period"), count,
          [hours](const int i, const Span& el) { number(el, hours[i].swellPeriod); });
  forEach(member(hourly, "swell_wave_direction"), count,
          [hours](const int i, const Span& el) { hours[i].swellFrom = bearing(el); });
  forEach(member(hourly, "sea_surface_temperature"), count,
          [hours](const int i, const Span& el) { number(el, hours[i].water); });
  string(member(member(doc, "current"), "time"), out.now, sizeof(out.now));
  bool any = false;
  for (int i = 0; i < count; ++i) any = any || hours[i].wave >= 0;
  return any;
}

bool parseWind(const char* json, const size_t len, Sea& sea) {
  const Span doc = root(json, len);
  if (!doc.ok()) return false;
  const Span hourly = member(doc, "hourly");
  if (!hourly.ok()) return false;
  // Where each of this answer's hours sits among the sea's. The two answers
  // cover the same days, so it is nearly always the same index.
  int16_t slot[kHours];
  Hour* hours = sea.hours;
  const int have = sea.count;
  int16_t* slots = slot;
  const int count = forEach(member(hourly, "time"), kHours, [hours, have, slots](const int i, const Span& el) {
    char iso[20];
    string(el, iso, sizeof(iso));
    const int month = digits(iso + 5, 2);
    const int day = digits(iso + 8, 2);
    const char* t = std::strchr(iso, 'T');
    const int hour = t ? digits(t + 1, 2) : 0;
    slots[i] = -1;
    for (int k = 0; k < have; ++k) {
      const int at = (i + k) % have;  // start looking at the same index
      if (hours[at].month == month && hours[at].day == day && hours[at].hour == hour) {
        slots[i] = static_cast<int16_t>(at);
        break;
      }
    }
  });
  if (count == 0) return false;
  forEach(member(hourly, "wind_speed_10m"), count, [hours, slots](const int i, const Span& el) {
    if (slots[i] >= 0) number(el, hours[slots[i]].wind);
  });
  forEach(member(hourly, "wind_gusts_10m"), count, [hours, slots](const int i, const Span& el) {
    if (slots[i] >= 0) number(el, hours[slots[i]].gust);
  });
  forEach(member(hourly, "wind_direction_10m"), count, [hours, slots](const int i, const Span& el) {
    if (slots[i] >= 0) hours[slots[i]].windFrom = bearing(el);
  });
  if (!sea.now[0]) string(member(member(doc, "current"), "time"), sea.now, sizeof(sea.now));
  sea.hasWind = false;
  for (int i = 0; i < have; ++i) sea.hasWind = sea.hasWind || hours[i].wind >= 0;
  return sea.hasWind;
}

int nowIndex(const Sea& sea) {
  if (sea.count == 0 || !sea.now[0]) return 0;
  const int month = digits(sea.now + 5, 2);
  const int day = digits(sea.now + 8, 2);
  const int hour = digits(sea.now + 11, 2);
  for (int i = 0; i < sea.count; ++i) {
    const Hour& h = sea.hours[i];
    if (h.month == month && h.day == day && h.hour == hour) return i;
  }
  return 0;
}

int hoursOfDay(const Sea& sea, const int d, int& first) {
  first = -1;
  int n = 0;
  for (int i = 0; i < sea.count; ++i) {
    if (sea.hours[i].dayIndex != d) continue;
    if (first < 0) first = i;
    ++n;
  }
  if (first < 0) first = 0;
  return n;
}

DayStats statsOf(const Sea& sea, const int d) {
  DayStats s;
  int first = 0;
  const int n = hoursOfDay(sea, d, first);
  for (int i = first; i < first + n; ++i) {
    const Hour& h = sea.hours[i];
    s.month = h.month;
    s.day = h.day;
    if (h.wind > s.windMax) {
      s.windMax = h.wind;
      s.windFrom = h.windFrom;
    }
    if (h.gust > s.gustMax) s.gustMax = h.gust;
    if (h.wave >= 0) {
      if (h.wave > s.waveMax) s.waveMax = h.wave;
      if (s.waveMin < 0 || h.wave < s.waveMin) s.waveMin = h.wave;
    }
  }
  return s;
}

int beaufort(const float knots) {
  // The top of each force, in knots.
  static const float kTop[12] = {1, 3, 6, 10, 16, 21, 27, 33, 40, 47, 55, 63};
  const float k = std::round(knots);
  for (int f = 0; f < 12; ++f) {
    if (k < kTop[f] || (f > 0 && k == kTop[f])) return f;
  }
  return 12;
}

const char* beaufortName(const int force) {
  static const char* const kNames[13] = {
      "CALM",      "LIGHT AIR", "LIGHT BREEZE", "GENTLE BREEZE", "MODERATE BREEZE", "FRESH BREEZE", "STRONG BREEZE",
      "NEAR GALE", "GALE",      "STRONG GALE",  "STORM",         "VIOLENT STORM",   "HURRICANE"};
  return kNames[force < 0 ? 0 : (force > 12 ? 12 : force)];
}

int seaState(const float metres) {
  static const float kTop[9] = {0.005f, 0.1f, 0.5f, 1.25f, 2.5f, 4, 6, 9, 14};
  for (int s = 0; s < 9; ++s) {
    if (metres <= kTop[s]) return s;
  }
  return 9;
}

const char* seaStateName(const int state) {
  static const char* const kNames[10] = {"GLASSY", "RIPPLED",    "SMOOTH", "SLIGHT",    "MODERATE",
                                         "ROUGH",  "VERY ROUGH", "HIGH",   "VERY HIGH", "PHENOMENAL"};
  return kNames[state < 0 ? 0 : (state > 9 ? 9 : state)];
}

const char* compass(const int degrees) {
  static const char* const kPoints[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                          "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  const int d = ((degrees % 360) + 360) % 360;
  return kPoints[static_cast<int>((d + 11.25f) / 22.5f) % 16];
}

int weekday(const int year, const int month, const int day) {
  static const int kT[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 1 || month > 12) return 0;
  const int y = month < 3 ? year - 1 : year;
  return (y + y / 4 - y / 100 + y / 400 + kT[month - 1] + day) % 7;
}

}  // namespace naval
