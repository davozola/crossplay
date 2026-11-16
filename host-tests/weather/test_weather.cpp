// WEATHER host tests: the Open-Meteo readers against captured-shape answers,
// the code table, the ASCII fold and the URLs.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "WeatherCore.h"

namespace {

int failures = 0;

void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

std::string slurp(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

bool near(const float a, const float b) { return std::fabs(a - b) < 0.01f; }

}  // namespace

int main() {
  using namespace weather;

  // --- forecast ---
  {
    const std::string json = slurp("forecast.json");
    check(!json.empty(), "fixture forecast.json readable");
    Forecast f;
    check(parseForecast(json.data(), json.size(), f), "forecast parses");
    check(near(f.current.temp, 12.4f), "current temp from current, not current_units");
    check(near(f.current.feels, 10.1f), "feels like");
    check(f.current.humidity == 71, "humidity");
    check(f.current.code == 2, "current code");
    check(near(f.current.wind, 14.3f), "wind");
    check(f.current.isDay, "is_day");
    check(std::strcmp(f.current.time, "2026-11-16T08:00") == 0, "current time");
    check(f.hourCount == 24, "24 hours");
    check(f.hours[0].hour == 8 && f.hours[16].hour == 0 && f.hours[23].hour == 7, "hour of day wraps midnight");
    check(f.hours[5].precip == 80 && f.hours[5].code == 63, "hour precip and code");
    check(f.hours[23].precip == 0, "null precip reads as 0");
    check(f.dayCount == 7, "7 days");
    check(f.days[0].year == 2026 && f.days[0].month == 11 && f.days[0].day == 16, "day date");
    check(near(f.days[5].max, 8.2f) && near(f.days[5].min, 1.2f), "day max/min");
    check(f.days[4].code == 95 && f.days[4].precip == 90, "day code and precip");
    check(std::strcmp(f.days[0].sunrise, "07:46") == 0 && std::strcmp(f.days[6].sunset, "17:25") == 0, "sun times");

    Forecast bad;
    check(!parseForecast("{\"error\":true,\"reason\":\"x\"}", 30, bad), "error answer rejected");
    check(!parseForecast("", 0, bad), "empty rejected");
    check(!parseForecast(json.data(), json.size() / 2, bad) || bad.dayCount <= 7, "truncated answer safe");
  }

  // --- geocode ---
  {
    const std::string json = slurp("geocode.json");
    Place p[kMaxResults];
    const int n = parseGeocode(json.data(), json.size(), p, kMaxResults);
    check(n == 4, "duplicate place dropped");
    check(std::strcmp(p[0].name, "BARCELONA") == 0, "name upper case");
    check(std::strcmp(p[0].region, "CATALUNA, ESPANA") == 0, "region folded");
    check(std::strcmp(p[1].region, "ANZOATEGUI, VENEZUELA") == 0, "second barcelona");
    check(std::strcmp(p[2].name, "SAO PAULO") == 0, "sao paulo folded");
    check(std::strcmp(p[2].region, "BRASIL") == 0, "admin equal to name is dropped");
    check(std::strcmp(p[3].name, "ZURICH") == 0 && std::strcmp(p[3].region, "SCHWEIZ") == 0, "no admin1");
    check(near(p[3].lat, 47.36667f), "lat");
    check(parseGeocode("{\"generationtime_ms\":0.3}", 24, p, kMaxResults) == 0, "no results");
    check(parseGeocode(json.data(), json.size(), p, 2) == 2, "max honoured");
  }

  // --- fold ---
  {
    char out[32];
    foldAscii("Málaga", 7, out, sizeof(out));
    check(std::strcmp(out, "MALAGA") == 0, "malaga");
    foldAscii("Kraków", 7, out, sizeof(out));
    check(std::strcmp(out, "KRAKOW") == 0, "krakow");
    foldAscii("Łódź", 7, out, sizeof(out));
    check(std::strcmp(out, "LODZ") == 0, "lodz");
    foldAscii("東京 Tokyo", 13, out, sizeof(out));
    check(std::strcmp(out, "TOKYO") == 0, "cjk skipped, no leading space");
    foldAscii("Straße", 7, out, sizeof(out));
    check(std::strcmp(out, "STRASSE") == 0, "eszett");
    char tiny[4];
    foldAscii("Barcelona", 9, tiny, sizeof(tiny));
    check(std::strcmp(tiny, "BAR") == 0, "fold truncates safely");
  }

  // --- codes ---
  check(skyOf(0) == Sky::Clear && skyOf(2) == Sky::PartCloud && skyOf(3) == Sky::Cloud, "sky clear/cloud");
  check(skyOf(45) == Sky::Fog && skyOf(53) == Sky::Drizzle && skyOf(65) == Sky::Rain, "sky fog/drizzle/rain");
  check(skyOf(81) == Sky::Showers && skyOf(75) == Sky::Snow && skyOf(99) == Sky::Thunder, "sky showers/snow/storm");
  check(skyOf(1234) == Sky::Cloud && std::strcmp(labelOf(1234), "CLOUDY") == 0, "unknown code");
  for (int c = 0; c < 100; ++c) {
    for (const char* s = labelOf(c); *s; ++s)
      check(*s >= 0x20 && *s < 0x7F && !(*s >= 'a' && *s <= 'z'), "label ascii");
  }

  // --- dates and units ---
  check(weekday(2026, 11, 16) == 0, "16 nov 2026 is a monday");
  check(weekday(2024, 2, 29) == 3, "leap day 2024 thursday");
  check(weekday(2000, 1, 1) == 5, "1 jan 2000 saturday");
  check(std::strcmp(weekdayName(6), "SUN") == 0 && std::strcmp(monthName(11), "NOV") == 0, "names");
  check(displayTemp(12.4f, false) == 12 && displayTemp(-0.6f, false) == -1, "celsius rounding");
  check(displayTemp(20.0f, true) == 68 && displayTemp(-40.0f, true) == -40, "fahrenheit");
  check(displayWind(16.09f, true) == 10 && displayWind(14.3f, false) == 14, "wind");

  // --- urls ---
  {
    char url[512];
    Place p;
    p.lat = 41.3888f;
    p.lon = -2.159f;
    forecastUrl(p, url, sizeof(url));
    check(std::strstr(url, "latitude=41.3888&longitude=-2.1590") != nullptr, "forecast coords");
    check(std::strstr(url, "forecast_hours=24") != nullptr && std::strstr(url, "timezone=auto") != nullptr,
          "forecast params");
    geocodeUrl("New York", url, sizeof(url));
    check(std::strstr(url, "name=New%20York") != nullptr, "query encoded");
    geocodeUrl("A&B=C", url, sizeof(url));
    check(std::strstr(url, "name=A%26B%3DC") != nullptr, "reserved chars encoded");
  }

  if (failures == 0) std::printf("weather: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
