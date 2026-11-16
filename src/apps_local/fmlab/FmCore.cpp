#include "FmCore.h"

#include <cmath>

namespace fm {

namespace {

// Each algorithm as "who modulates whom" pairs (modulator then target), the
// carriers, and the feedback loop (from, into).
struct Row {
  const char* edges;
  const char* carriers;
  const char* feedback;
};
constexpr Row kRows[kAlgorithms] = {
    {"21 65 54 43", "13", "66"},   {"21 65 54 43", "13", "22"},   {"32 21 65 54", "14", "66"},
    {"32 21 65 54", "14", "46"},   {"21 43 65", "135", "66"},     {"21 43 65", "135", "56"},
    {"21 43 53 65", "13", "66"},   {"21 43 53 65", "13", "44"},   {"21 43 53 65", "13", "22"},
    {"32 21 54 64", "14", "33"},   {"32 21 54 64", "14", "66"},   {"21 43 53 63", "13", "22"},
    {"21 43 53 63", "13", "66"},   {"21 43 54 64", "13", "66"},   {"21 43 54 64", "13", "22"},
    {"21 31 51 43 65", "1", "66"}, {"21 31 51 43 65", "1", "22"}, {"21 31 41 54 65", "1", "33"},
    {"32 21 64 65", "145", "66"},  {"31 32 54 64", "124", "33"},  {"31 32 64 65", "1245", "33"},
    {"21 63 64 65", "1345", "66"}, {"32 64 65", "1245", "66"},    {"63 64 65", "12345", "66"},
    {"64 65", "12345", "66"},      {"32 54 64", "124", "66"},     {"32 54 64", "124", "33"},
    {"21 54 43", "136", "55"},     {"43 65", "1235", "66"},       {"54 43", "1236", "55"},
    {"65", "12345", "66"},         {"", "123456", "66"},
};

uint8_t bit(const int op) { return static_cast<uint8_t>(1u << (op - 1)); }

// The lowest-numbered operator `op` modulates, or 0 for a carrier.
int firstTarget(const Algorithm& a, const int op) {
  for (int t = 1; t <= kOps; ++t) {
    if (a.modulators[t - 1] & bit(op)) return t;
  }
  return 0;
}

float widthOf(const Algorithm& a, const int op) {
  float w = 0;
  for (int m = 1; m <= kOps; ++m) {
    if (firstTarget(a, m) == op) w += widthOf(a, m);
  }
  return w < 1 ? 1 : w;
}

void place(const Algorithm& a, const int op, const float left, const int row, Diagram& d) {
  const float w = widthOf(a, op);
  d.x[op - 1] = left + (w - 1) / 2;
  d.row[op - 1] = static_cast<uint8_t>(row);
  if (row + 1 > d.rows) d.rows = row + 1;
  float at = left;
  for (int m = 1; m <= kOps; ++m) {
    if (firstTarget(a, m) != op) continue;
    place(a, m, at, row + 1, d);
    at += widthOf(a, m);
  }
}

}  // namespace

int Algorithm::carrierCount() const {
  int n = 0;
  for (int op = 1; op <= kOps; ++op) n += isCarrier(op) ? 1 : 0;
  return n;
}

int Algorithm::stackSize(const int op) const {
  uint8_t seen = bit(op);
  // Six operators: six passes reach everything that can be reached.
  for (int pass = 0; pass < kOps; ++pass) {
    for (int t = 1; t <= kOps; ++t) {
      if (seen & bit(t)) seen = static_cast<uint8_t>(seen | modulators[t - 1]);
    }
  }
  int n = 0;
  for (int o = 1; o <= kOps; ++o) n += (seen & bit(o)) ? 1 : 0;
  return n;
}

Algorithm algorithm(int number) {
  if (number < 1) number = 1;
  if (number > kAlgorithms) number = kAlgorithms;
  const Row& row = kRows[number - 1];
  Algorithm a;
  for (const char* e = row.edges; *e; ++e) {
    if (*e == ' ') continue;
    const int from = e[0] - '0';
    const int to = e[1] - '0';
    a.modulators[to - 1] = static_cast<uint8_t>(a.modulators[to - 1] | bit(from));
    ++e;
  }
  for (const char* c = row.carriers; *c; ++c) a.carriers = static_cast<uint8_t>(a.carriers | bit(*c - '0'));
  a.feedbackFrom = static_cast<uint8_t>(row.feedback[0] - '0');
  a.feedbackTo = static_cast<uint8_t>(row.feedback[1] - '0');
  return a;
}

const char* character(const Algorithm& a) {
  const int carriers = a.carrierCount();
  int tallest = 0;
  for (int op = 1; op <= kOps; ++op) {
    if (a.isCarrier(op) && a.stackSize(op) > tallest) tallest = a.stackSize(op);
  }
  if (carriers == 6) return "SIX SINES SIDE BY SIDE: ORGANS AND ADDITIVE SOUNDS. NOTHING MODULATES ANYTHING.";
  if (carriers >= 4) return "MOSTLY PLAIN CARRIERS WITH A LITTLE COLOUR: ORGANS, PADS, LAYERED SOUNDS.";
  if (carriers == 3 && tallest == 2) return "THREE SIMPLE PAIRS: ELECTRIC PIANOS, BELLS, EACH PAIR A LAYER OF ITS OWN.";
  if (carriers == 3) return "THREE VOICES OF DIFFERENT WEIGHT: A BODY, A BRIGHT STACK AND SOMETHING BESIDE THEM.";
  if (carriers == 1) return "EVERYTHING FEEDS ONE CARRIER: ONE DENSE, COMPLEX VOICE. LEADS, BRASS, EFFECTS.";
  if (tallest >= 4) return "A PLAIN PAIR FOR THE BODY AND A TALL STACK FOR BITE: BASSES, STRINGS, CLAVS.";
  return "TWO STACKS TO SET AGAINST EACH OTHER: ONE FOR THE ATTACK, ONE FOR THE SUSTAIN.";
}

Diagram layout(const Algorithm& a) {
  Diagram d;
  float left = 0;
  for (int op = 1; op <= kOps; ++op) {
    if (!a.isCarrier(op)) continue;
    place(a, op, left, 0, d);
    left += widthOf(a, op);
  }
  d.columns = left < 1 ? 1 : left;
  // A modulator shared by several operators sits over the middle of them.
  for (int op = 1; op <= kOps; ++op) {
    float sum = 0;
    int targets = 0;
    for (int t = 1; t <= kOps; ++t) {
      if (a.modulators[t - 1] & bit(op)) {
        sum += d.x[t - 1];
        ++targets;
      }
    }
    if (targets > 1) d.x[op - 1] = sum / static_cast<float>(targets);
  }
  return d;
}

double bessel(const int n, const double x) {
  // The power series; plenty for the indexes an FM voice uses.
  double term = 1;
  for (int k = 1; k <= n; ++k) term *= (x / 2) / k;
  double sum = term;
  for (int k = 1; k < 40; ++k) {
    term *= -(x / 2) * (x / 2) / (static_cast<double>(k) * (k + n));
    sum += term;
  }
  return sum;
}

void spectrum(const double c, const double m, const double index, double out[kPartials]) {
  for (int k = 0; k < kPartials; ++k) out[k] = 0;
  for (int n = -24; n <= 24; ++n) {
    double amplitude = bessel(n < 0 ? -n : n, index);
    if (n < 0 && (n % 2 != 0)) amplitude = -amplitude;  // J(-n) = (-1)^n J(n)
    double frequency = c + n * m;
    if (frequency < 0) {
      frequency = -frequency;
      amplitude = -amplitude;  // a sine at a negative frequency is the same sine upside down
    }
    const int slot = static_cast<int>(std::lround(frequency * 2));
    if (slot >= 0 && slot < kPartials && std::fabs(frequency * 2 - slot) < 0.01) out[slot] += amplitude;
  }
  for (int k = 0; k < kPartials; ++k) out[k] = std::fabs(out[k]);
  out[0] = 0;  // a partial at zero is not a sound
}

bool harmonic(const double c, const double m) {
  // With whole-number ratios every sideband c + n m is a whole number too.
  return std::fabs(c - std::lround(c)) < 0.01 && std::fabs(m - std::lround(m)) < 0.01;
}

const char* ratioCharacter(const double c, const double m) {
  if (!harmonic(c, m)) return "INHARMONIC: THE PARTIALS DO NOT LINE UP. BELLS, GONGS, METAL.";
  const long ci = std::lround(c);
  const long mi = std::lround(m);
  if (ci == 1 && mi == 1) return "1:1 EVERY HARMONIC, LIKE A SAW: BRASS, STRINGS, BASS.";
  if (ci == 1 && mi == 2) return "1:2 ODD HARMONICS ONLY, LIKE A SQUARE: CLARINET, HOLLOW SOUNDS.";
  if (ci == 1 && mi == 3) return "1:3 EVERY THIRD HARMONIC MISSING, LIKE A NARROW PULSE: NASAL, REEDY.";
  if (ci == 1) return "1:N A FUNDAMENTAL WITH WIDE-SPACED PARTIALS: BRIGHT, GLASSY.";
  if (mi == 1) return "N:1 THE CARRIER SITS HIGH AND THE SIDEBANDS FILL IN BELOW: THIN, BUZZY.";
  return "HARMONIC, BUT THE FUNDAMENTAL MAY BE MISSING: THE EAR HEARS THE SPACING.";
}

double wave(const double c, const double m, const double index, const double t) {
  const double twoPi = 6.283185307179586;
  return std::sin(twoPi * c * t + index * std::sin(twoPi * m * t));
}

const char* envelopeName(const int preset) {
  static const char* const kNames[kEnvelopePresets] = {"ORGAN", "PIANO", "PLUCK", "PAD", "BRASS", "BELL"};
  return kNames[preset < 0 ? 0 : (preset >= kEnvelopePresets ? kEnvelopePresets - 1 : preset)];
}

Envelope envelopePreset(const int preset) {
  static const uint8_t kRates[kEnvelopePresets][4] = {
      {99, 99, 99, 80}, {95, 30, 25, 50}, {99, 60, 40, 70}, {30, 40, 40, 35}, {60, 50, 40, 60}, {99, 35, 20, 30},
  };
  static const uint8_t kLevels[kEnvelopePresets][4] = {
      {99, 99, 99, 0}, {99, 75, 0, 0}, {99, 0, 0, 0}, {99, 90, 85, 0}, {99, 85, 80, 0}, {99, 60, 0, 0},
  };
  const int p = preset < 0 ? 0 : (preset >= kEnvelopePresets ? kEnvelopePresets - 1 : preset);
  Envelope e;
  for (int i = 0; i < 4; ++i) {
    e.rate[i] = kRates[p][i];
    e.level[i] = kLevels[p][i];
  }
  return e;
}

void envelopeShape(const Envelope& e, float x[6], float y[6]) {
  // A stage takes longer the further it has to go and the lower its rate. The
  // true curve is exponential and its times span seconds to minutes; this is
  // the picture, with every stage given room to be seen.
  auto span = [](const int from, const int to, const int rate) {
    const float distance = static_cast<float>(from > to ? from - to : to - from) / 99.0f;
    return 0.04f + distance * static_cast<float>(100 - rate) / 100.0f;
  };
  float w[5];
  w[0] = span(e.level[3], e.level[0], e.rate[0]);
  w[1] = span(e.level[0], e.level[1], e.rate[1]);
  w[2] = span(e.level[1], e.level[2], e.rate[2]);
  w[3] = 0.25f;  // the key held
  w[4] = span(e.level[2], e.level[3], e.rate[3]);
  float total = 0;
  for (const float v : w) total += v;
  const int levels[6] = {e.level[3], e.level[0], e.level[1], e.level[2], e.level[2], e.level[3]};
  float at = 0;
  for (int i = 0; i < 6; ++i) {
    x[i] = at / total;
    y[i] = static_cast<float>(levels[i]) / 99.0f;
    if (i < 5) at += w[i];
  }
}

}  // namespace fm
