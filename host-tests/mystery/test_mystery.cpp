// MISTERIOS host tests: every case can be solved by asking and looking, cannot
// be solved by guessing alone, and every text fits the screen it is shown on
// and the alphabet of the face it is set in.

#include <cstdio>
#include <cstring>

#include "MysteryCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what, const char* detail = "") {
  if (!ok) {
    if (failures < 30) std::printf("FAIL: %s %.60s\n", what, detail);
    ++failures;
  }
}

// Characters, not bytes; -1 if the text leaves Latin-1 or is not UTF-8.
int latinLength(const char* s) {
  int n = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p, ++n) {
    if (*p < 0x80) continue;
    if ((*p == 0xC2 || *p == 0xC3) && (p[1] & 0xC0) == 0x80) {
      ++p;
      continue;
    }
    return -1;
  }
  return n;
}
bool prose(const char* s, const int max) {
  const int n = latinLength(s);
  return n > 0 && n <= max;
}
bool label(const char* s, const int max) {
  const size_t n = std::strlen(s);
  if (n == 0 || n > static_cast<size_t>(max)) return false;
  for (const char* p = s; *p; ++p) {
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == ' ')) return false;
  }
  return true;
}
}  // namespace

int main() {
  using namespace mystery;
  for (int ci = 0; ci < kCases; ++ci) {
    const Case& c = caseAt(ci);
    check(label(c.title, 16), "title", c.title);
    check(prose(c.intro, 400), "intro length", c.title);
    check(prose(c.solution, 400), "solution length", c.title);
    check(c.culprit < kSuspects && c.clueCount >= 3 && c.clueCount <= kMaxClues, "shape", c.title);
    check(c.motive < c.clueCount && c.opportunity < c.clueCount && c.alibi < c.clueCount, "key clues exist", c.title);
    check(c.motive != c.opportunity && c.motive != c.alibi && c.opportunity != c.alibi, "key clues differ", c.title);
    for (int k = 0; k < c.clueCount; ++k) check(prose(c.clues[k], 125), "clue length", c.clues[k]);
    bool given[kMaxClues] = {};
    for (int s = 0; s < kSuspects; ++s) {
      const Suspect& su = c.suspects[s];
      check(label(su.name, 16) && label(su.role, 20), "suspect labels", su.name);
      check(prose(su.about, 170), "about length", su.name);
      check(s == c.culprit ? su.alibi[0] == 0 : prose(su.alibi, 220), "everyone but the culprit has an alibi", su.name);
      check(su.questions[0].needs < 0, "the first question is always open", su.name);
      for (int q = 0; q < kQuestions; ++q) {
        const Question& qu = su.questions[q];
        check(prose(qu.ask, 56), "question length", qu.ask);
        check(prose(qu.answer, 250), "answer length", qu.ask);
        check(qu.needs < c.clueCount && qu.gives < c.clueCount && (qu.needs < 0 || qu.needs != qu.gives),
              "clue indexes", qu.ask);
        if (qu.gives >= 0) given[qu.gives] = true;
      }
    }
    for (int p = 0; p < kPlaces; ++p) {
      check(label(c.places[p].name, 20) && prose(c.places[p].text, 260), "place", c.places[p].name);
      check(c.places[p].gives >= 0 && c.places[p].gives < c.clueCount, "a place always shows something",
            c.places[p].name);
      given[c.places[p].gives] = true;
    }
    for (int k = 0; k < c.clueCount; ++k) check(given[k], "every clue can be found", c.clues[k]);

    // Guessing alone never solves it.
    for (int s = 0; s < kSuspects; ++s) {
      Progress p;
      check(accuse(c, p, s) != Verdict::Solved && p.wrong == 1 && !p.solved, "no clues, no case", c.title);
    }
    // Doing everything there is to do does.
    Progress p;
    bool moved = true;
    while (moved) {
      moved = false;
      for (int s = 0; s < kSuspects; ++s) {
        for (int q = 0; q < kQuestions; ++q) {
          if (canAsk(c, p, s, q) && !wasAsked(p, s, q)) {
            ask(c, p, s, q);
            moved = true;
          }
        }
      }
      for (int pl = 0; pl < kPlaces; ++pl) {
        if (!((p.examined >> pl) & 1)) {
          examine(c, p, pl);
          moved = true;
        }
      }
    }
    check(clueCount(p) == c.clueCount && p.steps == 15, "everything can be asked and found", c.title);
    for (int s = 0; s < kSuspects; ++s) check(hiddenQuestions(c, p, s) == 0, "nothing stays hidden", c.title);
    for (int s = 0; s < kSuspects; ++s) {
      if (s != c.culprit) check(accuse(c, p, s) == Verdict::WrongSuspect, "the innocent stay innocent", c.title);
    }
    check(p.wrong == 3 && accuse(c, p, c.culprit) == Verdict::Solved && p.solved && p.best == 4, "solved, as an agent",
          c.title);
    const uint8_t stepsThen = p.steps;
    ask(c, p, 0, 0);
    check(p.steps == stepsThen, "asking again costs nothing");
    restart(p);
    check(p.best == 4 && p.clues == 0 && !p.solved, "restart keeps the rank");

    // Each key clue is needed.
    for (int missing = 0; missing < 3; ++missing) {
      Progress q;
      q.clues = static_cast<uint8_t>((1u << c.clueCount) - 1);
      const int clue = missing == 0 ? c.motive : (missing == 1 ? c.opportunity : c.alibi);
      q.clues = static_cast<uint8_t>(q.clues & ~(1u << clue));
      const Verdict v = accuse(c, q, c.culprit);
      check(v == (missing == 0 ? Verdict::NoMotive : (missing == 1 ? Verdict::NoOpportunity : Verdict::NoAlibi)),
            "each proof is needed", c.title);
    }
    // The shortest way through earns the top rank.
    Progress best;
    best.clues = static_cast<uint8_t>((1u << c.motive) | (1u << c.opportunity) | (1u << c.alibi));
    best.steps = 6;
    check(accuse(c, best, c.culprit) == Verdict::Solved && best.best == 1, "a clean case is a chief inspector's");
  }

  Progress all[kCases];
  all[1].clues = 0x15, all[1].asked = 0x0A5, all[1].examined = 5, all[1].steps = 7, all[1].wrong = 1, all[2].best = 2,
  all[2].solved = true;
  uint8_t blob[kSaveBytes];
  check(save(all, blob, sizeof(blob)) == kSaveBytes && save(all, blob, 3) == 0, "save size");
  Progress back[kCases];
  check(restore(back, blob, kSaveBytes) && back[1].asked == 0x0A5 && back[1].clues == 0x15 && back[1].steps == 7 &&
            back[2].solved && back[2].best == 2 && back[0].steps == 0,
        "round trip");
  blob[0] = 7;
  check(!restore(back, blob, kSaveBytes), "unknown version");
  check(std::strcmp(rankName(1), "INSPECTOR JEFE") == 0 && std::strcmp(rankName(0), "SIN RESOLVER") == 0, "rank names");

  if (failures == 0) std::printf("mystery: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
