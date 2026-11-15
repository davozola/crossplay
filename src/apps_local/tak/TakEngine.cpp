#include "TakEngine.h"

namespace tak {

namespace {

constexpr int kTopFlat = 400;
constexpr int kTopWall = 220;
constexpr int kTopCap = 320;
// Your pieces buried under your own top are hard to take back; the enemy's
// buried under yours are prisoners.
constexpr int kOwnUnder = 50;
constexpr int kPrisoner = 25;
constexpr int kTempo = 60;

// A connected group of road pieces, by how many more rows (or columns) it
// needs before it is a road. Indexed by that shortfall; zero is a road and
// never reaches the evaluation.
constexpr int kShort[kMaxSize + 1] = {0, 700, 320, 110, 30, 0, 0};

int centre(const int size, const int square) {
  const int r = square / size;
  const int c = square % size;
  const int dr = r * 2 - (size - 1);
  const int dc = c * 2 - (size - 1);
  const int d = (dr < 0 ? -dr : dr) + (dc < 0 ? -dc : dc);
  return (2 * (size - 1) - d) * 6;
}

int groupScore(const Game& game, const uint8_t color) {
  const int size = game.size;
  const int n = size * size;
  uint64_t mask = 0;
  for (int i = 0; i < n; ++i) {
    const Square& s = game.sq[i];
    if (roadPiece(s) && topColor(s) == color) mask |= 1ull << i;
  }
  int total = 0;
  uint64_t seen = 0;
  for (int i = 0; i < n; ++i) {
    const uint64_t bit = 1ull << i;
    if (!(mask & bit) || (seen & bit)) continue;
    // Flood this group.
    uint64_t group = bit;
    for (;;) {
      uint64_t grown = group;
      for (int j = 0; j < n; ++j) {
        if (!(group & (1ull << j))) continue;
        const int r = j / size;
        const int c = j % size;
        if (r > 0) grown |= 1ull << (j - size);
        if (r < size - 1) grown |= 1ull << (j + size);
        if (c > 0) grown |= 1ull << (j - 1);
        if (c < size - 1) grown |= 1ull << (j + 1);
      }
      grown &= mask;
      if (grown == group) break;
      group = grown;
    }
    seen |= group;
    int rows = 0;
    int cols = 0;
    for (int r = 0; r < size; ++r) {
      uint64_t rowBits = 0;
      for (int c = 0; c < size; ++c) rowBits |= 1ull << (r * size + c);
      if (group & rowBits) ++rows;
    }
    for (int c = 0; c < size; ++c) {
      uint64_t colBits = 0;
      for (int r = 0; r < size; ++r) colBits |= 1ull << (r * size + c);
      if (group & colBits) ++cols;
    }
    const int span = rows > cols ? rows : cols;
    total += kShort[size - span];
  }
  return total;
}

int sideScore(const Game& game, const uint8_t color) {
  const int size = game.size;
  const int n = size * size;
  int score = 0;
  for (int i = 0; i < n; ++i) {
    const Square& s = game.sq[i];
    if (empty(s) || topColor(s) != color) continue;
    switch (s.top) {
      case Stone::Flat:
        score += kTopFlat + centre(size, i);
        break;
      case Stone::Wall:
        score += kTopWall;
        break;
      case Stone::Cap:
        score += kTopCap + centre(size, i) * 2;
        break;
    }
    for (int h = 0; h + 1 < s.height; ++h) {
      score += ((s.colors >> h) & 1u) == color ? kOwnUnder : kPrisoner;
    }
  }
  score += groupScore(game, color);
  return score;
}

struct Searcher {
  SearchBuffers& buffers;
  uint32_t budget;
  uint32_t nodes = 0;
  bool stopped = false;

  int terminal(const Game& g, const uint8_t me, const int ply) const {
    const uint8_t w = winner(g.result);
    if (w == 0xFF) return 0;
    // Sooner wins score higher, later losses less badly.
    return w == me ? kWinScore - ply : -(kWinScore - ply);
  }

  // Cheap ordering key: winning and road-building placements first, walls last.
  static int orderKey(const Move& m) {
    if (m.kind == MoveKind::Place) {
      const Stone s = static_cast<Stone>(m.what);
      return s == Stone::Flat ? 3 : (s == Stone::Cap ? 2 : 0);
    }
    return 1;
  }

  void order(MoveList& list) const {
    // Stable partition by key, highest first, without extra storage.
    int at = 0;
    for (int key = 3; key >= 0; --key) {
      for (int i = at; i < list.count; ++i) {
        if (orderKey(list.moves[i]) == key) {
          const Move tmp = list.moves[i];
          for (int j = i; j > at; --j) list.moves[j] = list.moves[j - 1];
          list.moves[at++] = tmp;
        }
      }
    }
  }

