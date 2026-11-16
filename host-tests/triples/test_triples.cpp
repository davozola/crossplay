// TRIPLES host tests: how a line slides, what merges, where the new tile comes
// in, the score, and whole games played to the end.

#include <cstdio>
#include <cstring>

#include "TriplesCore.h"

namespace {
int failures = 0;
void check(const bool ok, const char* what) {
  if (!ok) {
    if (failures < 20) std::printf("FAIL: %s\n", what);
    ++failures;
  }
}
void setRow(triples::Game& g, int y, int a, int b, int c, int d) {
  g.cells[y * 4] = static_cast<uint16_t>(a);
  g.cells[y * 4 + 1] = static_cast<uint16_t>(b);
  g.cells[y * 4 + 2] = static_cast<uint16_t>(c);
  g.cells[y * 4 + 3] = static_cast<uint16_t>(d);
}
bool rowIs(const triples::Game& g, int y, int a, int b, int c, int d) {
  return g.at(0, y) == a && g.at(1, y) == b && g.at(2, y) == c && g.at(3, y) == d;
}
}  // namespace

int main() {
  using namespace triples;

  check(merges(1, 2) && merges(2, 1) && !merges(1, 1) && !merges(2, 2), "1 and 2 make 3, and only that way round");
  check(merges(3, 3) && merges(48, 48) && !merges(3, 6) && !merges(0, 3) && !merges(1, 3), "equals from 3 up merge");
  check(tileScore(1) == 0 && tileScore(2) == 0 && tileScore(3) == 3 && tileScore(6) == 9 && tileScore(48) == 243,
        "a tile is worth three times its half");

  // One row, every other cell empty of interest: the new tile is taken away
  // again so the row can be read.
  auto slideLeft = [](int a, int b, int c, int d, int wa, int wb, int wc, int wd, const char* what) {
    Game g;
    g.rng.state = 5;
    g.nextTile = 1;
    setRow(g, 0, a, b, c, d);
    int entered = -1;
    const bool moved = g.move(Dir::Left, &entered);
    const bool expectMove = a != wa || b != wb || c != wc || d != wd;
    check(moved == expectMove, what);
    if (moved) {
      check(entered == 3, "the new tile comes in at the far end of the row that moved");
      g.cells[entered] = 0;
    }
    check(rowIs(g, 0, wa, wb, wc, wd), what);
  };
  slideLeft(0, 3, 0, 0, 3, 0, 0, 0, "a tile moves one cell, not to the wall... from next to it");
  slideLeft(0, 0, 0, 3, 0, 0, 3, 0, "a tile moves one cell, no further");
  slideLeft(3, 3, 0, 0, 6, 0, 0, 0, "two threes at the wall merge");
  slideLeft(1, 2, 0, 0, 3, 0, 0, 0, "one and two merge");
  slideLeft(1, 1, 0, 0, 1, 1, 0, 0, "two ones do not");
  slideLeft(3, 6, 12, 24, 3, 6, 12, 24, "a full row with no pair does not move");
  slideLeft(3, 3, 3, 3, 6, 3, 3, 0, "only the first pair merges; the rest follow");
  slideLeft(6, 3, 3, 0, 6, 6, 0, 0, "a pair further back merges when the front cannot move");
  slideLeft(0, 1, 2, 3, 1, 2, 3, 0, "a gap at the wall takes the whole row a step");
  slideLeft(3, 0, 3, 0, 3, 3, 0, 0, "a tile moves into a gap without merging across it");

  {
    Game g;
    g.nextTile = 2;
    setRow(g, 1, 0, 0, 0, 3);
    check(!g.canMove(Dir::Right) && g.canMove(Dir::Left) && g.canMove(Dir::Up) && g.canMove(Dir::Down), "canMove");
    int entered = -1;
    check(g.move(Dir::Down, &entered) && g.at(3, 2) == 3 && entered == 3 && g.at(3, 0) == 2,
          "a move down brings the new tile in at the top of the column that moved");
    check(!g.move(Dir::Right), "a swipe that moves nothing is not a move");
  }

  // Whole games: the stack deals four of each, the board never holds a value
  // that cannot be made, and the end is a board that really cannot move.
  for (uint32_t seed = 1; seed <= 200; ++seed) {
    Game g;
    g.start(seed);
    int filled = 0;
    for (int i = 0; i < kCells; ++i) filled += g.cells[i] != 0;
    check(filled == 9 && g.nextTile >= 1 && g.nextTile <= 3, "a game starts with nine tiles and one waiting");
    int moves = 0;
    while (!g.over() && moves < 20000) {
      const Dir d = static_cast<Dir>(g.rng.below(4));
      const uint32_t before = g.score();
      if (g.move(d)) {
        ++moves;
        check(g.score() + 9 >= before, "a move never loses more than the new tile could");
      }
    }
    check(g.over() && moves > 8 && moves < 20000, "a game ends");
    for (int i = 0; i < kCells; ++i) {
      uint16_t v = g.cells[i];
      check(v != 0, "a finished board is full");
      while (v > 3 && v % 2 == 0) v = static_cast<uint16_t>(v / 2);
      check(v >= 1 && v <= 3, "every tile is 1, 2 or 3 doubled");
    }
    check(g.highest() >= 6, "random play still builds something");
  }
  {
    Game g;
    g.start(9);
    int count[4] = {};
    // Whatever was dealt to set the board up, the stack as a whole is fair.
    for (int i = 0; i < 12; ++i) ++count[g.stack[i]];
    check(count[1] == 4 && count[2] == 4 && count[3] == 4, "the stack is four of each");
  }

  if (failures == 0) std::printf("triples: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
