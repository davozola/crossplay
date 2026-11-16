#include "RpnCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rpn {

namespace {

constexpr const char* kMain[KeyCount] = {
    "N",   "I",   "PV",  "PMT", "FV",  "Y^X", "1/X",  "%T",    "D%",    "%", "F", "G",    "RDN", "X<>Y",
    "CLX", "STO", "RCL", "EEX", "CHS", "/",   "SQRT", "7",     "8",     "9", "X", "LN",   "4",   "5",
    "6",   "-",   "E^X", "1",   "2",   "3",   "+",    "ENTER", "ENTER", "0", ".", "LSTX",
};
constexpr const char* kF[KeyCount] = {
    "", "", "NPV", "RND", "IRR", "", "", "", "", "", "", "", "", "FIN", "REG", "", "", "", "", "",
    "", "", "",    "",    "",    "", "", "", "", "", "", "", "", "",    "",    "", "", "", "", "",
};
constexpr const char* kG[KeyCount] = {
    "12X", "12/", "CF0", "CFJ", "NJ", "",    "", "", "", "", "", "", "", "",   "", "", "", "", "", "",
    "X^2", "BEG", "END", "",    "",   "LOG", "", "", "", "", "", "", "", "N!", "", "", "", "", "", "",
};

int digitOf(const Key k) {
  switch (k) {
    case Key0:
      return 0;
    case Key1:
      return 1;
    case Key2:
      return 2;
    case Key3:
      return 3;
    case Key4:
      return 4;
    case Key5:
      return 5;
    case Key6:
      return 6;
    case Key7:
      return 7;
    case Key8:
      return 8;
    case Key9:
      return 9;
    default:
      return -1;
  }
}

// The time-value equation's left side; zero when the five registers agree.
double residual(const double n, const double r, const double pv, const double pmt, const double fv, const bool begin) {
  if (std::fabs(r) < 1e-12) return pv + pmt * n + fv;
  const double v = std::pow(1 + r, -n);
  return pv + pmt * (begin ? 1 + r : 1) * (1 - v) / r + fv * v;
}

// A root of f between the first pair of grid points where it changes sign,
// looking through the ordinary positive rates before the unusual ones.
template <typename F>
bool root(F f, double& out) {
  static const double kGrid[] = {0, 1e-5, 1e-4, 1e-3,  0.005, 0.01,  0.02,  0.05, 0.1,   0.2,  0.35,  0.5,  1,    2,
                                 5, 10,   100,  -1e-5, -1e-3, -0.01, -0.05, -0.1, -0.25, -0.5, -0.75, -0.9, -0.99};
  constexpr int kPoints = sizeof(kGrid) / sizeof(kGrid[0]);
  for (int g = 0; g + 1 < kPoints; ++g) {
    if (g == 16) continue;  // the positive run ends there; the negative one starts from zero
    double a = g == 17 ? 0 : kGrid[g];
    double b = g == 17 ? kGrid[17] : kGrid[g + 1];
    if (g > 17) a = kGrid[g];
    double fa = f(a);
    const double fb = f(b);
    if (!std::isfinite(fa) || !std::isfinite(fb)) continue;
    if (fa == 0) {
      out = a;
      return true;
    }
    if ((fa < 0) == (fb < 0) && fb != 0) continue;
    for (int it = 0; it < 200; ++it) {
      const double m = (a + b) / 2;
      const double fm = f(m);
      if ((fm < 0) == (fa < 0)) {
        a = m;
        fa = fm;
      } else {
        b = m;
      }
    }
    out = (a + b) / 2;
    return true;
  }
  return false;
}

}  // namespace

const char* keyLabel(const Key k) { return k < KeyCount ? kMain[k] : ""; }
const char* keyLabelF(const Key k) { return k < KeyCount ? kF[k] : ""; }
const char* keyLabelG(const Key k) { return k < KeyCount ? kG[k] : ""; }

bool solveN(Tvm& t) {
  const double r = t.i / 100;
  double n = 0;
  if (r <= -1) return false;
  if (std::fabs(r) < 1e-12) {
    if (t.pmt == 0) return false;
    n = -(t.pv + t.fv) / t.pmt;
  } else {
    const double c = t.pmt * (t.begin ? 1 + r : 1) / r;
    if (c - t.fv == 0) return false;
    const double v = (t.pv + c) / (c - t.fv);
    if (!(v > 0)) return false;
    n = -std::log(v) / std::log(1 + r);
  }
  if (!std::isfinite(n) || n < 0) return false;
  const double whole = std::floor(n);
  t.n = n - whole > 0.005 ? whole + 1 : whole;
  return true;
}

