// NAVAL WEATHER host tests: the two Open-Meteo answers read and merged from
// the fixtures beside this file, and the scales checked at their edges.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "NavalCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    if (failures < 20) std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
bool near(double a, double b, double eps = 0.011) {
  if (std::fabs(a - b) >= eps) std::printf("  got %.4f, want %.4f\n", a, b);
  return std::fabs(a - b) < eps;
}
std::string slurp(const char* path) {
  std::ifstream in(path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
}  // namespace

int main() {
  using namespace naval;
  const std::string marine = slurp("marine.json");
  const std::string wind = slurp("wind.json");
  check(!marine.empty() && !wind.empty(), "fixtures present");

  auto sea = std::make_unique<Sea>();
  check(parseMarine(marine.data(), marine.size(), *sea), "marine parses");
  check(sea->count == kHours && sea->dayCount == 7 && sea->year == 2026, "seven days of hours");
  check(!sea->hasWind && sea->hours[12].wind < 0, "no wind yet");
  check(parseWind(wind.data(), wind.size(), *sea) && sea->hasWind, "wind merges");
  const Hour& noon = sea->hours[12];
  check(noon.month == 11 && noon.day == 16 && noon.hour == 12 && noon.dayIndex == 0, "hours are dated");
  check(near(noon.wave, 1.13) && near(noon.wind, 14.9) && noon.windFrom == 66, "noon's sea and wind");
  check(noon.waveFrom == 92 && near(noon.swell, 0.9) && near(noon.windWave, 0.45) && near(noon.wavePeriod, 4.6),
        "the rest of the sea");
  check(near(noon.water, 18.5, 0.06), "water");
  check(sea->hours[kHours - 1].water < -50, "a null stays missing");
  check(sea->hours[30].dayIndex == 1 && sea->hours[167].dayIndex == 6, "day indexes");
  check(nowIndex(*sea) == 12, "now is the hour the clock is in");
  int first = 0;
  check(hoursOfDay(*sea, 2, first) == 24 && first == 48, "hours of a day");
  const DayStats s = statsOf(*sea, 2);
  check(near(s.windMax, 33.3) && s.day == 18 && s.gustMax > s.windMax && s.waveMax >= s.waveMin && s.waveMin > 0,
        "a day's extremes");
  check(s.windFrom == sea->hours[60].windFrom || s.windFrom == sea->hours[61].windFrom ||
            s.windFrom == sea->hours[62].windFrom || s.windFrom == sea->hours[63].windFrom ||
            s.windFrom == sea->hours[64].windFrom || s.windFrom == sea->hours[65].windFrom,
        "the bearing is the windiest hour's");

  // Wind that arrives shifted by a day still lands on the right hours.
  {
    std::string shifted = wind;
    const size_t at = shifted.find("\"time\":[");
    const size_t firstComma = shifted.find(',', at);
    // Drop the first hour from the time list and the first value from one series.
    shifted.erase(at + 8, firstComma - (at + 8) + 1);
    auto other = std::make_unique<Sea>();
    parseMarine(marine.data(), marine.size(), *other);
    check(parseWind(shifted.data(), shifted.size(), *other), "a shifted answer still merges");
    check(near(other->hours[13].wind, sea->hours[12].wind), "onto the hours its times name");
  }
  {
    auto bad = std::make_unique<Sea>();
    const char* inland = "{\"hourly\":{\"time\":[\"2026-11-16T00:00\"],\"wave_height\":[null]}}";
    check(!parseMarine(inland, std::strlen(inland), *bad), "an inland point has no sea");
    check(!parseMarine("[]", 2, *bad) && !parseMarine("{}", 2, *bad), "nor does nonsense");
    check(!parseWind("{}", 2, *sea), "wind needs hours too");
  }

  check(beaufort(0) == 0 && beaufort(1) == 1 && beaufort(3) == 1 && beaufort(4) == 2 && beaufort(6) == 2,
        "light forces");
  check(beaufort(10) == 3 && beaufort(11) == 4 && beaufort(16) == 4 && beaufort(17) == 5 && beaufort(21) == 5,
        "breezes");
  check(beaufort(27) == 6 && beaufort(28) == 7 && beaufort(34) == 8 && beaufort(40) == 8 && beaufort(41) == 9, "gales");
  check(beaufort(63) == 11 && beaufort(64) == 12 && beaufort(120) == 12, "the top");
  check(std::strcmp(beaufortName(8), "GALE") == 0 && std::strcmp(beaufortName(0), "CALM") == 0, "force names");
  check(seaState(0) == 0 && seaState(0.05f) == 1 && seaState(0.3f) == 2 && seaState(1.0f) == 3 && seaState(2.0f) == 4,
        "calm to moderate");
  check(seaState(3) == 5 && seaState(5) == 6 && seaState(8) == 7 && seaState(12) == 8 && seaState(20) == 9,
        "rough to phenomenal");
  check(std::strcmp(seaStateName(3), "SLIGHT") == 0, "sea names");
  check(std::strcmp(compass(0), "N") == 0 && std::strcmp(compass(359), "N") == 0 &&
            std::strcmp(compass(45), "NE") == 0 && std::strcmp(compass(200), "SSW") == 0 &&
            std::strcmp(compass(-90), "W") == 0 && std::strcmp(compass(348), "NNW") == 0,
        "compass points");
  check(weekday(2026, 11, 16) == 1 && weekday(2024, 2, 29) == 4, "weekdays");
  char url[400];
  marineUrl(41.38f, 2.17f, url, sizeof(url));
  check(std::strstr(url, "marine-api.open-meteo.com") && std::strstr(url, "latitude=41.38") &&
            std::strstr(url, "swell_wave_height"),
        "marine url");
  windUrl(41.38f, 2.17f, url, sizeof(url));
  check(std::strstr(url, "wind_speed_unit=kn") && std::strstr(url, "wind_gusts_10m"), "wind url in knots");

  // meteoblue's wind over the same hours: metres a second in, knots on the page.
  {
    const Hour first = sea->hours[0];
    char mb[512];
    std::snprintf(mb, sizeof(mb),
                  "{\"units\":{\"time\":\"YYYY-MM-DD hh:mm\",\"windspeed\":\"ms-1\"},\"data_1h\":{\"time\":"
                  "[\"%04d-%02d-%02d %02d:00\",\"1999-01-01 00:00\"],\"windspeed\":[10.0,3.0],"
                  "\"winddirection\":[225,90]}}",
                  sea->year, first.month, first.day, first.hour);
    sea->hours[0].gust = 5;
    check(parseMeteoblueWind(mb, std::strlen(mb), *sea), "meteoblue wind is read");
    check(sea->windIsMeteoblue, "and the sea says whose wind it is");
    check(sea->hours[0].wind > 19.4f && sea->hours[0].wind < 19.5f, "10 m/s is 19.4 knots");
    check(sea->hours[0].windFrom == 225, "with its direction");
    check(sea->hours[0].gust < 0, "a gust the new wind overtakes is dropped");
    const char* none = "{\"data_1h\":{\"time\":[\"1999-01-01 00:00\"],\"windspeed\":[3.0]}}";
    auto other = std::make_unique<Sea>(*sea);
    other->windIsMeteoblue = false;
    check(!parseMeteoblueWind(none, std::strlen(none), *other), "no matching hour is no wind");
    char key[40];
    check(tidyKey("  AbCd1234efGH5678\r\n", key, sizeof(key)) && std::strcmp(key, "AbCd1234efGH5678") == 0,
          "a key is tidied of spaces and line ends");
    check(!tidyKey(" \n", key, sizeof(key)), "nothing is not a key");
    char url[256];
    meteoblueUrl(41.39f, 2.16f, "KEY", url, sizeof(url));
    check(std::strstr(url, "basic-1h") && std::strstr(url, "apikey=KEY"), "the URL asks for basic-1h with the key");
  }

  if (failures == 0) std::printf("naval: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
