#pragma once

// TRIPLES: a sliding-number game on a 4x4 board. A swipe moves every tile one
// cell, no further. A 1 and a 2 pushed together make a 3; from 3 up, two of the
// same make their double. Each move lets one new tile in from the edge the
// swipe came from, and the game ends when nothing can move.
//
// Freestanding, with the randomness handed in, so host-tests/triples can play
// whole games and check every rule.

#include <cstdint>

namespace triples {

constexpr int kSide = 4;
constexpr int kCells = kSide * kSide;

enum class Dir : uint8_t { Left = 0, Right = 1, Up = 2, Down = 3 };

struct Rng {
  uint32_t state = 0x2545F491u;
  uint32_t next();
  int below(int n);
};

// The points a tile is worth: nothing for a 1 or a 2, 3 for a 3, and three
// times as much for each doubling after it.
uint32_t tileScore(uint16_t value);
bool merges(uint16_t a, uint16_t b);

class Game {
 public:
  uint16_t cells[kCells] = {};
  uint16_t nextTile = 0;
  Rng rng;

  void start(uint32_t seed);
  uint16_t at(int x, int y) const { return cells[y * kSide + x]; }
  // True when the swipe moved anything; then a new tile has come in and the
  // one after it is in nextTile. `entered` gets the cell it came in at.
  bool move(Dir dir, int* entered = nullptr);
  bool canMove(Dir dir) const;
  bool over() const;
  uint32_t score() const;
  uint16_t highest() const;

  // The stack new tiles are dealt from: four each of 1, 2 and 3, shuffled, and
  // shuffled again when it runs out. Public so a save can keep it.
  uint8_t stack[12] = {};
  uint8_t stackLeft = 0;

 private:
  uint16_t draw();
  // Slides one line a step toward its start. `line` holds four cell indices,
  // leading edge first.
  bool slide(const int line[kSide], bool apply);
};

}  // namespace triples