bool solveI(Tvm& t) {
  if (t.n <= 0) return false;
  double r = 0;
  const Tvm c = t;
  if (!root([&c](const double rate) { return residual(c.n, rate, c.pv, c.pmt, c.fv, c.begin); }, r)) return false;
  t.i = r * 100;
  return true;
}

bool solvePV(Tvm& t) {
  const double r = t.i / 100;
  if (r <= -1) return false;
  t.pv = -residual(t.n, r, 0, t.pmt, t.fv, t.begin);
  return std::isfinite(t.pv);
}

bool solvePMT(Tvm& t) {
  const double r = t.i / 100;
  if (r <= -1) return false;
  const double unit = residual(t.n, r, 0, 1, 0, t.begin);
  if (unit == 0) return false;
  t.pmt = -residual(t.n, r, t.pv, 0, t.fv, t.begin) / unit;
  return std::isfinite(t.pmt);
}

bool solveFV(Tvm& t) {
  const double r = t.i / 100;
  if (r <= -1) return false;
  const double unit = residual(t.n, r, 0, 0, 1, t.begin);
  if (unit == 0) return false;
  t.fv = -residual(t.n, r, t.pv, t.pmt, 0, t.begin) / unit;
  return std::isfinite(t.fv);
}

double npv(const CashFlows& c, const double ratePercent) {
  const double r = ratePercent / 100;
  double total = c.amount[0];
  int period = 0;
  for (int g = 1; g <= c.count; ++g) {
    for (int k = 0; k < c.times[g]; ++k) {
      ++period;
      total += c.amount[g] / std::pow(1 + r, period);
    }
  }
  return total;
}

bool irr(const CashFlows& c, double& ratePercent) {
  if (c.count == 0) return false;
  double r = 0;
  if (!root([&c](const double rate) { return npv(c, rate * 100); }, r)) return false;
  ratePercent = r * 100;
  return true;
}

void format(const double x, int decimals, char* out, const size_t cap) {
  if (cap == 0) return;
  if (decimals < 0) decimals = 0;
  if (decimals > 9) decimals = 9;
  if (!std::isfinite(x)) {
    std::snprintf(out, cap, "ERROR");
    return;
  }
  const double mag = std::fabs(x);
  char raw[40];
  // Ten digits is all the display holds: give up places before giving up the number.
  int places = decimals;
  if (mag >= 1) {
    const int whole = static_cast<int>(std::floor(std::log10(mag))) + 1;
    if (whole + places > 10) places = 10 - whole;
  }
  const bool tiny = mag != 0 && mag < 0.5 * std::pow(10.0, -decimals);
  if (mag >= 1e10 || places < 0 || tiny) {
    std::snprintf(raw, sizeof(raw), "%.6E", x);
    // "1.234567E+12" reads better on the panel as "1.234567 E12".
    char* e = std::strchr(raw, 'E');
    if (e == nullptr) {
      std::snprintf(out, cap, "%s", raw);
      return;
    }
    *e = 0;
    const int exponent = std::atoi(e + 1);
    std::snprintf(out, cap, "%s E%d", raw, exponent);
    return;
  }
  std::snprintf(raw, sizeof(raw), "%.*f", places, mag);
  const char* point = std::strchr(raw, '.');
  const size_t whole = point ? static_cast<size_t>(point - raw) : std::strlen(raw);
  size_t at = 0;
  // A value that rounds to zero carries no sign.
  bool zero = true;
  for (const char* c = raw; *c; ++c) zero = zero && (*c == '0' || *c == '.');
  if (x < 0 && !zero && at + 1 < cap) out[at++] = '-';
  for (size_t i = 0; i < whole && at + 2 < cap; ++i) {
    out[at++] = raw[i];
    if ((whole - 1 - i) % 3 == 0 && i + 1 < whole) out[at++] = ',';
  }
  for (const char* c = raw + whole; *c && at + 1 < cap; ++c) out[at++] = *c;
  out[at] = 0;
}

