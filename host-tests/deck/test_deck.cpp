// DECK's templates, checked on a laptop: every button has a name that fits a
// key, a chord for each system, and no key that moves between keyboards.
#include <cstdio>
#include <cstring>

#include "DeckCore.h"

using namespace deck;

static int failures = 0;
static int checks = 0;
static void check(const bool ok, const char* what, const char* where = "", const char* which = "") {
  ++checks;
  if (ok) return;
  ++failures;
  std::printf("FAIL %s  %s %s\n", what, where, which);
}

// Letters as a reader counts them: a UTF-8 lead byte, not its continuations.
static int lettersIn(const char* s, const char* end) {
  int n = 0;
  for (; s < end; ++s) n += (static_cast<unsigned char>(*s) & 0xC0) != 0x80;
  return n;
}

int main() {
  check(kTemplateCount >= 6, "six templates or more");
  for (int t = 0; t < kTemplateCount; ++t) {
    const Template& tpl = kTemplates[t];
    check(tpl.name && tpl.name[0], "a template is named");
    for (int a = 0; a < t; ++a) check(std::strcmp(kTemplates[a].name, tpl.name) != 0, "names differ", tpl.name);
    for (int b = 0; b < kButtons; ++b) {
      const Button& k = tpl.buttons[b];
      check(k.label && k.label[0], "a button is named", tpl.name);
      if (!k.label) continue;
      // At most two words, each short enough for a fifth of the panel's width.
      int words = 0;
      const char* w = k.label;
      for (const char* p = k.label;; ++p) {
        if (*p == ' ' || *p == 0) {
          ++words;
          check(lettersIn(w, p) >= 1 && lettersIn(w, p) <= 11, "a word fits a key", tpl.name, k.label);
          w = p + 1;
          if (*p == 0) break;
        }
      }
      check(words <= 2, "two words at most", tpl.name, k.label);
      for (int a = 0; a < b; ++a) {
        check(std::strcmp(tpl.buttons[a].label, k.label) != 0, "labels differ within a template", tpl.name, k.label);
      }
      for (int o = 0; o < 2; ++o) {
        const Os os = static_cast<Os>(o);
        const Chord c = k.on(os);
        check(c.usage != 0, "a chord has a key", tpl.name, k.label);
        check(layoutProof(c.usage), "the key does not move between keyboards", tpl.name, k.label);
        check((c.mods & 0xF0) == 0, "left-hand modifiers only", tpl.name, k.label);
        for (int a = 0; a < b; ++a) {
          const Chord d = tpl.buttons[a].on(os);
          // SISTEMA closes a window and quits a program with one chord on Windows, where they are one act.
          const bool sameAct = t == 0 && os == Os::Windows && c.mods == kAlt && c.usage == fkey(4);
          check(sameAct || d.mods != c.mods || d.usage != c.usage, "chords differ within a template", tpl.name, k.label);
        }
        char said[32];
        const int n = describe(c, os, said, sizeof(said));
        check(n > 0 && n < 24, "a chord can be written on a key", tpl.name, k.label);
      }
    }
  }

  char said[32];
  describe({kGui | kShift, letter('T')}, Os::Mac, said, sizeof(said));
  check(!std::strcmp(said, "SHIFT CMD T"), "Mac order", said);
  describe({kCtrl | kShift, letter('T')}, Os::Windows, said, sizeof(said));
  check(!std::strcmp(said, "CTRL SHIFT T"), "Windows order", said);
  describe({kAlt, fkey(4)}, Os::Windows, said, sizeof(said));
  check(!std::strcmp(said, "ALT F4"), "function keys", said);
  describe({0, fkey(11)}, Os::Mac, said, sizeof(said));
  check(!std::strcmp(said, "F11"), "two-digit function keys", said);
  describe({kGui | kAlt, digit(1)}, Os::Mac, said, sizeof(said));
  check(!std::strcmp(said, "OPT CMD 1"), "digits", said);
  char tiny[6];
  describe({kGui | kShift, letter('T')}, Os::Mac, tiny, sizeof(tiny));
  check(std::strlen(tiny) == 5, "a short buffer is filled and closed", tiny);

  uint8_t report[kReportBytes];
  fillReport(report, {kGui | kShift, letter('T')});
  check(report[0] == 0x0A && report[1] == 0 && report[2] == 0x17 && report[3] == 0 && report[7] == 0, "the report");
  check(letter('A') == 0x04 && letter('Z') == 0x1D && digit(1) == 0x1E && digit(0) == 0x27 && fkey(1) == 0x3A &&
            fkey(12) == 0x45,
        "usages");
  check(!layoutProof(0x2F) && !layoutProof(0x33) && !layoutProof(0x2E), "brackets, semicolon and equals are refused");

  uint8_t state[kStateBytes];
  Os os;
  int chosen;
  bool asked;
  packState(Os::Windows, 3, true, state);
  check(unpackState(state, kStateBytes, os, chosen, asked) && os == Os::Windows && chosen == 3 && asked, "state");
  state[2] = static_cast<uint8_t>(kTemplateCount);
  check(!unpackState(state, kStateBytes, os, chosen, asked) && os == Os::Mac && chosen == 0 && !asked,
        "a template that is gone is refused");
  check(!unpackState(state, 2, os, chosen, asked), "a short state is refused");

  std::printf("%d checks, %d failed\n", checks, failures);
  if (failures == 0) std::printf("deck: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
