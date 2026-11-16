#pragma once

// The rules of Tak, freestanding: no Arduino, no heap, no clock. Everything the
// activity and the engine know about the game is asked of this file, and
// host-tests/tak checks it on a laptop.
//
// Rules as played on playtak.com (USTA): 4x4, 5x5 and 6x6; the first move of
// each player places one of the OPPONENT's flats; a stack moves up to `size`
// pieces in a straight line, dropping at least one on every square it passes;
// walls and capstones block; a capstone moving alone onto a wall flattens it;
// a road of flats and capstones joining opposite edges wins, and when one
// move completes roads for both sides the mover wins; otherwise the game ends
// when the board is full or either side has placed everything, and the side
// with more flats on top wins.

#include <cstdint>

namespace tak {

constexpr int kMinSize = 4;
constexpr int kMaxSize = 6;
constexpr int kMaxSquares = kMaxSize * kMaxSize;

constexpr uint8_t kWhite = 0;
constexpr uint8_t kBlack = 1;
constexpr uint8_t other(const uint8_t color) { return static_cast<uint8_t>(color ^ 1); }

enum class Stone : uint8_t { Flat = 0, Wall = 1, Cap = 2 };

// Pieces in the box for each size: {stones, capstones}.
constexpr int stonesFor(const int size) { return size == 4 ? 15 : (size == 5 ? 21 : 30); }
constexpr int capsFor(const int size) { return size == 4 ? 0 : 1; }

enum Direction : uint8_t { North = 0, East = 1, South = 2, West = 3 };

// A stack. Bit i of `colors` is the colour of the i-th piece from the bottom
// (1 = black). Only the top piece can be a wall or a capstone, so one field
// says which. 62 is the most pieces a 6x6 game owns, so 64 bits always fit.
struct Square {
  uint64_t colors = 0;
  uint8_t height = 0;
  Stone top = Stone::Flat;
};

constexpr bool empty(const Square& s) { return s.height == 0; }
constexpr uint8_t topColor(const Square& s) { return static_cast<uint8_t>((s.colors >> (s.height - 1)) & 1u); }
// Counts toward a road: a flat or a capstone. A wall stands, and blocks.
constexpr bool roadPiece(const Square& s) { return s.height > 0 && s.top != Stone::Wall; }

enum class Result : uint8_t { Ongoing, WhiteRoad, BlackRoad, WhiteFlats, BlackFlats, Draw };

constexpr bool over(const Result r) { return r != Result::Ongoing; }
// Which colour won, or 0xFF for a draw or a game still going.
constexpr uint8_t winner(const Result r) {
  return (r == Result::WhiteRoad || r == Result::WhiteFlats)
             ? kWhite
             : ((r == Result::BlackRoad || r == Result::BlackFlats) ? kBlack : 0xFF);
}
constexpr bool roadWin(const Result r) { return r == Result::WhiteRoad || r == Result::BlackRoad; }

// Squares are numbered row-major from the top-left: index = row * size + col.
// Row 0 is the top of the screen; in PTN that is rank `size`.
struct Game {
  uint8_t size = 5;
  uint8_t turn = kWhite;
  uint16_t ply = 0;
  uint8_t stones[2] = {21, 21};
  uint8_t caps[2] = {1, 1};
  Result result = Result::Ongoing;
  Square sq[kMaxSquares];
};

enum class MoveKind : uint8_t { Place = 0, Slide = 1 };

// A placement, or a slide of `pick` pieces from `square` toward `dir`, dropping
// `drops` nibble by nibble (lowest nibble first) on successive squares. A
// nibble of zero ends the list; every listed drop is at least one, and they add
// up to `pick`.
struct Move {
  MoveKind kind = MoveKind::Place;
  uint8_t square = 0;
  // Place: the Stone. Slide: the Direction.
  uint8_t what = 0;
  uint8_t pick = 0;
  uint32_t drops = 0;
};

bool operator==(const Move& a, const Move& b);
inline bool operator!=(const Move& a, const Move& b) { return !(a == b); }

constexpr Move place(const int square, const Stone stone) {
  return Move{MoveKind::Place, static_cast<uint8_t>(square), static_cast<uint8_t>(stone), 0, 0};
}
// Builds a slide from a list of drop counts.
Move slide(int square, Direction dir, const uint8_t* drops, int count);
int dropCount(const Move& m);
inline int dropAt(const Move& m, const int i) { return static_cast<int>((m.drops >> (4 * i)) & 0xFu); }

// Every legal move, in a fixed order. 4096 covers the worst position on 6x6
// with room to spare: placements are at most 3 x 36, and a stack's slides are
// bounded by 4 directions x 2^6 drop patterns.
constexpr int kMaxMoves = 4096;
struct MoveList {
  int count = 0;
  Move moves[kMaxMoves];
};

void reset(Game& game, int size);
// The colour of the piece placed this turn: the opponent's on each side's first
// move, one's own after.
uint8_t placingColor(const Game& game);
bool openingPly(const Game& game);

int generate(const Game& game, MoveList& out);
bool isLegal(const Game& game, const Move& move);
// Plays a move generate() would have produced, and settles the result. Does
// not check legality: the engine calls it in its inner loop. Callers holding a
// move from a person check isLegal() first.
void apply(Game& game, const Move& move);

// Whether `color` has a road on the board as it stands.
bool hasRoad(const Game& game, uint8_t color);
// The squares of one road for `color`, as a bitmask over square indices, or 0.
// The result screen marks these.
uint64_t roadSquares(const Game& game, uint8_t color);
// Flats on top for each side, which is what a flat win counts.
int flatCount(const Game& game, uint8_t color);

// Neighbour in a direction, or -1 off the board.
int step(const Game& game, int square, Direction dir);

// Portable Tak Notation for a move, e.g. "c3", "Sa1", "3c3>12". `out` needs 12.
void toPtn(const Game& game, const Move& move, char* out, int outSize);
// The inverse, for tests and saved games. False on anything malformed; says
// nothing about legality.
bool fromPtn(const Game& game, const char* text, Move& out);

}  // namespace tak
