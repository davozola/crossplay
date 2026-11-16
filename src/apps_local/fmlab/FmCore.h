#pragma once

// FM SYNTH LAB's model: the 32 six-operator algorithms of the classic FM
// layout and where to draw each operator, the spectrum a carrier and a
// modulator make between them, and the four-rate, four-level envelope.
// Freestanding, so host-tests/fmlab can check the table and the arithmetic.

#include <cstdint>

namespace fm {

constexpr int kOps = 6;
constexpr int kAlgorithms = 32;

struct Algorithm {
  // modulators[i]: the operators that modulate operator i + 1, one bit each
  // (bit 0 is operator 1).
  uint8_t modulators[kOps] = {};
  uint8_t carriers = 0;      // the operators that are heard, one bit each
  uint8_t feedbackFrom = 0;  // 1..6
  uint8_t feedbackTo = 0;    // the same operator, or the one it loops back into
  bool isCarrier(int op) const { return (carriers >> (op - 1)) & 1; }
  int carrierCount() const;
  // How many operators stand over this one, itself included.
  int stackSize(int op) const;
};

// `number` is 1..32.
Algorithm algorithm(int number);
// A line on what an algorithm of this shape tends to be good for.
const char* character(const Algorithm& a);

// Where each operator goes in a diagram: carriers along the bottom, each
// modulator above what it modulates. x is in columns, from 0.
struct Diagram {
  float x[kOps] = {};
  uint8_t row[kOps] = {};  // 0 for a carrier, 1 for what modulates it, ...
  float columns = 1;
  int rows = 1;
};
Diagram layout(const Algorithm& a);

// ---- two operators: what a modulator does to a carrier ----

// Bessel function of the first kind, order n >= 0.
double bessel(int n, double x);

constexpr int kPartials = 33;  // frequencies 0, 0.5, 1, ... 16 times the base
// Fills `out[k]` with the size of the partial at k / 2 times the base
// frequency, for a carrier at `c` and a modulator at `m` times it, both whole
// or half numbers, and modulation index `index`. Sidebands that fold back
// through zero come back upside down, as they do in the instrument.
void spectrum(double c, double m, double index, double out[kPartials]);
// True when every partial is a whole multiple of the lowest one.
bool harmonic(double c, double m);
const char* ratioCharacter(double c, double m);
// One sample of the wave, t in 0..1 over one cycle of the base frequency.
double wave(double c, double m, double index, double t);

// ---- the envelope ----

struct Envelope {
  uint8_t rate[4] = {99, 99, 99, 99};
  uint8_t level[4] = {99, 99, 99, 0};
};
constexpr int kEnvelopePresets = 6;
const char* envelopeName(int preset);
Envelope envelopePreset(int preset);
// The envelope as five points across a width of 1: start (at level 4), after
// each of the three key-down stages, a stretch held at level 3, and the
// release back to level 4. `x` and `y` take six values each; y is 0..1.
void envelopeShape(const Envelope& e, float x[6], float y[6]);

}  // namespace fm
