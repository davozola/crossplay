#pragma once

// Chess opponent: alpha-beta search over ChessCore.
//
// Freestanding like ChessCore, so the whole thing is host-testable. No heap, no
// clock, no RTOS. Search is bounded by a node budget rather than a wall-clock
// deadline: node counts are deterministic, so a test can assert on them and the
// same budget behaves identically on the C3 and the S3.

#include <cstdint>

#include "ChessCore.h"

namespace chess {

// Scores are centipawns from the side-to-move's point of view.
constexpr int kMateScore = 30000;
// Beyond any plausible material score, so "mate in n" always beats material.
constexpr int kMateThreshold = kMateScore - 1000;

// Each ply costs a ~1KB MoveList in SearchBuffers, and this move generator is
// slow enough that depth 4 is already a second or more on a 160MHz C3, so
// there is nothing to gain from a deeper cap.
constexpr int kMaxSearchDepth = 4;

// One MoveList per ply, ~1KB each. Far too big for a stack frame on the C3, so
// the caller owns it: the activity holds one as a member. Handing the search a
// pre-allocated buffer is also what keeps the engine allocation-free.
// think() goes deeper and follows captures past its depth, so it needs more
// plies than search() does.
constexpr int kThinkDepth = 6;
constexpr int kThinkPlies = 22;
struct SearchBuffers {
  MoveList plies[kThinkPlies];
};

struct SearchResult {
  Move best{};
  int score = 0;
  uint32_t nodes = 0;
  // True when the node budget stopped the search early. The move is still
  // legal and still the best found so far, just less considered.
  bool budgetExhausted = false;
  bool hasMove = false;
};

// Static evaluation in centipawns, positive when the side to move is better.
int evaluate(const Position& position);

// Searches to `depth` and returns the best move for the side to move.
// `nodeBudget` caps the work; pass 0 for no cap. Returns hasMove == false only
// when the position has no legal moves (checkmate or stalemate).
SearchResult search(Position& position, int depth, SearchBuffers& buffers, uint32_t nodeBudget = 0);

// The stronger opponent. Deepens a ply at a time to `maxDepth` or until
// `nodeBudget` positions have been looked at, and answers with the last depth
// it finished, so a budget that runs out never returns a half-considered move.
// At the end of a line it goes on through the captures until the position is
// quiet, which is what stops it taking a defended piece with its queen because
// the recapture was one move past where it stopped looking. `depthReached`,
// when given, is the last depth finished.
SearchResult think(Position& position, int maxDepth, SearchBuffers& buffers, uint32_t nodeBudget,
                   int* depthReached = nullptr);

}  // namespace chess