  int negamax(const Game& g, const int depth, int alpha, const int beta, const int ply) {
    ++nodes;
    const uint8_t me = g.turn;
    if (over(g.result)) return terminal(g, me, ply);
    if (depth == 0) return evaluate(g, me);
    if (nodes >= budget) {
      stopped = true;
      return evaluate(g, me);
    }
    MoveList& list = buffers.plies[depth];
    generate(g, list);
    if (list.count == 0) return evaluate(g, me);
    // A move that wins on the spot ends the search here. Each trial is a
    // played position, so it is paid for out of the same budget.
    nodes += static_cast<uint32_t>(list.count);
    Game& next = buffers.games[ply + 1];
    for (int i = 0; i < list.count; ++i) {
      next = g;
      apply(next, list.moves[i]);
      if (winner(next.result) == me) return kWinScore - ply - 1;
    }
    if (depth > 1) order(list);
    int best = -kWinScore * 2;
    for (int i = 0; i < list.count; ++i) {
      next = g;
      apply(next, list.moves[i]);
      const int score = -negamax(next, depth - 1, -beta, -alpha, ply + 1);
      if (score > best) best = score;
      if (best > alpha) alpha = best;
      if (alpha >= beta || stopped) break;
      if (nodes >= budget) {
        stopped = true;
        break;
      }
    }
    return best;
  }
};

uint32_t nextRandom(uint32_t& state) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return state;
}

}  // namespace

Limits limitsFor(const Level level, const uint32_t seed) {
  Limits l;
  l.seed = seed == 0 ? 1 : seed;
  switch (level) {
    case Level::Easy:
      l.maxDepth = 1;
      l.nodeBudget = 4000;
      l.noise = 260;
      break;
    case Level::Medium:
      l.maxDepth = 2;
      l.nodeBudget = 30000;
      l.noise = 40;
      break;
    case Level::Hard:
      l.maxDepth = 3;
      l.nodeBudget = 150000;
      l.noise = 0;
      break;
  }
  return l;
}

int evaluate(const Game& game, const uint8_t color) {
  if (over(game.result)) {
    const uint8_t w = winner(game.result);
    return w == 0xFF ? 0 : (w == color ? kWinScore : -kWinScore);
  }
  int score = sideScore(game, color) - sideScore(game, other(color));
  // Late in the game the flat count is what decides it.
  const int left = game.stones[color] + game.caps[color];
  const int theirs = game.stones[other(color)] + game.caps[other(color)];
  const int least = left < theirs ? left : theirs;
  if (least <= 4) score += (flatCount(game, color) - flatCount(game, other(color))) * (5 - least) * 150;
  if (game.turn == color) score += kTempo;
  return score;
}

SearchResult search(const Game& game, SearchBuffers& buffers, const Limits& limits) {
  SearchResult result;
  if (over(game.result)) return result;
  MoveList& root = buffers.plies[0];
  generate(game, root);
  if (root.count == 0) return result;
  const uint8_t me = game.turn;
  result.best = root.moves[0];
  result.found = true;

  // A road in one is taken without searching. Refusing to allow one is the
  // search's job from depth two up, which is why the easiest level misses it.
  Game& next = buffers.games[1];
  for (int i = 0; i < root.count; ++i) {
    next = game;
    apply(next, root.moves[i]);
    if (winner(next.result) == me) {
      result.best = root.moves[i];
      result.score = kWinScore;
      result.depth = 1;
      return result;
    }
  }

  uint32_t rng = limits.seed;
  int* jitter = buffers.jitter;
  const int jittered = root.count < kMaxJitter ? root.count : kMaxJitter;
  for (int i = 0; i < jittered; ++i) {
    jitter[i] = limits.noise > 0 ? static_cast<int>(nextRandom(rng) % static_cast<uint32_t>(limits.noise)) : 0;
  }

  Searcher s{buffers, limits.nodeBudget};
  const int maxDepth =
      limits.maxDepth < 1 ? 1 : (limits.maxDepth > kMaxSearchDepth ? kMaxSearchDepth : limits.maxDepth);
  int bestIndex = 0;
  for (int depth = 1; depth <= maxDepth; ++depth) {
    int alpha = -kWinScore * 2;
    int iterBest = -1;
    int iterScore = -kWinScore * 2;
    // Last iteration's best goes first.
    if (bestIndex != 0) {
      const Move tmp = root.moves[bestIndex];
      for (int j = bestIndex; j > 0; --j) root.moves[j] = root.moves[j - 1];
      root.moves[0] = tmp;
      if (bestIndex < jittered) {
        const int jt = jitter[bestIndex];
        for (int j = bestIndex; j > 0; --j) jitter[j] = jitter[j - 1];
        jitter[0] = jt;
      }
      bestIndex = 0;
    }
    for (int i = 0; i < root.count; ++i) {
      next = game;
      apply(next, root.moves[i]);
      int score = -s.negamax(next, depth - 1, -kWinScore * 2, -alpha + (limits.noise > 0 ? limits.noise : 0), 1);
      if (i < jittered) score += jitter[i];
      if (s.stopped && depth > 1) break;
      if (score > iterScore) {
        iterScore = score;
        iterBest = i;
      }
      if (iterScore > alpha) alpha = iterScore;
    }
    if (iterBest < 0) break;
    // A cut-off iteration is only trusted for the moves it finished, and it
    // finished the previous best first, so its pick is at least as good.
    bestIndex = iterBest;
    result.best = root.moves[iterBest];
    result.score = iterScore;
    result.depth = depth;
    if (s.stopped) break;
  }
  result.nodes = s.nodes;
  return result;
}

}  // namespace tak
