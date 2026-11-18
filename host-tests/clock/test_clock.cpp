// RELOJ host tests: the time and the date in words, the week of the year, and
// that every pen-stroke numeral stays in its box.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ClockCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what, const int n = 0) {
  if (!ok) {
    if (failures < 20) std::printf("FAIL: %s (%d)\n", what, n);
    ++failures;
  }
}
bool says(const int h, const int m, const char* lead, const char* hour, const char* tail) {
  const clockcore::Said w = clockcore::saidEs(h, m);
  const bool ok = !std::strcmp(w.lead, lead) && !std::strcmp(w.hour, hour) && !std::strcmp(w.tail, tail);
  if (!ok) std::printf("  %02d:%02d says '%s %s %s'\n", h, m, w.lead, w.hour, w.tail);
  return ok;
}
bool longSays(const int h, const int m, const char* want) {
  char got[96];
  clockcore::longTimeEs(h, m, got, sizeof(got));
  if (std::strcmp(got, want)) std::printf("  %02d:%02d is '%s'\n", h, m, got);
  return !std::strcmp(got, want);
}
}  // namespace

int main() {
  using namespace clockcore;
  check(says(10, 0, "son las", "diez", "en punto") && says(10, 9, "son las", "diez", "y nueve"), "to the minute");
  check(says(10, 15, "son las", "diez", "y cuarto") && says(10, 30, "son las", "diez", "y media"), "quarter and half");
  check(says(10, 31, "son las", "once", "menos veintinueve") && says(10, 45, "son las", "once", "menos cuarto") &&
            says(10, 59, "son las", "once", "menos uno"),
        "past the half it counts down to the next hour");
  check(says(12, 40, "es la", "una", "menos veinte") && says(13, 5, "es la", "una", "y cinco") &&
            says(1, 0, "es la", "una", "en punto"),
        "one o'clock is singular");
  check(says(0, 0, "son las", "doce", "en punto") && says(23, 58, "son las", "doce", "menos dos") &&
            says(12, 22, "son las", "doce", "y veintidós"),
        "midnight and noon");
  for (int h = 0; h < 24; ++h) {
    for (int m = 0; m < 60; ++m) {
      const Said w = saidEs(h, m);
      check(w.lead[0] && w.hour[0] && w.tail[0] && std::strlen(w.tail) <= 18, "every minute has words", h * 100 + m);
      char line[96];
      longTimeEs(h, m, line, sizeof(line));
      check(std::strlen(line) > 12 && std::strlen(line) < 60, "every minute has its line", h * 100 + m);
    }
  }
  check(longSays(10, 9, "las diez y nueve de la mañana") && longSays(13, 0, "la una en punto de la tarde") &&
            longSays(23, 59, "las once y cincuenta y nueve de la noche") &&
            longSays(0, 30, "las doce y media de la madrugada"),
        "the time in one line");
  char number[24];
  numberEs(31, number, sizeof(number));
  check(!std::strcmp(number, "treinta y uno"), "thirty-one");
  numberEs(16, number, sizeof(number));
  check(!std::strcmp(number, "dieciséis"), "sixteen has its accent");
  // Day of the year from 0, weekday from Sunday 0.
  check(isoWeek(2026, 321, 3) == 47, "18 November 2026 is week 47");
  check(isoWeek(2026, 0, 4) == 1 && isoWeek(2027, 0, 5) == 53 && isoWeek(2024, 365, 2) == 1, "the edges of a year");
  check(isoWeek(2021, 2, 0) == 53 && isoWeek(2021, 3, 1) == 1, "a year that begins inside week 53");
  check(!std::strcmp(weekdayLong(3, true), "MIÉRCOLES") && !std::strcmp(weekdayShort(6, false), "sáb") &&
            !std::strcmp(monthLong(11), "noviembre") && !std::strcmp(monthRoman(11), "XI"),
        "names with their accents");

  for (int d = 0; d < 10; ++d) {
    const Stroke* s = nullptr;
    const int n = strokesOf(d, s);
    check(n >= 1 && n <= 3 && s != nullptr, "strokes", d);
    for (int i = 0; i < n; ++i) {
      if (s[i].kind == 'L') {
        for (int k = 0; k < 4; ++k) {
          const float limit = k % 2 ? kNumeralHeight : 1.0f;
          check(s[i].v[k] >= -0.001f && s[i].v[k] <= limit + 0.001f, "a line stays in the box", d);
        }
      } else {
        check(s[i].kind == 'A' && s[i].v[5] > s[i].v[4], "an arc runs forward", d);
        // Sample the arc: every point inside the box, give or take a hair.
        for (int k = 0; k <= 40; ++k) {
          const float a = (s[i].v[4] + (s[i].v[5] - s[i].v[4]) * k / 40) * 3.14159265f / 180;
          const float x = s[i].v[0] + s[i].v[2] * std::cos(a);
          const float y = s[i].v[1] + s[i].v[3] * std::sin(a);
          check(x >= -0.02f && x <= 1.02f && y >= -0.02f && y <= kNumeralHeight + 0.02f, "an arc stays in the box", d);
        }
      }
    }
  }
  check(!std::strcmp(weekdayEs(0), "DOM") && !std::strcmp(weekdayEs(1), "LUN") && !std::strcmp(weekdayEs(7), "DOM"),
        "weekdays");
  check(!std::strcmp(monthEs(1), "ENE") && !std::strcmp(monthEs(12), "DIC") && !std::strcmp(monthEs(0), "ENE"),
        "months");
  for (int f = 0; f < kFaces; ++f) check(faceName(f)[0] != 0, "faces are named", f);

  if (failures == 0) std::printf("clock: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
