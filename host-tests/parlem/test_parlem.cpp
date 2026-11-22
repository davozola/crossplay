// PARLEM's phrases, checked on a laptop: three hundred, each whole and with
// its tip, none twice, short enough to be set large, and none with a sign the
// device's serif cuts do not have; then the sections, the day and the state.
#include <cstdio>
#include <cstring>

#include "ParlemCore.h"

using namespace parlem;

static int failures = 0;
static int checks = 0;
static void check(const bool ok, const char* what, const char* which = "") {
  ++checks;
  if (ok) return;
  ++failures;
  std::printf("FAIL %s  %s\n", what, which);
}

static int letters(const char* s) {
  int n = 0;
  for (; *s; ++s) n += (static_cast<unsigned char>(*s) & 0xC0) != 0x80;
  return n;
}

// ASCII and the two-byte Latin-1 letters and signs.
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
  check(kCount == 300, "three hundred phrases");
  check(kTopicCount == 12 && kSectionCount == 13, "twelve topics, and all of them");
  for (int i = 0; i < kCount; ++i) {
    const Phrase& p = kPhrases[i];
    check(p.topic < kTopicCount, "a phrase has a topic");
    check(p.ca && p.ca[0] && p.es && p.es[0] && p.tip && p.tip[0], "a phrase is whole and has its tip",
          p.ca ? p.ca : "");
    if (!p.ca || !p.es || !p.tip) continue;
    check(drawable(p.ca) && drawable(p.es) && drawable(p.tip), "every sign can be drawn", p.ca);
    check(letters(p.ca) <= 32 && letters(p.es) <= 36, "short enough to be set large", p.ca);
    check(letters(p.tip) <= 90, "the tip fits its page", p.ca);
    for (int a = 0; a < i; ++a) check(std::strcmp(kPhrases[a].ca, p.ca) != 0, "no phrase twice", p.ca);
    // While every topic still has phrases left, a week is not seven of one.
    if (i > 0 && i < 240) check(kPhrases[i - 1].topic != p.topic, "neighbours differ in topic", p.ca);
  }
  for (int t = 0; t < kTopicCount; ++t) {
    check(drawable(kTopics[t]) && letters(kTopics[t]) <= 16, "a topic's name fits", kTopics[t]);
  }

  // Sections.
  int sum = 0;
  for (int s = 1; s < kSectionCount; ++s) {
    const int count = countIn(s);
    sum += count;
    check(count >= 20, "a section is worth opening", sectionName(s));
    int at = nth(s, 0);
    check(inSection(at, s) && placeIn(at, s) == 0, "a section's first", sectionName(s));
    for (int n = 1; n < count; ++n) {
      const int next = stepIn(at, s, 1);
      check(next > at && inSection(next, s) && placeIn(next, s) == n && nth(s, n) == next, "stepping stays inside",
            sectionName(s));
      check(stepIn(next, s, -1) == at, "and comes back", sectionName(s));
      at = next;
    }
    check(stepIn(at, s, 1) == nth(s, 0), "a section goes round", sectionName(s));
  }
  check(sum == kCount && countIn(0) == kCount, "every phrase is in one section");
  check(stepIn(kCount - 1, 0, 1) == 0 && stepIn(0, 0, -1) == kCount - 1 && stepIn(4, 0, 1) == 5,
        "all of them go round");
  check(std::strcmp(sectionName(0), "TODAS") == 0 && sectionName(1) == kTopics[0], "names");

  // A day has its phrase, in all of them and inside a section.
  const long day = dayNumber(2026, 11, 22);
  check(phraseFor(0, day + 1) == stepIn(phraseFor(0, day), 0, 1), "tomorrow's is the next");
  check(phraseFor(0, day + kCount) == phraseFor(0, day), "it comes round");
  for (int s = 1; s < kSectionCount; ++s) {
    check(inSection(phraseFor(s, day), s) && phraseFor(s, day + 1) == stepIn(phraseFor(s, day), s, 1) &&
              inSection(phraseFor(s, -1), s),
          "a section has its own day's phrase", sectionName(s));
  }

  State state;
  open(state, day);
  check(state.index == phraseFor(0, day) && state.lastDay == day, "the first opening shows today's");
  state.index = stepIn(state.index, 0, 1);
  const int kept = state.index;
  open(state, day);
  check(state.index == kept, "the same day keeps the phrase tapped to");
  choose(state, 5);
  check(state.section == 5 && inSection(state.index, 5), "choosing a section lands inside it");
  const int inside = state.index;
  choose(state, 5);
  check(state.index == inside, "choosing it again moves nothing");
  open(state, day + 1);
  check(state.index == phraseFor(5, day + 1), "a new day inside a section shows the section's");
  choose(state, 0);
  check(state.index == phraseFor(5, day + 1), "back to all of them keeps the page");
  State unset;
  unset.index = 7;
  open(unset, -1);
  check(unset.index == 7 && unset.lastDay == -1, "no clock, no change");

  uint8_t bytes[kStateBytes];
  State saved;
  saved.style = Style::Diccionari;
  saved.section = 12;
  saved.index = nth(12, countIn(12) - 1);
  saved.lastDay = day;
  check(saved.index > 255, "an index past a byte");
  packState(saved, bytes);
  State read;
  check(unpackState(bytes, kStateBytes, read) && read.style == Style::Diccionari && read.section == 12 &&
            read.index == saved.index && read.lastDay == day,
        "state");
  bytes[2] = 3;
  check(!unpackState(bytes, kStateBytes, read) && read.index == 0, "a phrase outside its section is refused");
  packState(saved, bytes);
  bytes[2] = static_cast<uint8_t>(kSectionCount);
  check(!unpackState(bytes, kStateBytes, read), "a section that is gone is refused");
  packState(saved, bytes);
  bytes[3] = static_cast<uint8_t>(kCount & 0xFF);
  bytes[4] = static_cast<uint8_t>(kCount >> 8);
  bytes[2] = 0;
  check(!unpackState(bytes, kStateBytes, read), "a phrase that is gone is refused");
  check(!unpackState(bytes, 7, read), "a short state is refused");
  State never;
  packState(never, bytes);
  check(unpackState(bytes, kStateBytes, read) && read.lastDay == -1, "a day never seen survives the trip");

  std::printf("%d checks, %d failed\n", checks, failures);
  if (failures == 0) std::printf("parlem: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
