#pragma once

// Four grid puzzles that share a shelf of boards and a way of being played:
// Slitherlink, Shikaku, Akari and Hashi. Freestanding. The device never solves
// a board, it only says whether what has been drawn on it is right; every
// board has one solution (tools_local/logic), so "right" and "the answer" are
// the same thing.

#include <cstdint>

namespace logic {

constexpr int kMaxSide = 11;
constexpr int kMaxCells = kMaxSide * kMaxSide;
constexpr int kSizes = 3;

enum class Game : uint8_t { Slither = 0, Shikaku = 1, Akari = 2, Hashi = 3 };
constexpr int kGames = 4;

const char* gameName(Game game);
// One line on how it is played, for the picker.
const char* gameRule(Game game);
int sideOf(Game game, int sizeIndex);
int boardsPerSize();

struct Board {
  uint8_t w = 0;
  uint8_t h = 0;
  const char* cells = nullptr;
  char at(int x, int y) const { return cells[y * w + x]; }
};
Board boardOf(Game game, int sizeIndex, int number);

// ---- Slitherlink: one closed loop along the dots; a number is how many of
// its cell's four sides the loop uses.
struct Slither {
  enum : uint8_t { None = 0, Line = 1, Cross = 2 };
  Board b;
  uint8_t hz[(kMaxSide + 1) * kMaxSide] = {};  // [y * w + x], y in 0..h
  uint8_t vt[kMaxSide * (kMaxSide + 1)] = {};  // [y * (w + 1) + x], x in 0..w

  void load(const Board& board);
  void cycleH(int x, int y);
  void cycleV(int x, int y);
  int linesAround(int x, int y) const;
  bool solved() const;
  int save(uint8_t* out, int cap) const;
  bool restore(const uint8_t* in, int len);
};

// ---- Shikaku: cut the grid into rectangles, each holding one number, which
// is its area.
struct Shikaku {
  struct Rect {
    int8_t x, y, w, h;
  };
  Board b;
  Rect rects[kMaxCells] = {};
  int count = 0;
  int8_t anchorX = -1;  // the first corner tapped, waiting for the second
  int8_t anchorY = -1;

  void load(const Board& board);
  int rectAt(int x, int y) const;
  // First tap on a free cell sets a corner, the second closes the rectangle
  // (taking away any it lands on). A tap on a rectangle removes it.
  void tap(int x, int y);
  bool good(int rect) const;
  bool solved() const;
  int save(uint8_t* out, int cap) const;
  bool restore(const uint8_t* in, int len);
};

// ---- Akari: light every white cell; bulbs must not shine on each other; a
// numbered wall has that many bulbs against it.
struct Akari {
  enum : uint8_t { Empty = 0, Bulb = 1, Dot = 2 };
  Board b;
  uint8_t mark[kMaxCells] = {};

  void load(const Board& board);
  bool white(int x, int y) const { return b.at(x, y) == '.'; }
  void tap(int x, int y);
  bool lit(int x, int y) const;
  // True for a bulb another bulb can see.
  bool clash(int x, int y) const;
  int bulbsAround(int x, int y) const;
  bool solved() const;
  int save(uint8_t* out, int cap) const;
  bool restore(const uint8_t* in, int len);
};

// ---- Hashi: join the islands with bridges, one or two between a pair, never
// crossing, each island carrying its number, all of it one piece.
struct Hashi {
  struct Island {
    int8_t x, y, n;
  };
  struct Edge {
    uint8_t a, b, count;
  };
  static constexpr int kMaxIslands = 48;
  static constexpr int kMaxEdges = 96;
  Board b;
  Island islands[kMaxIslands] = {};
  int islandCount = 0;
  Edge edges[kMaxEdges] = {};
  int edgeCount = 0;

  void load(const Board& board);
  int sumOf(int island) const;
  bool horizontal(int edge) const { return islands[edges[edge].a].y == islands[edges[edge].b].y; }
  bool blocked(int edge) const;  // a bridge already crosses where this would go
  // None, one, two, none. False when a crossing bridge is in the way.
  bool cycle(int edge);
  // The possible bridge nearest a point given in cells (2.5 is the middle of
  // the third column), or -1 when none is within half a cell.
  int edgeNear(float gx, float gy) const;
  bool solved() const;
  int save(uint8_t* out, int cap) const;
  bool restore(const uint8_t* in, int len);
};

}  // namespace logic
