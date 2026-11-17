// DOTS AND BOXES host tests: lines and the squares they close, keeping the
// turn, and whole games between the opponent's levels.

#include <cstdio>
#include <initializer_list>

#include "BoxesCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
}  // namespace

int main() {
  using namespace boxes;
  Board b;
  start(b, 3, 2);
  check(b.lines() == 3 * 3 + 2 * 4, "a 3 by 2 board has 17 lines");
  // Every line borders one or two squares, and every square has four distinct sides.
  for (int line = 0; line < b.lines(); ++line) {
    int touched[2];
    const int n = boxesOf(b, line, touched);
    check(n == 1 || n == 2, "a line borders one or two squares");
    for (int i = 0; i < n; ++i) {
      const int box = touched[i];
      check(line == b.top(box) || line == b.bottom(box) || line == b.left(box) || line == b.right(box),
            "and is one of their sides");
    }
  }
  check(draw(b, b.top(0)) == 0 && b.turn == 2, "a line that closes nothing passes the turn");
  check(draw(b, b.top(0)) == -1 && b.turn == 2, "a drawn line cannot be drawn again");
  draw(b, b.left(0));
  draw(b, b.bottom(0));
  check(b.turn == 2 && b.sides(0) == 3, "three sides, player two to move");
  check(draw(b, b.right(0)) == 1 && b.owner[0] == 2 && b.score[2] == 1 && b.turn == 2,
        "the fourth side wins the square and keeps the turn");
  // One line closing two squares at once.
  {
    Board d;
    start(d, 2, 1);
    for (const int line : {d.top(0), d.bottom(0), d.left(0), d.top(1), d.bottom(1), d.right(1)}) d.line[line] = true;
    check(draw(d, d.right(0)) == 2 && d.score[1] == 2 && d.over(), "the middle line closes both");
  }

  Rng rng;
  int strongWins = 0;
  int games = 0;
  for (int side = 3; side <= 6; ++side) {
    for (int n = 0; n < 10; ++n) {
      Board g;
      start(g, side, side);
      const uint8_t strong = static_cast<uint8_t>(1 + n % 2);
      int guard = 0;
      while (!g.over() && guard++ < 200) {
        const int line = choose(g, g.turn == strong ? 2 : 0, rng);
        check(line >= 0 && !g.line[line], "the device draws a line that is free");
        if (line < 0) break;
        draw(g, line);
      }
      check(g.over() && g.score[1] + g.score[2] == side * side, "a game ends with every square owned");
      strongWins += g.score[strong] > g.score[3 - strong];
      ++games;
    }
  }
  check(strongWins * 10 >= games * 8, "the top level beats the bottom one at least four games in five");
  std::printf("boxes: level 2 won %d of %d against level 0\n", strongWins, games);

  if (failures == 0) std::printf("boxes: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
