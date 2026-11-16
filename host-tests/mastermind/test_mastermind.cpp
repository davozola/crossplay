#include <cstdio>
#include <cstdlib>

#include "MastermindCore.h"

using namespace mastermind;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

static bool is(const Score s, const int b, const int w) { return s.black == b && s.white == w; }

static void scoring() {
  const uint8_t code[4] = {0, 1, 2, 3};
  const uint8_t g1[4] = {0, 1, 2, 3};
  CHECK(is(score(code, g1, 4), 4, 0));
  const uint8_t g2[4] = {3, 2, 1, 0};
  CHECK(is(score(code, g2, 4), 0, 4));
  const uint8_t g3[4] = {0, 0, 0, 0};
  CHECK(is(score(code, g3, 4), 1, 0));  // one 0 in the code, already black
  const uint8_t g4[4] = {4, 4, 0, 0};
  CHECK(is(score(code, g4, 4), 0, 1));  // two 0s offered, one in the code
  // Repeats in the code.
  const uint8_t rep[4] = {1, 1, 2, 2};
  const uint8_t g5[4] = {2, 2, 1, 1};
  CHECK(is(score(rep, g5, 4), 0, 4));
  const uint8_t g6[4] = {1, 2, 1, 5};
  CHECK(is(score(rep, g6, 4), 1, 2));
  const uint8_t g7[4] = {1, 1, 1, 1};
  CHECK(is(score(rep, g7, 4), 2, 0));
}

static void wholeGame() {
  Game g;
  start(g, Mode::Easy, 1234);
  const Rules r = rulesFor(Mode::Easy);
  // Easy has no repeats in the code.
  for (int i = 0; i < r.pegs; ++i)
    for (int j = i + 1; j < r.pegs; ++j) CHECK(g.code[i] != g.code[j]);
  CHECK(add(g, 0));
  CHECK(!add(g, 0));  // no repeats in a guess either
  CHECK(add(g, 1) && add(g, 2));
  CHECK(!submit(g));  // not full
  CHECK(add(g, 3));
  CHECK(!add(g, 4));  // full
  CHECK(clearSlot(g, 1));
  CHECK(add(g, 5));
  CHECK(g.current[1] == 5);
  CHECK(submit(g));
  CHECK(g.rowsUsed == 1);
  // Play the code itself: won.
  for (int i = 0; i < r.pegs; ++i) add(g, g.code[i]);
  CHECK(submit(g));
  CHECK(g.state == State::Won);
  CHECK(!add(g, 0));

  Game h;
  start(h, Mode::Hard, 99);
  const Rules hr = rulesFor(Mode::Hard);
  int wrong = 0;
  for (int s = 0; s < hr.symbols; ++s) {
    bool inCode = false;
    for (int i = 0; i < hr.pegs; ++i) inCode |= h.code[i] == s;
    if (!inCode) wrong = s;
  }
  for (int row = 0; row < hr.rows; ++row) {
    for (int i = 0; i < hr.pegs; ++i) CHECK(add(h, wrong));
    CHECK(submit(h));
  }
  CHECK(h.state == State::Lost);
  CHECK(h.rowsUsed == hr.rows);
}

static void codesVary() {
  Game a, b;
  start(a, Mode::Classic, 1);
  start(b, Mode::Classic, 2);
  bool differ = false;
  for (int i = 0; i < 4; ++i) differ |= a.code[i] != b.code[i];
  CHECK(differ);
  for (int i = 0; i < 4; ++i) CHECK(a.code[i] < 6);
}

int main() {
  scoring();
  wholeGame();
  codesVary();
  if (failures) {
    std::printf("%d mastermind check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("mastermind: all checks passed\n");
  return EXIT_SUCCESS;
}