double Calculator::entryValue() const {
  double v = std::strtod(mantissa_[0] ? mantissa_ : "0", nullptr);
  if (negative_) v = -v;
  if (hasExponent_) v *= std::pow(10.0, exponentNegative_ ? -exponent_ : exponent_);
  return v;
}

void Calculator::lift() {
  s_.stack[3] = s_.stack[2];
  s_.stack[2] = s_.stack[1];
  s_.stack[1] = s_.stack[0];
}

void Calculator::endEntry() {
  if (!entering_) return;
  s_.stack[0] = entryValue();
  entering_ = false;
}

void Calculator::fail(const int code) { error_ = static_cast<uint8_t>(code); }

// Puts an answer in X. A two-number operation uses up Y and drops the stack.
void Calculator::result(const double v, const bool binary) {
  if (!std::isfinite(v)) {
    fail(0);
    return;
  }
  s_.lastX = s_.stack[0];
  if (binary) {
    s_.stack[1] = s_.stack[2];
    s_.stack[2] = s_.stack[3];
  }
  s_.stack[0] = v;
  liftNext_ = true;
}

void Calculator::digit(const char c) {
  if (!entering_) {
    if (liftNext_) lift();
    entering_ = true;
    mantissa_[0] = 0;
    negative_ = false;
    hasExponent_ = false;
    exponent_ = 0;
    exponentNegative_ = false;
  }
  if (hasExponent_) {
    if (c != '.') exponent_ = (exponent_ % 10) * 10 + (c - '0');
  } else {
    const size_t len = std::strlen(mantissa_);
    size_t digits = 0;
    for (size_t i = 0; i < len; ++i) digits += mantissa_[i] != '.';
    const bool point = std::strchr(mantissa_, '.') != nullptr;
    if (c == '.') {
      if (!point && len + 1 < sizeof(mantissa_)) {
        if (len == 0)
          std::snprintf(mantissa_, sizeof(mantissa_), "0.");
        else {
          mantissa_[len] = '.';
          mantissa_[len + 1] = 0;
        }
      }
    } else if (digits < 10 && len + 1 < sizeof(mantissa_)) {
      mantissa_[len] = c;
      mantissa_[len + 1] = 0;
    }
  }
  s_.stack[0] = entryValue();
  liftNext_ = true;
}

void Calculator::finKey(const int which) {
  double* regs = &s_.tvm.n;
  if (!finComputes_) {
    endEntry();
    regs[which] = s_.stack[0];
    liftNext_ = true;
    finComputes_ = true;
    return;
  }
  Tvm t = s_.tvm;
  bool ok = false;
  switch (which) {
    case 0:
      ok = solveN(t);
      break;
    case 1:
      ok = solveI(t);
      break;
    case 2:
      ok = solvePV(t);
      break;
    case 3:
      ok = solvePMT(t);
      break;
    default:
      ok = solveFV(t);
      break;
  }
  if (!ok) {
    fail(5);
    return;
  }
  s_.tvm = t;
  if (liftNext_) lift();
  s_.stack[0] = regs[which];
  liftNext_ = true;
  finComputes_ = true;
}

