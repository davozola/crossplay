#pragma once

// RPN 12C's model: a four-level RPN stack, ten storage registers, and the
// financial keys of the classic business calculator: the five time-value-of-
// money registers, each solved from the other four, and cash flows with NPV
// and IRR. Freestanding, so host-tests/rpn can press keys on a laptop.

#include <cstddef>
#include <cstdint>

namespace rpn {

// The keys, in the order they sit on the panel: five to a row.
enum Key : uint8_t {
  KeyN,
  KeyI,
  KeyPV,
  KeyPMT,
  KeyFV,
  KeyPow,
  KeyInv,
  KeyPctT,
  KeyDPct,
  KeyPct,
  KeyF,
  KeyG,
  KeyRoll,
  KeySwap,
  KeyClx,
  KeySto,
  KeyRcl,
  KeyEex,
  KeyChs,
  KeyDiv,
  KeySqrt,
  Key7,
  Key8,
  Key9,
  KeyMul,
  KeyLn,
  Key4,
  Key5,
  Key6,
  KeySub,
  KeyExp,
  Key1,
  Key2,
  Key3,
  KeyAdd,
  KeyEnter,
  KeyEnter2,
  Key0,
  KeyDot,
  KeyLastX,
  KeyCount,
};
constexpr int kColumns = 5;
constexpr int kRows = 8;

const char* keyLabel(Key k);   // what the key does unshifted
const char* keyLabelF(Key k);  // after f, or ""
const char* keyLabelG(Key k);  // after g, or ""

// ---- the arithmetic the keys rest on ----

struct Tvm {
  double n = 0, i = 0, pv = 0, pmt = 0, fv = 0;  // i in percent a period
  bool begin = false;
};
// Each solves one register from the other four and returns false when there
// is no answer. solveN rounds up to a whole period, as the calculator does.
bool solveN(Tvm& t);
bool solveI(Tvm& t);
bool solvePV(Tvm& t);
bool solvePMT(Tvm& t);
bool solveFV(Tvm& t);

constexpr int kMaxFlows = 20;
struct CashFlows {
  double amount[kMaxFlows + 1] = {};  // [0] is the initial flow
  uint8_t times[kMaxFlows + 1] = {};
  uint8_t count = 0;  // groups after the initial one
};
double npv(const CashFlows& c, double ratePercent);
bool irr(const CashFlows& c, double& ratePercent);

// `x` in fixed notation with `decimals` places and thousands separators, or
// in scientific when it will not fit ten digits.
void format(double x, int decimals, char* out, size_t cap);

class Calculator {
 public:
  void press(Key k);

  // The display line.
  void display(char* out, size_t cap) const;
  double x() const { return s_.stack[0]; }
  double level(const int i) const { return s_.stack[i]; }
  double fin(const int i) const { return (&s_.tvm.n)[i]; }
  double reg(const int i) const { return s_.reg[i]; }
  int decimals() const { return s_.decimals; }
  bool begin() const { return s_.tvm.begin; }
  int shift() const { return shift_; }  // 0, 1 after f, 2 after g
  bool storing() const { return pending_ == 1; }
  bool recalling() const { return pending_ == 2; }
  int error() const { return error_; }
  int flows() const { return s_.flows.count; }

  // What lasts between sessions.
  static constexpr size_t kSaveBytes = 512;
  size_t save(uint8_t* out, size_t cap) const;
  bool restore(const uint8_t* in, size_t size);

 private:
  struct State {
    double stack[4] = {};
    double lastX = 0;
    double reg[10] = {};
    Tvm tvm;
    CashFlows flows;
    uint8_t decimals = 2;
  };

  void digit(char c);
  void endEntry();
  void lift();
  void result(double v, bool binary);
  void finKey(int which);
  void fail(int code);
  double entryValue() const;

  State s_;
  char mantissa_[14] = {};  // digits and at most one point
  bool negative_ = false;
  bool hasExponent_ = false;
  int exponent_ = 0;
  bool exponentNegative_ = false;
  bool entering_ = false;
  bool liftNext_ = false;
  bool finComputes_ = false;  // the last key was a financial one
  uint8_t shift_ = 0;
  uint8_t pending_ = 0;   // 1 STO, 2 RCL
  uint8_t error_ = 0xFF;  // 0xFF: none
};

}  // namespace rpn
