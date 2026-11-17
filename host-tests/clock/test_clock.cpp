// RELOJ host tests: the time in words, and that every numeral's strokes,
// segments and dots are sane.

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
  const clockcore::Words w = clockcore::wordsEs(h, m);
  const bool ok = !std::strcmp(w.lead, lead) && !std::strcmp(w.hour, hour) && !std::strcmp(w.tail, tail);
  if (!ok) std::printf("  %02d:%02d says '%s %s %s'\n", h, m, w.lead, w.hour, w.tail);
  return ok;
}
}  // namespace

int main() {
  using namespace clockcore;
  check(says(10, 0, "SON LAS", "DIEZ", "EN PUNTO"), "on the hour");
  check(says(10, 2, "SON LAS", "DIEZ", "EN PUNTO") && says(10, 3, "SON LAS", "DIEZ", "Y CINCO"), "rounds to five");
  check(says(10, 15, "SON LAS", "DIEZ", "Y CUARTO") && says(10, 30, "SON LAS", "DIEZ", "Y MEDIA"), "quarter and half");
  check(says(10, 32, "SON LAS", "DIEZ", "Y MEDIA") && says(10, 33, "SON LAS", "ONCE", "MENOS VEINTICINCO"),
        "past the half it counts down to the next hour");
  check(says(10, 45, "SON LAS", "ONCE", "MENOS CUARTO") && says(10, 58, "SON LAS", "ONCE", "EN PUNTO"),
        "toward the hour");
  check(says(12, 40, "ES LA", "UNA", "MENOS VEINTE") && says(13, 5, "ES LA", "UNA", "Y CINCO") &&
            says(1, 0, "ES LA", "UNA", "EN PUNTO"),
        "one o'clock is singular");
  check(says(0, 0, "SON LAS", "DOCE", "EN PUNTO") && says(23, 58, "SON LAS", "DOCE", "EN PUNTO") &&
            says(12, 20, "SON LAS", "DOCE", "Y VEINTE"),
        "midnight and noon");
  check(says(23, 40, "SON LAS", "DOCE", "MENOS VEINTE") && says(15, 25, "SON LAS", "TRES", "Y VEINTICINCO"),
        "afternoon");
  for (int h = 0; h < 24; ++h) {
    for (int m = 0; m < 60; ++m) {
      const Words w = wordsEs(h, m);
      check(w.lead[0] && w.hour[0] && w.tail[0] && std::strlen(w.tail) <= 17, "every minute has words", h * 100 + m);
    }
  }

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
    int lit = 0;
    for (int b = 0; b < 7; ++b) lit += (segmentsOf(d) >> b) & 1;
    check(lit >= 2 && lit <= 7 && segmentsOf(d) < 0x80, "segments", d);
    int dots = 0;
    for (int r = 0; r < 7; ++r) {
      check(dotsOf(d)[r] < 0x20, "five dots a row", d);
      for (int b = 0; b < 5; ++b) dots += (dotsOf(d)[r] >> b) & 1;
    }
    check(dots >= 9 && dots <= 22, "dots", d);
  }
  check(segmentsOf(8) == 0x7F && segmentsOf(1) == 0x06 && segmentsOf(0) == 0x3F, "known segments");
  for (int a = 0; a < 10; ++a) {
    for (int b = a + 1; b < 10; ++b) {
      check(segmentsOf(a) != segmentsOf(b) && std::memcmp(dotsOf(a), dotsOf(b), 7) != 0, "no two numerals alike",
            a * 10 + b);
    }
  }
  check(!std::strcmp(weekdayEs(0), "DOM") && !std::strcmp(weekdayEs(1), "LUN") && !std::strcmp(weekdayEs(7), "DOM"),
        "weekdays");
  check(!std::strcmp(monthEs(1), "ENE") && !std::strcmp(monthEs(12), "DIC") && !std::strcmp(monthEs(0), "ENE"),
        "months");
  for (int f = 0; f < kFaces; ++f) check(faceName(f)[0] != 0, "faces are named", f);

  // The clock of clocks: a hand that points down is met by one pointing back
  // up, or the stroke would stop in the middle of a dial. Not the other way:
  // with two hands a dial, the waist of an 8 cannot also reach down.
  for (int d = 0; d < 10; ++d) {
    const Dial* dials = dialsOf(d);
    for (int i = 0; i < 6; ++i) {
      for (const Hand hand : {dials[i].a, dials[i].b}) {
        if (hand == Down && i < 4) {
          const Dial below = dials[i + 2];
          check(below.a == Up || below.b == Up, "a stroke down is met from below", d * 10 + i);
        }
        check(!(hand == Up && i < 2) && !(hand == Down && i >= 4), "no stroke leaves the numeral", d * 10 + i);
      }
    }
  }

  if (failures == 0) std::printf("clock: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