void Calculator::press(const Key k) {
  if (k >= KeyCount) return;
  if (error_ != 0xFF) {  // any key clears an error and does nothing else
    error_ = 0xFF;
    shift_ = 0;
    pending_ = 0;
    return;
  }
  const int d = digitOf(k);
  const bool wasFin = finComputes_;
  finComputes_ = false;

  if (pending_ != 0) {
    const uint8_t what = pending_;
    pending_ = 0;
    if (d >= 0) {
      endEntry();
      if (what == 1) {
        s_.reg[d] = s_.stack[0];
      } else {
        if (liftNext_) lift();
        s_.stack[0] = s_.reg[d];
      }
      liftNext_ = true;
      return;
    }
    if (k <= KeyFV) {
      endEntry();
      if (what == 1) {
        (&s_.tvm.n)[k] = s_.stack[0];
      } else {
        if (liftNext_) lift();
        s_.stack[0] = (&s_.tvm.n)[k];
      }
      liftNext_ = true;
      return;
    }
    // Anything else abandons the STO or RCL and is taken as itself.
  }

  if (k == KeyF || k == KeyG) {
    const uint8_t want = k == KeyF ? 1 : 2;
    shift_ = shift_ == want ? 0 : want;
    finComputes_ = wasFin;
    return;
  }

  const uint8_t shift = shift_;
  shift_ = 0;

  if (shift == 1) {
    if (d >= 0) {
      endEntry();
      s_.decimals = static_cast<uint8_t>(d);
      return;
    }
    endEntry();
    switch (k) {
      case KeyPV: {
        const double v = npv(s_.flows, s_.tvm.i);
        if (liftNext_) lift();
        s_.stack[0] = v;
        liftNext_ = true;
        if (!std::isfinite(v)) fail(0);
        return;
      }
      case KeyFV: {
        double rate = 0;
        if (!irr(s_.flows, rate)) {
          fail(3);
          return;
        }
        s_.tvm.i = rate;
        if (liftNext_) lift();
        s_.stack[0] = rate;
        liftNext_ = true;
        return;
      }
      case KeyPMT: {  // round X to what the display shows
        const double scale = std::pow(10.0, s_.decimals);
        result(std::round(s_.stack[0] * scale) / scale, false);
        return;
      }
      case KeySwap:
        s_.tvm = Tvm{};
        return;
      case KeyClx: {
        const uint8_t keep = s_.decimals;
        s_ = State{};
        s_.decimals = keep;
        liftNext_ = false;
        return;
      }
      default:
        return;
    }
  }

  if (shift == 2) {
    endEntry();
    const double x = s_.stack[0];
    switch (k) {
      case KeyN:
        s_.tvm.n = x * 12;
        s_.stack[0] = s_.tvm.n;
        liftNext_ = true;
        finComputes_ = true;
        return;
      case KeyI:
        s_.tvm.i = x / 12;
        s_.stack[0] = s_.tvm.i;
        liftNext_ = true;
        finComputes_ = true;
        return;
      case KeyPV:
        s_.flows = CashFlows{};
        s_.flows.amount[0] = x;
        s_.flows.times[0] = 1;
        s_.tvm.n = 0;
        liftNext_ = true;
        return;
      case KeyPMT:
        if (s_.flows.count >= kMaxFlows) {
          fail(6);
          return;
        }
        ++s_.flows.count;
        s_.flows.amount[s_.flows.count] = x;
        s_.flows.times[s_.flows.count] = 1;
        s_.tvm.n = s_.flows.count;
        liftNext_ = true;
        return;
      case KeyFV:
        if (!(x >= 1 && x <= 99) || x != std::floor(x)) {
          fail(6);
          return;
        }
        s_.flows.times[s_.flows.count] = static_cast<uint8_t>(x);
        liftNext_ = true;
        return;
      case Key7:
        s_.tvm.begin = true;
        return;
      case Key8:
        s_.tvm.begin = false;
        return;
      case KeySqrt:
        result(x * x, false);
        return;
      case KeyLn:
        if (x <= 0)
          fail(0);
        else
          result(std::log10(x), false);
        return;
      case Key3: {
        if (x < 0 || x > 69 || x != std::floor(x)) {
          fail(0);
          return;
        }
        double f = 1;
        for (int i = 2; i <= static_cast<int>(x); ++i) f *= i;
        result(f, false);
        return;
      }
      default:
        return;
    }
  }

  if (d >= 0) {
    digit(static_cast<char>('0' + d));
    return;
  }
  const double x = entering_ ? entryValue() : s_.stack[0];
  const double y = s_.stack[1];
  switch (k) {
    case KeyDot:
      digit('.');
      return;
    case KeyEex:
      if (!entering_) digit('1');
      hasExponent_ = true;
      s_.stack[0] = entryValue();
      return;
    case KeyChs:
      if (entering_) {
        if (hasExponent_)
          exponentNegative_ = !exponentNegative_;
        else
          negative_ = !negative_;
        s_.stack[0] = entryValue();
      } else {
        s_.stack[0] = -s_.stack[0];
      }
      return;
    case KeyEnter:
    case KeyEnter2:
      endEntry();
      lift();
      liftNext_ = false;
      return;
    case KeyClx:
      entering_ = false;
      s_.stack[0] = 0;
      liftNext_ = false;
      return;
    case KeyN:
    case KeyI:
    case KeyPV:
    case KeyPMT:
    case KeyFV:
      finComputes_ = wasFin;
      finKey(k);
      return;
    case KeySto:
      endEntry();
      pending_ = 1;
      return;
    case KeyRcl:
      endEntry();
      pending_ = 2;
      return;
    default:
      break;
  }
  endEntry();
  switch (k) {
    case KeyAdd:
      result(y + x, true);
      break;
    case KeySub:
      result(y - x, true);
      break;
    case KeyMul:
      result(y * x, true);
      break;
    case KeyDiv:
      if (x == 0)
        fail(0);
      else
        result(y / x, true);
      break;
    case KeyPow:
      result(std::pow(y, x), true);
      break;
    case KeyInv:
      if (x == 0)
        fail(0);
      else
        result(1 / x, false);
      break;
    case KeySqrt:
      if (x < 0)
        fail(0);
      else
        result(std::sqrt(x), false);
      break;
    case KeyLn:
      if (x <= 0)
        fail(0);
      else
        result(std::log(x), false);
      break;
    case KeyExp:
      result(std::exp(x), false);
      break;
    // The three percentages leave the base in Y.
    case KeyPct:
      result(y * x / 100, false);
      break;
    case KeyDPct:
      if (y == 0)
        fail(0);
      else
        result((x - y) / y * 100, false);
      break;
    case KeyPctT:
      if (y == 0)
        fail(0);
      else
        result(x / y * 100, false);
      break;
    case KeySwap:
      s_.stack[0] = y;
      s_.stack[1] = x;
      liftNext_ = true;
      break;
    case KeyRoll: {
      const double top = s_.stack[0];
      s_.stack[0] = s_.stack[1];
      s_.stack[1] = s_.stack[2];
      s_.stack[2] = s_.stack[3];
      s_.stack[3] = top;
      liftNext_ = true;
      break;
    }
    case KeyLastX:
      if (liftNext_) lift();
      s_.stack[0] = s_.lastX;
      liftNext_ = true;
      break;
    default:
      break;
  }
}

