#pragma once

// REVERSI, the rules and the opponent. Freestanding and header-only, so
// host-tests/reversi plays it.
//
// A stone goes where it traps at least one line of the other colour between
// itself and another of its own; every stone so trapped turns over. A player
// with nowhere to go passes; when neither can move the fuller colour wins.

#include <cstdint>

namespace reversi {

constexpr int kMaxSide = 8;
constexpr int kMaxCells = kMaxSide * kMaxSide;
enum : uint8_t { Empty = 0, Black = 1, White = 2 };

inline uint8_t other(const uint8_t player) { return player == Black ? White : Black; }

struct Board {
  uint8_t cell[kMaxCells] = {};
  int side = 8;
  uint8_t turn = Black;  // black moves first
  int8_t last = -1;      // the square most recently played, for the mark on it

  int cells() const { return side * side; }
  int count(const uint8_t colour) const {
    int n = 0;
    for (int i = 0; i < cells(); ++i) n += cell[i] == colour;
    return n;
  }
};

inline void start(Board& board, const int side) {
  board = Board{};
  board.side = side;
  const int a = side / 2 - 1;
  const int b = side / 2;
  board.cell[a * side + a] = White;
  board.cell[b * side + b] = White;
  board.cell[a * side + b] = Black;
  board.cell[b * side + a] = Black;
}

// How many stones `player` would turn by playing `at`; with `apply`, does it.
// 0 means the move is not allowed.
inline int flips(Board& board, const int at, const uint8_t player, const bool apply) {
  if (at < 0 || at >= board.cells() || board.cell[at] != Empty) return 0;
  const int side = board.side;
  const int x0 = at % side;
  const int y0 = at / side;
  const uint8_t foe = other(player);
  int total = 0;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      int x = x0 + dx;
      int y = y0 + dy;
      int run = 0;
      while (x >= 0 && y >= 0 && x < side && y < side && board.cell[y * side + x] == foe) {
        x += dx;
        y += dy;
        ++run;
      }
      if (run == 0 || x < 0 || y < 0 || x >= side || y >= side || board.cell[y * side + x] != player) continue;
      total += run;
      if (apply) {
        for (int k = 1; k <= run; ++k) board.cell[(y0 + dy * k) * side + x0 + dx * k] = player;
      }
    }
  }
  if (apply && total > 0) board.cell[at] = player;
  return total;
}

inline bool legal(const Board& board, const int at, const uint8_t player) {
  Board copy = board;
  return flips(copy, at, player, false) > 0;
}

inline bool canMove(const Board& board, const uint8_t player) {
  for (int i = 0; i < board.cells(); ++i) {
    if (board.cell[i] == Empty && legal(board, i, player)) return true;
  }
  return false;
}

inline bool over(const Board& board) { return !canMove(board, Black) && !canMove(board, White); }

enum class Played : uint8_t { No, Yes, YesAndOtherPasses, YesAndOver };

// The player to move plays `at`. The turn goes to the other colour unless it
// has nowhere to go.
inline Played play(Board& board, const int at) {
  if (flips(board, at, board.turn, true) == 0) return Played::No;
  board.last = static_cast<int8_t>(at);
  const uint8_t next = other(board.turn);
  if (canMove(board, next)) {
    board.turn = next;
    return Played::Yes;
  }
  if (canMove(board, board.turn)) return Played::YesAndOtherPasses;
  return Played::YesAndOver;
}

// --- The opponent --------------------------------------------------------------

struct Rng {
  uint32_t state = 0x2545F491u;
  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
  int below(const int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

// What a square is worth to hold: corners are gold, the squares beside them
// hand the corner over, edges are good.
inline int squareValue(const int side, const int at) {
  const int x = at % side;
  const int y = at / side;
  const int ex = x < side - 1 - x ? x : side - 1 - x;  // distance from the nearer edge
  const int ey = y < side - 1 - y ? y : side - 1 - y;
  if (ex == 0 && ey == 0) return 100;
  if (ex <= 1 && ey <= 1) return ex == 1 && ey == 1 ? -40 : -20;
  if (ex == 0 || ey == 0) return 10;
  if (ex == 1 || ey == 1) return -2;
  return 1;
}

inline int evaluate(const Board& board, const uint8_t player) {
  const uint8_t foe = other(player);
  int score = 0;
  int empty = 0;
  for (int i = 0; i < board.cells(); ++i) {
    if (board.cell[i] == player) score += squareValue(board.side, i);
    if (board.cell[i] == foe) score -= squareValue(board.side, i);
    empty += board.cell[i] == Empty;
  }
  // Near the end only the count matters.
  if (empty <= 10) return (board.count(player) - board.count(foe)) * 10;
  int mine = 0;
  int theirs = 0;
  for (int i = 0; i < board.cells(); ++i) {
    if (board.cell[i] != Empty) continue;
    mine += legal(board, i, player);
    theirs += legal(board, i, foe);
  }
  return score + (mine - theirs) * 4;
}

inline int search(const Board& board, const uint8_t player, const uint8_t mover, const int depth, int alpha,
                  const int beta) {
  if (depth == 0) return evaluate(board, player);
  bool moved = false;
  int best = mover == player ? -100000 : 100000;
  int low = alpha;
  int high = beta;
  for (int i = 0; i < board.cells(); ++i) {
    if (board.cell[i] != Empty) continue;
    Board next = board;
    if (flips(next, i, mover, true) == 0) continue;
    moved = true;
    const int value = search(next, player, other(mover), depth - 1, low, high);
    if (mover == player) {
      if (value > best) best = value;
      if (best > low) low = best;
    } else {
      if (value < best) best = value;
      if (best < high) high = best;
    }
    if (low >= high) break;
  }
  if (moved) return best;
  // A pass; two in a row is the end.
  if (!canMove(board, other(mover))) return (board.count(player) - board.count(other(player))) * 1000;
  return search(board, player, other(mover), depth - 1, alpha, beta);
}

constexpr int kLevels = 4;

// The square the device plays for the side to move, -1 when it has none.
// Level 0 plays any allowed square, leaning to the one that turns most;
// level 1 looks at what each square is worth; level 2 thinks four moves on;
// level 3 thinks six on, and once eight squares or fewer are left it plays the
// rest out exactly, to the last stone.
inline int choose(const Board& board, const int level, Rng& rng) {
  const uint8_t player = board.turn;
  int best = -1;
  int bestScore = -1000000;
  for (int i = 0; i < board.cells(); ++i) {
    if (board.cell[i] != Empty) continue;
    Board next = board;
    const int turned = flips(next, i, player, true);
    if (turned == 0) continue;
    int score = 0;
    if (level <= 0) {
      score = turned + rng.below(6);
    } else if (level == 1) {
      score = squareValue(board.side, i) * 3 + turned + rng.below(3);
    } else if (level == 2) {
      score = search(next, player, other(player), 3, -100000, 100000) * 4 + rng.below(3);
    } else {
      // Deep enough to reach the end is the end counted, not judged.
      int empties = 0;
      for (int k = 0; k < next.cells(); ++k) empties += next.cell[k] == Empty ? 1 : 0;
      const int depth = empties <= 8 ? empties + 2 : 5;
      score = search(next, player, other(player), depth, -100000, 100000);
    }
    if (score > bestScore) {
      bestScore = score;
      best = i;
    }
  }
  return best;
}

}  // namespace reversi
