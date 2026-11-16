#pragma once

// The opponent: a short look ahead over the real rules, scored mostly by how
// close each queen is to being surrounded. Freestanding.

#include "HiveCore.h"

namespace hive {

struct Rng {
  uint32_t state = 0x6C8E9CF5u;
  uint32_t next();
  int below(int n);
};

constexpr int kLevels = 3;
const char* levelName(int level);

// Move lists for each ply of the search, kept off the stack: the caller
// allocates one of these on the heap for as long as it wants an opponent.
struct SearchBuffers {
  static constexpr int kMaxDepth = 3;
  Move moves[kMaxDepth][kMaxMoves];
};

// The move for the side to move. `nodes`, when given, gets how many positions
// were looked at.
Move chooseMove(const Game& game, int level, Rng& rng, SearchBuffers& buffers, long* nodes = nullptr);

}  // namespace hive
