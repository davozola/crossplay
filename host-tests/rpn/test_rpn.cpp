// RPN 12C host tests: keys pressed in the order a person presses them, checked
// against answers known from the calculator's own handbook arithmetic.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "RpnCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    if (failures < 30) std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
bool near(double a, double b, double eps = 0.005) {
  if (std::fabs(a - b) >= eps) std::printf("  got %.6f, want %.6f\n", a, b);
  return std::fabs(a - b) < eps;
}
using namespace rpn;

void type(Calculator& c, const char* number) {
  for (const char* p = number; *p; ++p) {
    static const Key kDigits[10] = {Key0, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9};
    if (*p == '.')
      c.press(KeyDot);
    else if (*p == '-')
      c.press(KeyChs);
    else
      c.press(kDigits[*p - '0']);
  }
}
void keys(Calculator& c, std::initializer_list<Key> ks) {
  for (const Key k : ks) c.press(k);
}
bool shows(const Calculator& c, const char* want) {
  char out[40];
  c.display(out, sizeof(out));
  if (std::strcmp(out, want) != 0) std::printf("  shows '%s', want '%s'\n", out, want);
  return std::strcmp(out, want) == 0;
}
}  // namespace

int main() {
  for (int k = 0; k < KeyCount; ++k) check(keyLabel(static_cast<Key>(k))[0] != 0, "every key is labelled");
  check(KeyCount == kColumns * kRows, "the keys fill the panel");

  {  // the stack
    Calculator c;
    type(c, "2");
    c.press(KeyEnter);
    type(c, "3");
    c.press(KeyAdd);
    check(near(c.x(), 5) && shows(c, "5.00"), "2 ENTER 3 +");
    type(c, "4");
    c.press(KeyMul);
    check(near(c.x(), 20), "an answer lifts when a number follows it");
    c.press(KeyLastX);
    check(near(c.x(), 4) && near(c.level(1), 20), "LSTX");
    Calculator d;
    type(d, "1");
    d.press(KeyEnter);
    type(d, "2");
    d.press(KeyEnter);
    type(d, "3");
    d.press(KeyEnter);
    type(d, "4");
    check(near(d.level(3), 1) && near(d.level(2), 2) && near(d.level(1), 3) && near(d.x(), 4), "four levels");
    d.press(KeyAdd);
    check(near(d.x(), 7) && near(d.level(1), 2) && near(d.level(2), 1) && near(d.level(3), 1),
          "T repeats as the stack drops");
    d.press(KeyRoll);
    check(near(d.x(), 2) && near(d.level(3), 7), "roll down");
    d.press(KeySwap);
    check(near(d.x(), 1) && near(d.level(1), 2), "swap");
    Calculator e;
    type(e, "3");
    e.press(KeyEnter);
    e.press(KeyMul);
    check(near(e.x(), 9), "ENTER then an operation uses the copy");
    type(e, "5");
    e.press(KeyClx);
    type(e, "6");
    e.press(KeyAdd);
    check(near(e.x(), 15), "CLX lets the next number replace it");
  }
  {  // entry and the display
    Calculator c;
    type(c, "1234567.5");
    check(shows(c, "1,234,567.5"), "entry shows what was keyed");
    c.press(KeyChs);
    check(shows(c, "-1,234,567.5"), "CHS while keying");
    c.press(KeyEnter);
    check(shows(c, "-1,234,567.50"), "then the fixed format");
    keys(c, {KeyF, Key4});
    check(shows(c, "-1,234,567.500") && c.decimals() == 4, "f 4, as far as ten digits go");
    keys(c, {KeyF, Key0});
    check(shows(c, "-1,234,568"), "f 0 rounds");
    Calculator d;
    type(d, "1.5");
    d.press(KeyEex);
    type(d, "12");
    check(shows(d, "1.5 E12"), "EEX entry");
    d.press(KeyEnter);
    check(near(d.x() / 1e12, 1.5) && shows(d, "1.500000 E12"), "too big for fixed");
    Calculator e;
    e.press(KeyDot);
    type(e, "5");
    check(shows(e, "0.5"), "a leading point");
    type(e, "0000000000000");
    check(shows(e, "0.500000000"), "ten digits at most");
    char out[40];
    format(0.0001, 2, out, sizeof(out));
    check(std::strcmp(out, "1.000000 E-4") == 0, "too small for fixed");
    format(-0.001, 2, out, sizeof(out));
    check(std::strcmp(out, "-1.000000 E-3") == 0, "small and negative");
    format(123456789.126, 2, out, sizeof(out));
    check(std::strcmp(out, "123,456,789.1") == 0, "places give way to digits");
    format(0, 2, out, sizeof(out));
    check(std::strcmp(out, "0.00") == 0, "zero");
  }
  {  // functions
    Calculator c;
    type(c, "100");
    c.press(KeyEnter);
    type(c, "15");
    c.press(KeyPct);
    check(near(c.x(), 15) && near(c.level(1), 100), "% keeps the base");
    c.press(KeyAdd);
    check(near(c.x(), 115), "and adds onto it");
    type(c, "80");
    c.press(KeyEnter);
    type(c, "100");
    c.press(KeyDPct);
    check(near(c.x(), 25), "percent change");
    type(c, "200");
    c.press(KeyEnter);
    type(c, "50");
    c.press(KeyPctT);
    check(near(c.x(), 25), "percent of total");
    type(c, "2");
    c.press(KeyEnter);
    type(c, "10");
    c.press(KeyPow);
    check(near(c.x(), 1024), "y^x");
    c.press(KeySqrt);
    check(near(c.x(), 32), "sqrt");
    c.press(KeyInv);
    check(near(c.x(), 0.03125, 1e-9), "1/x");
    type(c, "1");
    c.press(KeyExp);
    c.press(KeyLn);
    check(near(c.x(), 1, 1e-9), "e^x and ln");
    type(c, "5");
    keys(c, {KeyG, Key3});
    check(near(c.x(), 120), "n!");
    type(c, "1000");
    keys(c, {KeyG, KeyLn});
    check(near(c.x(), 3, 1e-9), "log");
    type(c, "7");
    keys(c, {KeyG, KeySqrt});
    check(near(c.x(), 49), "x squared");
    type(c, "0");
    c.press(KeyDiv);
    check(c.error() == 0 && shows(c, "ERROR 0"), "dividing by zero");
    c.press(KeyAdd);
    check(c.error() == 0xFF && near(c.x(), 0), "any key clears it and does nothing else");
    type(c, "3.14159");
    keys(c, {KeySto, Key4, KeyClx, KeyRcl, Key4});
    check(near(c.x(), 3.14159, 1e-9) && near(c.reg(4), 3.14159, 1e-9), "STO and RCL");
    keys(c, {KeyF, KeyPMT});
    check(near(c.x(), 3.14, 1e-9), "RND to the display");
  }
  {  // a mortgage: 30 years monthly at 6%, 100,000
    Calculator c;
    type(c, "30");
    keys(c, {KeyG, KeyN});
    type(c, "6");
    keys(c, {KeyG, KeyI});
    type(c, "100000");
    c.press(KeyPV);
    type(c, "0");
    c.press(KeyFV);
    c.press(KeyPMT);
    check(near(c.fin(0), 360) && near(c.fin(1), 0.5) && near(c.x(), -599.55), "the payment");
    check(shows(c, "-599.55"), "shown");
    // Balance after ten years.
    type(c, "120");
    c.press(KeyN);
    c.press(KeyFV);
    check(near(c.x(), -83685.72, 0.02), "the balance after 120 payments");
    keys(c, {KeyRcl, KeyPMT});
    check(near(c.x(), -599.55), "RCL of a financial register");
  }
  {  // each register from the other four
    Tvm t;
    t.n = 10, t.i = 5, t.pv = -1000;
    check(solveFV(t) && near(t.fv, 1628.89), "FV of 1000 at 5% for 10");
    Tvm u;
    u.i = 1, u.pv = -1000, u.fv = 2000;
    check(solveN(u) && near(u.n, 70), "n rounds up: 69.66 becomes 70");
    Tvm v;
    v.n = 360, v.pv = 100000, v.pmt = -599.55;
    check(solveI(v) && near(v.i, 0.5, 1e-4), "i by search");
    Tvm w;
    w.n = 48, w.i = 0.75, w.pmt = -300;
    check(solvePV(w) && near(w.pv, 12055.43, 0.01), "PV of a car loan");
    w.begin = true;
    check(solvePV(w) && near(w.pv, 12145.85, 0.01), "the same paid at the start of each month");
    Tvm z;
    z.n = 12, z.i = 0, z.pv = -1200;
    check(solvePMT(z) && near(z.pmt, 100), "no interest");
    Tvm neg;
    neg.n = 5, neg.pv = -1000, neg.fv = 800;
    check(solveI(neg) && near(neg.i, -4.3648, 1e-3), "a loss is a negative rate");
    Tvm bad;
    bad.n = 10, bad.pv = 100, bad.pmt = 10, bad.fv = 100;
    check(!solveI(bad), "all one sign has no rate");
    Calculator c;
    type(c, "10");
    c.press(KeyN);
    type(c, "100");
    c.press(KeyPV);
    type(c, "10");
    c.press(KeyPMT);
    type(c, "100");
    c.press(KeyFV);
    c.press(KeyI);
    check(c.error() == 5, "and says so");
  }
  {  // cash flows
    Calculator c;
    type(c, "1000");
    keys(c, {KeyChs, KeyG, KeyPV});
    type(c, "500");
    keys(c, {KeyG, KeyPMT});
    type(c, "3");
    keys(c, {KeyG, KeyFV});
    check(c.flows() == 1, "one group of three");
    type(c, "10");
    c.press(KeyI);
    keys(c, {KeyF, KeyPV});
    check(near(c.x(), 243.43), "NPV at 10%");
    keys(c, {KeyF, KeyFV});
    check(near(c.x(), 23.375, 1e-3) && near(c.fin(1), 23.375, 1e-3), "IRR, left in i");
    CashFlows f;
    f.amount[0] = -80000, f.times[0] = 1;
    f.count = 5;
    const double flows[5] = {-500, 4500, 5500, 4500, 130000};
    for (int i = 0; i < 5; ++i) f.amount[i + 1] = flows[i], f.times[i + 1] = 1;
    double rate = 0;
    check(irr(f, rate) && near(rate, 13.06, 0.01) && near(npv(f, rate), 0, 1e-4),
          "a five-year property: the rate that brings NPV to zero");
    check(near(npv(f, 13), 212.18, 0.01), "and its NPV just under that rate");
    CashFlows none;
    check(!irr(none, rate), "no flows, no rate");
  }
  {  // saving
    Calculator c;
    type(c, "42");
    c.press(KeyEnter);
    type(c, "7");
    keys(c, {KeySto, Key1, KeyG, Key7, KeyF, Key3});
    type(c, "9");
    uint8_t blob[Calculator::kSaveBytes];
    const size_t n = c.save(blob, sizeof(blob));
    check(n > 0 && n <= Calculator::kSaveBytes, "fits");
    Calculator d;
    check(d.restore(blob, n) && near(d.x(), 9) && near(d.reg(1), 7) && d.begin() && d.decimals() == 3, "comes back");
    blob[0] = 9;
    check(!d.restore(blob, n) && !d.restore(blob, 3), "refuses what it does not know");
  }

  if (failures == 0) std::printf("rpn: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