void Calculator::display(char* out, const size_t cap) const {
  if (cap == 0) return;
  if (error_ != 0xFF) {
    std::snprintf(out, cap, "ERROR %u", static_cast<unsigned>(error_));
    return;
  }
  if (!entering_) {
    format(s_.stack[0], s_.decimals, out, cap);
    return;
  }
  // What has been keyed, as keyed, with the thousands marked.
  const char* point = std::strchr(mantissa_, '.');
  const size_t whole = point ? static_cast<size_t>(point - mantissa_) : std::strlen(mantissa_);
  size_t at = 0;
  if (negative_ && at + 1 < cap) out[at++] = '-';
  if (whole == 0 && at + 1 < cap) out[at++] = '0';
  for (size_t i = 0; i < whole && at + 2 < cap; ++i) {
    out[at++] = mantissa_[i];
    if ((whole - 1 - i) % 3 == 0 && i + 1 < whole) out[at++] = ',';
  }
  for (const char* c = mantissa_ + whole; *c && at + 1 < cap; ++c) out[at++] = *c;
  out[at] = 0;
  if (hasExponent_) std::snprintf(out + at, cap - at, " E%s%02d", exponentNegative_ ? "-" : "", exponent_);
}

size_t Calculator::save(uint8_t* out, const size_t cap) const {
  if (cap < sizeof(State) + 2) return 0;
  State s = s_;
  if (entering_) s.stack[0] = entryValue();
  out[0] = 1;
  out[1] = static_cast<uint8_t>(sizeof(State) & 0xFF);
  std::memcpy(out + 2, &s, sizeof(State));
  return sizeof(State) + 2;
}

bool Calculator::restore(const uint8_t* in, const size_t size) {
  if (size != sizeof(State) + 2 || in[0] != 1 || in[1] != static_cast<uint8_t>(sizeof(State) & 0xFF)) return false;
  State s;
  std::memcpy(&s, in + 2, sizeof(State));
  if (s.decimals > 9 || s.flows.count > kMaxFlows) return false;
  for (const double v : s.stack) {
    if (!std::isfinite(v)) return false;
  }
  s_ = s;
  entering_ = false;
  liftNext_ = true;
  return true;
}

}  // namespace rpn
