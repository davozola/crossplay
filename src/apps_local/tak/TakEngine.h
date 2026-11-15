#pragma once

// The computer opponent: alpha-beta over TakCore with iterative deepening,
// bounded by a node budget rather than a clock so a test can assert on it and
// the device and the laptop agree. The evaluation follows the shape of
// Taktician's (github.com/nelhage/taktician, MIT): flats on top, what sits
// under your stacks, and how far each connected group stretches toward a road.
//
// No heap, no clock, no RTOS: the caller owns the buffers.

#include <cstdint>

#include "TakCore.h"

namespace tak {

constexpr int kWinScore = 1000000;
constexpr int kMaxSearchDepth = 4;

// One MoveList per ply. 32KB each, so the activity allocates this once on
// entry rather than putting it anywhere near a stack.
constexpr int kMaxJitter = 512;
struct SearchBuffers {
  MoveList plies[kMaxSearchDepth + 1];
  // The child position at each ply. Here rather than in the search's stack
  // frames: a Game is ~600 bytes, and the search runs on the loop task.
  Game games[kMaxSearchDepth + 2];
  // Per-root-move noise for the easier levels.
  int jitter[kMaxJitter];
};

enum class Level : uint8_t { Easy = 0, Medium = 1, Hard = 2 };

struct Limits {
  int maxDepth = 3;
  uint32_t nodeBudget = 40000;
  // Up to this much is added to each root move's score, so an easy opponent
  // does not play the same game twice. Zero is deterministic.
  int noise = 0;
  uint32_t seed = 1;
};

Limits limitsFor(Level level, uint32_t seed);

struct SearchResult {
  Move best{};
  bool found = false;
  int score = 0;
  int depth = 0;
  uint32_t nodes = 0;
};

// The static evaluation, from `color`'s point of view.
int evaluate(const Game& game, uint8_t color);

SearchResult search(const Game& game, SearchBuffers& buffers, const Limits& limits);

}  // namespace tak
