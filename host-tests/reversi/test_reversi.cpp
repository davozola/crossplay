// REVERSI host tests: the opening, turning stones over, passing, and whole
// games between the three levels of the opponent.

#include <cstdio>

#include "ReversiCore.h"

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
  using namespace reversi;
  Board b;
  start(b, 8);
  check(b.count(Black) == 2 && b.count(White) == 2 && b.turn == Black, "the opening");
  int legalCount = 0;
  for (int i = 0; i < 64; ++i) legalCount += legal(b, i, Black);
  check(legalCount == 4, "black has four first moves");
  check(!legal(b, 0, Black) && !legal(b, 27, Black), "a corner and a taken square are not among them");
  // d3 (x=3,y=2) turns d4.
  check(play(b, 2 * 8 + 3) == Played::Yes, "d3 is played");
  check(b.cell[3 * 8 + 3] == Black && b.count(Black) == 4 && b.count(White) == 1 && b.turn == White, "and turns d4");
  check(play(b, 0) == Played::No && b.turn == White, "a move that turns nothing is refused");

  // A pass: white holds one stone that black is about to take.
  {
    Board p;
    p.side = 8;
    p.cell[0] = Black;
    p.cell[1] = White;
    p.turn = Black;
    check(play(p, 2) == Played::YesAndOver && p.count(White) == 0, "taking the last stone ends the game");
    Board q;
    q.side = 8;
    q.cell[0] = Black;
    q.cell[1] = White;
    q.cell[16] = White;
    q.cell[8] = White;  // black can still take along the column after the row
    q.turn = Black;
    const Played r = play(q, 2);
    check(r == Played::YesAndOtherPasses && q.turn == Black, "the other side passes when it cannot move");
  }

  // Whole games: they end, the board adds up, and thinking beats guessing.
  Rng rng;
  int strongWins = 0;
  int games = 0;
  for (int side = 6; side <= 8; side += 2) {
    for (int n = 0; n < 12; ++n) {
      Board g;
      start(g, side);
      const uint8_t strong = n % 2 ? Black : White;
      int guard = 0;
      while (!over(g) && guard++ < 200) {
        const int at = choose(g, g.turn == strong ? 2 : 0, rng);
        check(at >= 0, "the side to move always has a move");
        if (at < 0) break;
        check(play(g, at) != Played::No, "the device only plays allowed squares");
      }
      check(over(g), "a game ends");
      check(g.count(Black) + g.count(White) <= side * side, "the stones fit the board");
      strongWins += g.count(strong) > g.count(other(strong));
      ++games;
    }
  }
  check(strongWins * 10 >= games * 8, "the top level beats the bottom one at least four games in five");
  std::printf("reversi: level 2 won %d of %d against level 0\n", strongWins, games);

  // The expert against the hard one, both colours, both boards.
  int expertWins = 0;
  int expertLosses = 0;
  for (int side = 6; side <= 8; side += 2) {
    for (int n = 0; n < 8; ++n) {
      Board g;
      start(g, side);
      const uint8_t expert = n % 2 ? Black : White;
      int guard = 0;
      while (!over(g) && guard++ < 200) {
        const int at = choose(g, g.turn == expert ? 3 : 2, rng);
        if (at < 0 || play(g, at) == Played::No) break;
      }
      expertWins += g.count(expert) > g.count(other(expert));
      expertLosses += g.count(expert) < g.count(other(expert));
    }
  }
  std::printf("reversi: level 3 won %d and lost %d of 16 against level 2\n", expertWins, expertLosses);
  check(expertWins > expertLosses, "the expert beats the hard level more often than not");

  if (failures == 0) std::printf("reversi: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
