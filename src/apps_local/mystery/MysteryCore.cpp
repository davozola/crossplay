#include "MysteryCore.h"

namespace mystery {

bool hasClue(const Progress& p, const int clue) { return clue >= 0 && clue < kMaxClues && ((p.clues >> clue) & 1); }

int clueCount(const Progress& p) {
  int n = 0;
  for (int i = 0; i < kMaxClues; ++i) n += (p.clues >> i) & 1;
  return n;
}

bool wasAsked(const Progress& p, const int suspect, const int question) {
  return (p.asked >> (suspect * kQuestions + question)) & 1;
}

bool canAsk(const Case& c, const Progress& p, const int suspect, const int question) {
  if (suspect < 0 || suspect >= kSuspects || question < 0 || question >= kQuestions) return false;
  const int needs = c.suspects[suspect].questions[question].needs;
  return needs < 0 || hasClue(p, needs);
}

int hiddenQuestions(const Case& c, const Progress& p, const int suspect) {
  int n = 0;
  for (int q = 0; q < kQuestions; ++q) n += canAsk(c, p, suspect, q) ? 0 : 1;
  return n;
}

namespace {
int note(Progress& p, const int clue) {
  if (clue < 0 || hasClue(p, clue)) return -1;
  p.clues = static_cast<uint8_t>(p.clues | (1u << clue));
  return clue;
}
void step(Progress& p) {
  if (p.steps < 250) ++p.steps;
}
}  // namespace

int ask(const Case& c, Progress& p, const int suspect, const int question) {
  if (!canAsk(c, p, suspect, question)) return -1;
  if (!wasAsked(p, suspect, question)) {
    p.asked = static_cast<uint16_t>(p.asked | (1u << (suspect * kQuestions + question)));
    if (!p.solved) step(p);
  }
  return note(p, c.suspects[suspect].questions[question].gives);
}

int examine(const Case& c, Progress& p, const int place) {
  if (place < 0 || place >= kPlaces) return -1;
  if (!((p.examined >> place) & 1)) {
    p.examined = static_cast<uint8_t>(p.examined | (1u << place));
    if (!p.solved) step(p);
  }
  return note(p, c.places[place].gives);
}

Verdict accuse(const Case& c, Progress& p, const int suspect) {
  Verdict v = Verdict::Solved;
  if (suspect != c.culprit)
    v = Verdict::WrongSuspect;
  else if (!hasClue(p, c.motive))
    v = Verdict::NoMotive;
  else if (!hasClue(p, c.opportunity))
    v = Verdict::NoOpportunity;
  else if (!hasClue(p, c.alibi))
    v = Verdict::NoAlibi;
  if (p.solved) return v;
  if (v != Verdict::Solved) {
    if (p.wrong < 250) ++p.wrong;
    return v;
  }
  p.solved = true;
  const int rank = rankOf(p);
  if (p.best == 0 || rank < p.best) p.best = static_cast<uint8_t>(rank);
  return v;
}

int rankOf(const Progress& p) {
  if (p.wrong == 0 && p.steps <= 10) return 1;
  if (p.wrong == 0) return 2;
  if (p.wrong == 1) return 3;
  return 4;
}

const char* rankName(const int rank) {
  static const char* const kNames[5] = {"SIN RESOLVER", "INSPECTOR JEFE", "INSPECTOR", "SUBINSPECTOR", "AGENTE"};
  return kNames[rank < 0 || rank > 4 ? 0 : rank];
}

void restart(Progress& p) {
  const uint8_t best = p.best;
  p = Progress{};
  p.best = best;
}

size_t save(const Progress* all, uint8_t* out, const size_t cap) {
  if (cap < kSaveBytes) return 0;
  out[0] = 1;
  for (int i = 0; i < kCases; ++i) {
    uint8_t* o = out + 1 + i * 8;
    const Progress& p = all[i];
    o[0] = p.clues;
    o[1] = static_cast<uint8_t>(p.asked & 0xFF);
    o[2] = static_cast<uint8_t>(p.asked >> 8);
    o[3] = p.examined;
    o[4] = p.steps;
    o[5] = p.wrong;
    o[6] = p.best;
    o[7] = p.solved ? 1 : 0;
  }
  return kSaveBytes;
}

bool restore(Progress* all, const uint8_t* in, const size_t size) {
  if (size != kSaveBytes || in[0] != 1) return false;
  for (int i = 0; i < kCases; ++i) {
    const uint8_t* o = in + 1 + i * 8;
    if (o[6] > 4 || o[7] > 1 || o[0] >= (1u << kMaxClues) || o[3] >= (1u << kPlaces) || o[2] >= 0x10) return false;
  }
  for (int i = 0; i < kCases; ++i) {
    const uint8_t* o = in + 1 + i * 8;
    Progress& p = all[i];
    p.clues = o[0];
    p.asked = static_cast<uint16_t>(o[1] | (o[2] << 8));
    p.examined = o[3];
    p.steps = o[4];
    p.wrong = o[5];
    p.best = o[6];
    p.solved = o[7] != 0;
  }
  return true;
}

}  // namespace mystery
