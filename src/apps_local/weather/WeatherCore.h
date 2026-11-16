#pragma once

// WEATHER's model: Open-Meteo's forecast and geocoding answers read into fixed
// structs, the WMO weather codes folded into the handful of skies the panel
// draws, and the URLs that ask for them. Freestanding (no Arduino), so the
// parsing runs as a host test against captured responses.

#include <cstddef>
#include <cstdint>

namespace weather {

constexpr int kHours = 24;
constexpr int kDays = 7;
constexpr int kMaxPlaces = 6;
constexpr int kMaxResults = 6;
constexpr int kNameChars = 28;
constexpr int kRegionChars = 36;

// What the icon draws. WMO 4677's ~28 codes in nine shapes.
enum class Sky : uint8_t { Clear, PartCloud, Cloud, Fog, Drizzle, Rain, Showers, Snow, Thunder };

Sky skyOf(int wmo);
// A short label for a WMO code, upper case, ASCII.
const char* labelOf(int wmo);

struct Current {
  float temp = 0;
  float feels = 0;
  float wind = 0;  // km/h
  int humidity = 0;
  int code = 0;
  bool isDay = true;
  char time[17] = "";  // "YYYY-MM-DDTHH:MM", the place's local time
};

struct Hour {
  int8_t hour = 0;  // local hour of day
  int8_t precip = 0;
  int16_t code = 0;
  float temp = 0;
};

struct Day {
  int16_t year = 0;
  int8_t month = 0;
  int8_t day = 0;
  int8_t precip = 0;
  int16_t code = 0;
  float max = 0;
  float min = 0;
  char sunrise[6] = "";  // "HH:MM"
  char sunset[6] = "";
};

struct Forecast {
  Current current;
  Hour hours[kHours];
  int hourCount = 0;
  Day days[kDays];
  int dayCount = 0;
};

struct Place {
  char name[kNameChars] = "";      // ASCII, upper case
  char region[kRegionChars] = "";  // "CATALONIA, SPAIN"
  float lat = 0;
  float lon = 0;
};

// False unless the answer carries a current block and at least one day.
bool parseForecast(const char* json, size_t len, Forecast& out);
// Number of places read into `out`, at most `max`. Zero for no match.
int parseGeocode(const char* json, size_t len, Place* out, int max);

// UTF-8 to upper-case ASCII: accents dropped (MALAGA, SAO PAULO, ZURICH),
// anything without a Latin reading skipped. The panel's faces are ASCII.
void foldAscii(const char* utf8, size_t len, char* out, size_t cap);

void forecastUrl(const Place& place, char* out, size_t cap);
void geocodeUrl(const char* query, char* out, size_t cap);

// 0 = Monday.
int weekday(int year, int month, int day);
const char* weekdayName(int wd);   // "MON"
const char* monthName(int month);  // "JAN", 1-based

// Rounded for display, in Celsius or Fahrenheit.
int displayTemp(float celsius, bool fahrenheit);
int displayWind(float kmh, bool imperial);

// Same place to within ~1 km: the geocoder and a saved entry agree.
bool samePlace(const Place& a, const Place& b);

}  // namespace weather
