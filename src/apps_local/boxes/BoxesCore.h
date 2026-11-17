#pragma once

// DOTS AND BOXES, the rules and the opponent. Freestanding and header-only,
// so host-tests/boxes plays it.
//
// Players take turns drawing one side of a square on a grid of dots. Whoever
// draws a square's fourth side owns it and goes again. Most squares wins.

#include <cstdint>

namespace boxes {

constexpr int kMaxSide = 6;  // squares along a side
constexpr int kMaxBoxes = kMaxSide * kMaxSide;
constexpr int kMaxLines = 2 * kMaxSide * (kMaxSide + 1);

struct Board {
  int w = 4;
  int h = 4;
  bool line[kMaxLines] = {};
  uint8_t owner[kMaxBoxes] = {};  // 0 nobody, 1 or 2
  uint8_t turn = 1;
  int score[3] = {0, 0, 0};
  int16_t last = -1;

  // Lines are numbered across first: the (h + 1) rows of w, then the h rows of (w + 1) uprights.
  int across() const { return (h + 1) * w; }
  int lines() const { return across() + h * (w + 1); }
  int top(const int box) const { return box; }
  int bottom(const int box) const { return box + w; }
  int left(const int box) const { return across() + (box / w) * (w + 1) + box % w; }
  int right(const int box) const { return left(box) + 1; }
  int sides(const int box) const { return line[top(box)] + line[bottom(box)] + line[left(box)] + line[right(box)]; }
  bool over() const { return score[1] + score[2] == w * h; }
};

inline void start(Board& board, const int w, const int h) {
  board = Board{};
  board.w = w;
  board.h = h;
}

// The squares (at most two) a line is a side of. Returns how many.
inline int boxesOf(const Board& board, const int line, int out[2]) {
  int n = 0;
  if (line < board.across()) {
    const int row = line / board.w;
    const int col = line % board.w;
    if (row > 0) out[n++] = (row - 1) * board.w + col;
    if (row < board.h) out[n++] = row * board.w + col;
  } else {
    const int k = line - board.across();
    const int row = k / (board.w + 1);
    const int col = k % (board.w + 1);
    if (col > 0) out[n++] = row * board.w + col - 1;
    if (col < board.w) out[n++] = row * board.w + col;
  }
  return n;
}

// The player to move draws `line`. Returns the squares it closed, -1 when the
// line is already drawn. Closing one keeps the turn.
inline int draw(Board& board, const int line) {
  if (line < 0 || line >= board.lines() || board.line[line] || board.over()) return -1;
  board.line[line] = true;
  board.last = static_cast<int16_t>(line);
  int touched[2];
  const int n = boxesOf(board, line, touched);
  int closed = 0;
  for (int i = 0; i < n; ++i) {
    if (board.owner[touched[i]] == 0 && board.sides(touched[i]) == 4) {
      board.owner[touched[i]] = board.turn;
      ++board.score[board.turn];
      ++closed;
    }
  }
  if (closed == 0) board.turn = static_cast<uint8_t>(3 - board.turn);
  return closed;
}

struct Rng {
  uint32_t state = 0x9E3779B9u;
  uint32_t next() {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
  }
  int below(const int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

// How many squares the other side could then take in a row if `line` were
// drawn: what a move gives away.
inline int givesAway(const Board& board, const int line) {
  Board copy = board;
  copy.line[line] = true;
  int given = 0;
  for (bool again = true; again;) {
    again = false;
    for (int box = 0; box < copy.w * copy.h; ++box) {
      if (copy.owner[box] != 0 || copy.sides(box) != 3) continue;
      const int around[4] = {copy.top(box), copy.bottom(box), copy.left(box), copy.right(box)};
      for (const int side : around) copy.line[side] = true;
      copy.owner[box] = 3;
      ++given;
      again = true;
    }
    // Squares the last line closed outright are counted as taken too.
    for (int box = 0; box < copy.w * copy.h; ++box) {
      if (copy.owner[box] == 0 && copy.sides(box) == 4) copy.owner[box] = 3;
    }
  }
  return given;
}

constexpr int kLevels = 3;

// The line the device draws, -1 when none is left. Level 0 draws anything,
// though it takes a square put in front of it half the time; level 1 always
// takes squares and avoids setting one up; level 2 also, when it has to give
// something away, gives the least.
inline int choose(const Board& board, const int level, Rng& rng) {
  int open[kMaxLines];
  int openCount = 0;
  int closing[kMaxLines];
  int closingCount = 0;
  int safe[kMaxLines];
  int safeCount = 0;
  for (int line = 0; line < board.lines(); ++line) {
    if (board.line[line]) continue;
    open[openCount++] = line;
    int touched[2];
    const int n = boxesOf(board, line, touched);
    bool closes = false;
    bool setsUp = false;
    for (int i = 0; i < n; ++i) {
      const int sides = board.sides(touched[i]);
      closes = closes || sides == 3;
      setsUp = setsUp || sides == 2;
    }
    if (closes) closing[closingCount++] = line;
    if (!closes && !setsUp) safe[safeCount++] = line;
  }
  if (openCount == 0) return -1;
  if (closingCount > 0 && (level > 0 || rng.below(2) == 0)) return closing[rng.below(closingCount)];
  if (level == 0) return open[rng.below(openCount)];
  if (safeCount > 0) return safe[rng.below(safeCount)];
  if (level == 1) return open[rng.below(openCount)];
  int best = open[0];
  int least = 1000;
  for (int i = 0; i < openCount; ++i) {
    const int given = givesAway(board, open[i]) * 4 + rng.below(3);
    if (given < least) {
      least = given;
      best = open[i];
    }
  }
  return best;
}

}  // namespace boxes
