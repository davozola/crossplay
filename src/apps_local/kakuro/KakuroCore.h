#pragma once

// Kakuro, freestanding: the bank, the clues derived from it, the runs, the
// player's digits and notes, and the checks. No Arduino, no heap.
// host-tests/kakuro drives it and re-proves every puzzle in the bank.

#include <cstdint>

namespace kakuro {

constexpr int kMaxSide = 10;
constexpr int kMaxCells = kMaxSide * kMaxSide;
// Every white cell is in one across and one down run; a run is at least two
// cells, so there are at most this many.
constexpr int kMaxRuns = kMaxCells;

enum class Size : uint8_t { Small = 0, Medium = 1, Large = 2 };
constexpr int kSizeCount = 3;

int sideOf(Size size);
int countOf(Size size);

struct Run {
  uint8_t start = 0;  // first white cell
  uint8_t length = 0;
  uint8_t sum = 0;
  bool down = false;
};

// The puzzle's fixed parts, derived from the bank entry.
struct Layout {
  uint8_t side = 0;
  uint8_t solution[kMaxCells] = {};  // 0 for a block
  // On a block: the across and down sums it carries, 0 for none.
  uint8_t acrossClue[kMaxCells] = {};
  uint8_t downClue[kMaxCells] = {};
  int runCount = 0;
  Run runs[kMaxRuns];
  // For a white cell: its across run and its down run.
  uint8_t acrossRun[kMaxCells] = {};
  uint8_t downRun[kMaxCells] = {};
};

bool load(Size size, int index, Layout& out);
inline bool white(const Layout& l, const int cell) { return l.solution[cell] != 0; }
int runCell(const Layout& l, const Run& run, int i);

// What the player has done to one puzzle. Small enough to save whole.
struct Progress {
  uint8_t entry[kMaxCells] = {};   // 0 empty, 1-9
  uint16_t notes[kMaxCells] = {};  // bit d set: d pencilled in
};

void setDigit(Progress& p, int cell, int digit);  // also clears the cell's notes
void toggleNote(Progress& p, int cell, int digit);
void clearCell(Progress& p, int cell);

// A white cell whose digit is repeated elsewhere in one of its runs.
bool duplicate(const Layout& l, const Progress& p, int cell);
// A run every cell of which is filled and whose digits do not make its sum or
// repeat. Drawn on its clue.
bool runWrong(const Layout& l, const Progress& p, int run);
// Every white cell holds the solution's digit.
bool solved(const Layout& l, const Progress& p);
int filledCount(const Layout& l, const Progress& p);
int whiteCount(const Layout& l);

// The combinations of `length` distinct digits 1-9 adding up to `sum`, as
// bitmasks (bit d), in ascending order. Returns how many; writes at most
// `max`.
int combinations(int sum, int length, uint16_t* out, int max);

}  // namespace kakuro
