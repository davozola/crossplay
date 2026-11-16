#pragma once

// Mastermind against the device: it hides a code, you have a fixed number of
// guesses, and after each one it says how many symbols are right and in the
// right place (black) and how many are right but elsewhere (white).
// Freestanding, so host-tests/mastermind checks the scoring, which is where
// every implementation of this game goes wrong: repeated symbols.

#include <cstdint>

namespace mastermind {

constexpr int kMaxPegs = 5;
constexpr int kMaxSymbols = 8;
constexpr int kMaxRows = 12;
constexpr uint8_t kEmpty = 0xFF;

enum class Mode : uint8_t { Easy = 0, Classic = 1, Hard = 2 };
constexpr int kModeCount = 3;

struct Rules {
  uint8_t pegs;
  uint8_t symbols;
  uint8_t rows;
  bool repeats;
};

// Easy: 4 of 6, no repeats, 10 rows. Classic: 4 of 6, repeats, 10 rows.
// Hard: 5 of 8, repeats, 12 rows.
Rules rulesFor(Mode mode);

struct Score {
  uint8_t black = 0;
  uint8_t white = 0;
};

Score score(const uint8_t* code, const uint8_t* guess, int pegs);

enum class State : uint8_t { Playing, Won, Lost };

struct Game {
  Mode mode = Mode::Classic;
  uint8_t code[kMaxPegs] = {};
  uint8_t guesses[kMaxRows][kMaxPegs] = {};
  Score scores[kMaxRows];
  uint8_t rowsUsed = 0;
  // The row being composed.
  uint8_t current[kMaxPegs] = {kEmpty, kEmpty, kEmpty, kEmpty, kEmpty};
  State state = State::Playing;
};

void start(Game& game, Mode mode, uint32_t seed);

// Puts `symbol` in the first empty slot of the row being composed. False when
// the row is full, the game is over, or (without repeats) the symbol is
// already in the row.
bool add(Game& game, int symbol);
// Empties one slot of the row being composed.
bool clearSlot(Game& game, int slot);
void clearRow(Game& game);
bool rowFull(const Game& game);
// Scores the composed row and moves on. False unless the row is full.
bool submit(Game& game);

}  // namespace mastermind
