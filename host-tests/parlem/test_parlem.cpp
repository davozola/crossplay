// PARLEM's phrases, checked on a laptop: a hundred and fifty, each whole, none
// twice, short enough to be set large, and none with a sign the device's
// serif cuts do not have.
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
  check(kCount == 150, "a hundred and fifty phrases");
  int tips = 0;
  for (int i = 0; i < kCount; ++i) {
    const Phrase& p = kPhrases[i];
    check(p.topic && p.topic[0] && p.ca && p.ca[0] && p.es && p.es[0] && p.tip, "a phrase is whole", p.ca ? p.ca : "");
    if (!p.topic || !p.ca || !p.es || !p.tip) continue;
    check(drawable(p.topic) && drawable(p.ca) && drawable(p.es) && drawable(p.tip), "every sign can be drawn", p.ca);
    check(letters(p.ca) <= 32 && letters(p.es) <= 36, "short enough to be set large", p.ca);
    check(letters(p.tip) <= 60 && letters(p.topic) <= 16, "the tip and the topic fit their lines", p.ca);
    tips += p.tip[0] != 0;
    for (int a = 0; a < i; ++a) check(std::strcmp(kPhrases[a].ca, p.ca) != 0, "no phrase twice", p.ca);
    // A week is not seven of one topic.
    if (i > 0 && i < 100) check(std::strcmp(kPhrases[i - 1].topic, p.topic) != 0, "neighbours differ in topic", p.ca);
  }
  check(tips >= kCount / 2, "more than half carry a tip");
  std::printf("%d checks, %d failed\n", checks, failures);
  if (failures == 0) std::printf("parlem: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
