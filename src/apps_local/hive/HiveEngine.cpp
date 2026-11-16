#include "HiveEngine.h"

namespace hive {

namespace {

constexpr int kWin = 100000;

struct Search {
  SearchBuffers* buffers;
  long nodes = 0;
  long budget = 0;
};

// Good for `color`: the other queen hemmed in, its own free, pieces on the
// table rather than in the hand.
int evaluate(const Game& g, const int color) {
  const int w = g.winner();
  if (w == 2) return 0;
  if (w >= 0) return w == color ? kWin : -kWin;
  int score = 22 * g.queenNeighbours(1 - color) - 26 * g.queenNeighbours(color);
  for (int i = 0; i < kPieces; ++i) {
    if (!g.pieces[i].placed) continue;
    score += g.pieces[i].color == color ? 2 : -2;
  }
  return score;
}

int negamax(Search& s, const Game& g, const int depth, int alpha, const int beta) {
  ++s.nodes;
  if (depth == 0 || g.winner() >= 0 || s.nodes >= s.budget) return evaluate(g, g.turn);
  Move* moves = s.buffers->moves[depth - 1];
  const int n = g.legalMoves(moves, kMaxMoves);
  int best = -kWin * 2;
  for (int i = 0; i < n; ++i) {
    Game next = g;
    next.apply(moves[i]);
    const int score = -negamax(s, next, depth - 1, -beta, -alpha);
    if (score > best) best = score;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  // A win sooner beats a win later.
  if (best > kWin / 2) --best;
  return best;
}

}  // namespace

uint32_t Rng::next() {
  if (state == 0) state = 0x9E3779B9u;
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

int Rng::below(const int n) { return n <= 1 ? 0 : static_cast<int>(next() % static_cast<uint32_t>(n)); }

const char* levelName(const int level) {
  static const char* const kNames[kLevels] = {"EASY", "MEDIUM", "HARD"};
  return kNames[level < 0 ? 0 : (level >= kLevels ? kLevels - 1 : level)];
}

Move chooseMove(const Game& game, const int level, Rng& rng, SearchBuffers& buffers, long* nodes) {
  // Easy looks one move ahead and is easily distracted; Hard looks three,
  // within a budget that keeps the wait to a few seconds on the device.
  static const int kDepth[kLevels] = {1, 2, 3};
  static const int kNoise[kLevels] = {30, 6, 2};
  static const long kBudget[kLevels] = {2000, 3500, 8000};
  const int lv = level < 0 ? 0 : (level >= kLevels ? kLevels - 1 : level);
  Search s;
  s.buffers = &buffers;
  s.budget = kBudget[lv];
  const int depth = kDepth[lv];
  Move* moves = buffers.moves[depth - 1];
  const int n = game.legalMoves(moves, kMaxMoves);
  Move best = moves[0];
  int bestScore = -kWin * 4;
  for (int i = 0; i < n; ++i) {
    Game next = game;
    next.apply(moves[i]);
    int score = depth > 1 ? -negamax(s, next, depth - 1, -kWin * 2, kWin * 2) : -evaluate(next, next.turn);
    ++s.nodes;
    if (score < kWin / 2 && score > -kWin / 2) score += rng.below(kNoise[lv] + 1);
    if (score > bestScore) {
      bestScore = score;
      best = moves[i];
    }
  }
  if (nodes) *nodes = s.nodes;
  return best;
}

}  // namespace hive
