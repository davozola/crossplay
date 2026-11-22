// DITA's sayings, checked on a laptop: a hundred and fifty of them, each whole,
// none twice, none with a sign the device's serif cuts do not have; and the
// arithmetic that picks one for a day.
#include <cstdio>
#include <cstring>

#include "DitaCore.h"

using namespace dita;

static int failures = 0;
static int checks = 0;
static void check(const bool ok, const char* what, const char* which = "") {
  ++checks;
  if (ok) return;
  ++failures;
  std::printf("FAIL %s  %s\n", what, which);
}

// Letters as a reader counts them.
static int letters(const char* s) {
  int n = 0;
  for (; *s; ++s) n += (static_cast<unsigned char>(*s) & 0xC0) != 0x80;
  return n;
}

// ASCII and the two-byte Latin-1 letters and signs; nothing from further up,
// where the curly apostrophe and the ellipsis live.
static bool drawable(const char* s) {
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
    if (*p < 0x80) {
      if (*p < 0x20) return false;
      continue;
    }
    if ((*p != 0xC2 && *p != 0xC3) || (p[1] & 0xC0) != 0x80) return false;
    ++p;
  }
  return true;
}

int main() {
  check(kCount == 150, "a hundred and fifty sayings");
  for (int i = 0; i < kCount; ++i) {
    const Saying& s = kSayings[i];
    check(s.ca && s.ca[0] && s.es && s.es[0] && s.extra, "a saying is whole", s.ca ? s.ca : "");
    if (!s.ca || !s.es || !s.extra) continue;
    check(drawable(s.ca) && drawable(s.es) && drawable(s.extra), "every sign can be drawn", s.ca);
    check(letters(s.ca) <= 60 && letters(s.es) <= 60 && letters(s.extra) <= 60, "short enough for the page", s.ca);
    check((s.kind == Extra::None) == (s.extra[0] == 0), "a third line is there when it is named", s.ca);
    check(std::strcmp(s.ca, s.es) != 0, "the Spanish is not the Catalan again", s.ca);
    for (int a = 0; a < i; ++a) check(std::strcmp(kSayings[a].ca, s.ca) != 0, "no saying twice", s.ca);
  }

  check(dayNumber(1970, 1, 1) == 0 && dayNumber(1970, 1, 2) == 1 && dayNumber(1969, 12, 31) == -1, "days from 1970");
  check(dayNumber(2000, 3, 1) - dayNumber(2000, 2, 28) == 2, "2000 was a leap year");
  check(dayNumber(2100, 3, 1) - dayNumber(2100, 2, 28) == 1, "2100 is not");
  check(dayNumber(2026, 11, 22) == 20779, "today");
  check(dayNumber(2027, 1, 1) - dayNumber(2026, 12, 31) == 1, "across a year");

  // A day has one saying, the next day the next, and it comes round.
  const long day = dayNumber(2026, 11, 22);
  check(sayingFor(day + 1) == step(sayingFor(day), 1), "tomorrow's is the next");
  check(sayingFor(day + kCount) == sayingFor(day), "it comes round");
  check(sayingFor(-1) == kCount - 1, "a day before 1970 still has one");
  check(step(kCount - 1, 1) == 0 && step(0, -1) == kCount - 1, "stepping goes round");

  // Opening: a new day shows that day's, the same day leaves the page alone.
  State state;
  open(state, day);
  check(state.index == sayingFor(day) && state.lastDay == day, "the first opening shows today's");
  state.index = step(state.index, 5);
  open(state, day);
  check(state.index == step(sayingFor(day), 5), "the same day keeps the saying tapped to");
  open(state, day + 1);
  check(state.index == sayingFor(day + 1) && state.lastDay == day + 1, "a new day shows its own");
  State unset;
  unset.index = 7;
  open(unset, -1);
  check(unset.index == 7 && unset.lastDay == -1, "no clock, no change");

  uint8_t bytes[kStateBytes];
  State saved;
  saved.style = Style::Diccionari;
  saved.index = 149;
  saved.lastDay = day;
  packState(saved, bytes);
  State read;
  check(unpackState(bytes, kStateBytes, read) && read.style == Style::Diccionari && read.index == 149 &&
            read.lastDay == day,
        "state");
  State never;
  packState(never, bytes);
  check(unpackState(bytes, kStateBytes, read) && read.lastDay == -1, "a day never seen survives the trip");
  bytes[2] = static_cast<uint8_t>(kCount);
  check(!unpackState(bytes, kStateBytes, read) && read.index == 0, "a saying that is gone is refused");
  packState(saved, bytes);
  bytes[1] = static_cast<uint8_t>(kStyleCount);
  check(!unpackState(bytes, kStateBytes, read), "a style that is gone is refused");
  check(!unpackState(bytes, 3, read), "a short state is refused");
  check(stepStyle(Style::Editorial, -1) == Style::Diccionari && stepStyle(Style::Diccionari, 1) == Style::Editorial,
        "styles go round");

  std::printf("%d checks, %d failed\n", checks, failures);
  if (failures == 0) std::printf("dita: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
