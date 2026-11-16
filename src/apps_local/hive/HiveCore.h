#pragma once

// HIVE: the rules of the base game, with no Arduino in them. No board: pieces
// sit on a hex lattice that has no edge, each one knowing its cell and how
// high in a stack it is. host-tests/hive plays whole games against these.
//
// Eleven pieces a side: one queen, two beetles, two spiders, three
// grasshoppers, three ants. White moves first. A queen must be down by its
// owner's fourth turn and nothing of theirs moves until it is. The hive is
// never split, even for the length of a move. Surround the other queen to win.

#include <cstdint>

namespace hive {

enum class Bug : uint8_t { Queen = 0, Beetle = 1, Spider = 2, Hopper = 3, Ant = 4 };
constexpr int kBugTypes = 5;
constexpr int kBugCount[kBugTypes] = {1, 2, 2, 3, 3};
constexpr int kPerPlayer = 11;
constexpr int kPieces = kPerPlayer * 2;
// Six neighbours in turn, so the two either side of one are the cells a
// slide in that direction squeezes between.
constexpr int8_t kDirQ[6] = {1, 1, 0, -1, -1, 0};
constexpr int8_t kDirR[6] = {0, -1, -1, 0, 1, 1};

const char* bugName(Bug bug);
char bugLetter(Bug bug);

struct Piece {
  uint8_t bug = 0;
  uint8_t color = 0;  // 0 white, 1 black
  uint8_t placed = 0;
  uint8_t z = 0;  // 0 on the table, 1 on top of one piece, ...
  int8_t q = 0;
  int8_t r = 0;
};

struct Move {
  int8_t piece = -1;  // index into Game::pieces; -1 for a pass
  int8_t q = 0;
  int8_t r = 0;
  bool pass() const { return piece < 0; }
};

// More than any position offers: five kinds to place on a few dozen cells
// and eleven pieces to move.
constexpr int kMaxMoves = 512;

class Game {
 public:
  Piece pieces[kPieces];
  uint8_t turn = 0;
  uint8_t turnsTaken[2] = {0, 0};

  Game() { reset(); }
  void reset();

  // The piece on top at a cell, or -1. `skip` is left out, as if lifted.
  int topAt(int q, int r, int skip = -1) const;
  int heightAt(int q, int r, int skip = -1) const;
  int inHand(int color, Bug bug) const;
  int handPiece(int color, Bug bug) const;
  bool queenPlaced(int color) const;
  int placedCount() const;

  // Every move the side to move has; a single pass when it has none.
  int legalMoves(Move* out, int cap) const;
  // Where one piece may go: cells to place it if it is in hand, to move it if
  // it is down. Zero when it is not this side's, or the rules hold it still.
  int destinations(int piece, Move* out, int cap) const;
  void apply(const Move& move);

  int queenNeighbours(int color) const;
  // -1 nobody yet, 0 white, 1 black, 2 both queens surrounded at once.
  int winner() const;
  // The invariant: everything on the table is one group.
  bool connected(int skip = -1) const;

 private:
  int placements(int color, int piece, Move* out, int cap) const;
  int movements(int piece, Move* out, int cap) const;
  bool slides(int q, int r, int dir, int skip) const;
};

}  // namespace hive
