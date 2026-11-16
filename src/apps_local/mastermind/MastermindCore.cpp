#include "MastermindCore.h"

namespace mastermind {

Rules rulesFor(const Mode mode) {
  switch (mode) {
    case Mode::Easy:
      return Rules{4, 6, 10, false};
    case Mode::Classic:
      return Rules{4, 6, 10, true};
    case Mode::Hard:
      return Rules{5, 8, 12, true};
  }
  return Rules{4, 6, 10, true};
}

Score score(const uint8_t* code, const uint8_t* guess, const int pegs) {
  Score s;
  int codeLeft[kMaxSymbols] = {};
  int guessLeft[kMaxSymbols] = {};
  for (int i = 0; i < pegs; ++i) {
    if (code[i] == guess[i]) {
      ++s.black;
    } else {
      ++codeLeft[code[i] % kMaxSymbols];
      ++guessLeft[guess[i] % kMaxSymbols];
    }
  }
  // Each symbol scores white at most as often as it is left over on BOTH sides.
  for (int k = 0; k < kMaxSymbols; ++k)
    s.white += static_cast<uint8_t>(codeLeft[k] < guessLeft[k] ? codeLeft[k] : guessLeft[k]);
  return s;
}

void start(Game& game, const Mode mode, uint32_t seed) {
  game = Game{};
  game.mode = mode;
  const Rules r = rulesFor(mode);
  if (seed == 0) seed = 0x9E3779B9u;
  bool used[kMaxSymbols] = {};
  for (int i = 0; i < r.pegs; ++i) {
    for (;;) {
      seed ^= seed << 13;
      seed ^= seed >> 17;
      seed ^= seed << 5;
      const int s = static_cast<int>(seed % r.symbols);
      if (!r.repeats && used[s]) continue;
      used[s] = true;
      game.code[i] = static_cast<uint8_t>(s);
      break;
    }
  }
  for (int i = 0; i < kMaxPegs; ++i) game.current[i] = kEmpty;
}

bool add(Game& game, const int symbol) {
  const Rules r = rulesFor(game.mode);
  if (game.state != State::Playing || symbol < 0 || symbol >= r.symbols) return false;
  if (!r.repeats) {
    for (int i = 0; i < r.pegs; ++i)
      if (game.current[i] == symbol) return false;
  }
  for (int i = 0; i < r.pegs; ++i) {
    if (game.current[i] == kEmpty) {
      game.current[i] = static_cast<uint8_t>(symbol);
      return true;
    }
  }
  return false;
}

bool clearSlot(Game& game, const int slot) {
  const Rules r = rulesFor(game.mode);
  if (game.state != State::Playing || slot < 0 || slot >= r.pegs || game.current[slot] == kEmpty) return false;
  game.current[slot] = kEmpty;
  return true;
}

void clearRow(Game& game) {
  for (int i = 0; i < kMaxPegs; ++i) game.current[i] = kEmpty;
}

bool rowFull(const Game& game) {
  const Rules r = rulesFor(game.mode);
  for (int i = 0; i < r.pegs; ++i)
    if (game.current[i] == kEmpty) return false;
  return true;
}

bool submit(Game& game) {
  if (game.state != State::Playing || !rowFull(game)) return false;
  const Rules r = rulesFor(game.mode);
  const int row = game.rowsUsed;
  for (int i = 0; i < r.pegs; ++i) game.guesses[row][i] = game.current[i];
  game.scores[row] = score(game.code, game.current, r.pegs);
  ++game.rowsUsed;
  clearRow(game);
  if (game.scores[row].black == r.pegs) {
    game.state = State::Won;
  } else if (game.rowsUsed >= r.rows) {
    game.state = State::Lost;
  }
  return true;
}

}  // namespace mastermind
