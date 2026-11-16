// FM SYNTH LAB host tests: the algorithm table is a set of sane graphs, the
// diagram never puts two operators in one place, and the spectrum arithmetic
// matches what FM is known to do.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FmCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what, int n = 0) {
  if (!ok) {
    if (failures < 20) std::printf("FAIL: %s (%d)\n", what, n);
    ++failures;
  }
}
bool near(double a, double b, double eps = 1e-3) { return std::fabs(a - b) < eps; }
}  // namespace

int main() {
  using namespace fm;

  static const int kCarriers[kAlgorithms] = {2, 2, 2, 2, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1,
                                             1, 1, 3, 3, 4, 4, 4, 5, 5, 3, 3, 3, 4, 4, 5, 6};
  for (int n = 1; n <= kAlgorithms; ++n) {
    const Algorithm a = algorithm(n);
    check(a.carrierCount() == kCarriers[n - 1], "carrier count", n);
    int edges = 0;
    for (int op = 1; op <= kOps; ++op) {
      bool modulates = false;
      for (int t = 1; t <= kOps; ++t) {
        if ((a.modulators[t - 1] >> (op - 1)) & 1) {
          modulates = true;
          ++edges;
          check(t < op, "a modulator has a higher number than what it modulates", n);
        }
      }
      check(modulates != a.isCarrier(op), "every operator is a carrier or a modulator, never both or neither", n);
    }
    check(a.feedbackFrom >= 1 && a.feedbackFrom <= 6 && a.feedbackTo >= a.feedbackFrom && a.feedbackTo <= 6, "feedback",
          n);
    int reached = 0;
    for (int op = 1; op <= kOps; ++op) {
      if (a.isCarrier(op)) reached += a.stackSize(op);
    }
    check(reached >= kOps, "every operator is over some carrier", n);
    check(character(a)[0] != 0, "has a character line", n);

    const Diagram d = layout(a);
    check(d.rows >= 1 && d.rows <= 4 && d.columns >= 1 && d.columns <= 6, "diagram fits", n);
    for (int op = 1; op <= kOps; ++op) {
      check(a.isCarrier(op) == (d.row[op - 1] == 0), "carriers and only carriers on the bottom row", n);
      check(d.x[op - 1] >= 0 && d.x[op - 1] <= d.columns - 1 + 1e-3, "inside the columns", n);
      for (int t = 1; t <= kOps; ++t) {
        if ((a.modulators[t - 1] >> (op - 1)) & 1)
          check(d.row[op - 1] > d.row[t - 1], "a modulator is drawn above its target", n);
      }
      for (int o = 1; o < op; ++o) {
        check(!(d.row[o - 1] == d.row[op - 1] && std::fabs(d.x[o - 1] - d.x[op - 1]) < 0.9f),
              "no two operators overlap", n);
      }
    }
  }
  {
    const Algorithm a5 = algorithm(5);
    check(a5.stackSize(1) == 2 && a5.stackSize(3) == 2 && a5.stackSize(5) == 2, "5 is three pairs");
    const Algorithm a1 = algorithm(1);
    check(a1.stackSize(1) == 2 && a1.stackSize(3) == 4 && a1.feedbackFrom == 6, "1 is a pair and a stack of four");
    const Algorithm a4 = algorithm(4);
    check(a4.feedbackFrom == 4 && a4.feedbackTo == 6, "4 loops its whole stack");
    const Diagram d19 = layout(algorithm(19));
    check(near(d19.x[5], (d19.x[3] + d19.x[4]) / 2), "a shared modulator sits between its targets");
    check(algorithm(0).carrierCount() == 2 && algorithm(99).carrierCount() == 6, "out of range clamps");
  }

  // Bessel: known values.
  check(near(bessel(0, 0), 1) && near(bessel(1, 0), 0), "J at zero");
  check(near(bessel(0, 2.4048), 0, 1e-3), "first zero of J0");
  check(near(bessel(0, 1), 0.76520, 1e-4) && near(bessel(1, 1), 0.44005, 1e-4) && near(bessel(2, 5), 0.04657, 1e-4),
        "J table values");
  check(near(bessel(3, 10), 0.05838, 1e-4), "J3(10)");

  double s[kPartials];
  spectrum(1, 1, 0, s);
  check(near(s[2], 1) && near(s[4], 0) && near(s[6], 0), "no modulation: the carrier alone");
  spectrum(1, 2, 2, s);
  for (int k = 0; k < kPartials; ++k) {
    const bool odd = k % 4 == 2;  // 1, 3, 5, ... times the base
    if (!odd) check(near(s[k], 0, 1e-6), "1:2 has odd harmonics only", k);
  }
  check(s[2] > 0.1 && s[6] > 0.1, "and has them");
  spectrum(1, 1, 3, s);
  check(near(s[2], std::fabs(bessel(0, 3) - bessel(2, 3)), 1e-6),
        "1:1 fundamental is J0 with J2 folded back against it");
  check(near(s[4], std::fabs(bessel(1, 3) + bessel(3, 3)), 1e-6),
        "1:1 second harmonic is J1 with J3 folded back onto it");
  spectrum(1, 3.5, 2, s);
  check(s[9] > 0.1 && s[5] > 0.1, "1:3.5 puts partials at 4.5 and, folded, 2.5");
  check(harmonic(1, 2) && harmonic(3, 1) && !harmonic(1, 3.5) && !harmonic(0.5, 1), "harmonic ratios are whole ones");
  check(std::strstr(ratioCharacter(1, 2), "ODD") && std::strstr(ratioCharacter(1, 3.5), "INHARMONIC"), "ratio lines");
  check(near(wave(1, 1, 0, 0.25), 1) && near(wave(2, 1, 0, 0.125), 1), "wave with no modulation is a sine");
  // Energy: the squares of all the sidebands add to one.
  {
    double sum = 0;
    for (int n = -24; n <= 24; ++n) sum += bessel(n < 0 ? -n : n, 6) * bessel(n < 0 ? -n : n, 6);
    check(near(sum, 1, 1e-6), "sideband energy sums to one");
  }

  for (int p = 0; p < kEnvelopePresets; ++p) {
    const Envelope e = envelopePreset(p);
    float x[6], y[6];
    envelopeShape(e, x, y);
    check(near(x[0], 0) && x[5] <= 1.0001f, "envelope spans the width", p);
    for (int i = 1; i < 6; ++i) check(x[i] > x[i - 1], "envelope points move right", p);
    check(near(y[0], e.level[3] / 99.0) && near(y[1], e.level[0] / 99.0) && near(y[3], y[4]) && near(y[5], y[0]),
          "levels in order, held, then back", p);
    check(envelopeName(p)[0] != 0, "named", p);
  }
  {
    Envelope slow, fast;
    slow.rate[0] = 10;
    fast.rate[0] = 99;
    float xs[6], ys[6], xf[6], yf[6];
    envelopeShape(slow, xs, ys);
    envelopeShape(fast, xf, yf);
    check(xs[1] > xf[1] * 3, "a low rate takes visibly longer");
  }

  if (failures == 0) std::printf("fmlab: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
