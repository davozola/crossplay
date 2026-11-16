#include "TriplesCore.h"

namespace triples {

namespace {

// The four cells of line `i` for a swipe, the edge the tiles move toward first.
void lineOf(const Dir dir, const int i, int out[kSide]) {
  for (int k = 0; k < kSide; ++k) {
    switch (dir) {
      case Dir::Left:
        out[k] = i * kSide + k;
        break;
      case Dir::Right:
        out[k] = i * kSide + (kSide - 1 - k);
        break;
      case Dir::Up:
        out[k] = k * kSide + i;
        break;
      case Dir::Down:
        out[k] = (kSide - 1 - k) * kSide + i;
        break;
    }
  }
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

uint32_t tileScore(const uint16_t value) {
  if (value < 3) return 0;
  uint32_t points = 3;
  for (uint16_t v = value; v > 3; v = static_cast<uint16_t>(v / 2)) points *= 3;
  return points;
}

bool merges(const uint16_t a, const uint16_t b) {
  if (a == 0 || b == 0) return false;
  if (a + b == 3) return true;
  return a >= 3 && a == b;
}

uint16_t Game::draw() {
  // Now and then, once the board has grown, a bigger tile comes in instead of
  // one from the stack: anything from 6 up to an eighth of the highest.
  const uint16_t top = highest();
  if (top >= 48 && rng.below(21) == 0) {
    int steps = 0;
    for (uint16_t v = static_cast<uint16_t>(top / 8); v > 6; v = static_cast<uint16_t>(v / 2)) ++steps;
    uint16_t bonus = 6;
    for (int k = rng.below(steps + 1); k > 0; --k) bonus = static_cast<uint16_t>(bonus * 2);
    return bonus;
  }
  if (stackLeft == 0) {
    for (int i = 0; i < 12; ++i) stack[i] = static_cast<uint8_t>(1 + i / 4);
    for (int i = 11; i > 0; --i) {
      const int j = rng.below(i + 1);
      const uint8_t t = stack[i];
      stack[i] = stack[j];
      stack[j] = t;
    }
    stackLeft = 12;
  }
  return stack[--stackLeft];
}

void Game::start(const uint32_t seed) {
  rng.state = seed;
  stackLeft = 0;
  for (int i = 0; i < kCells; ++i) cells[i] = 0;
  // Nine tiles to begin with, anywhere.
  for (int placed = 0; placed < 9;) {
    const int at = rng.below(kCells);
    if (cells[at] != 0) continue;
    cells[at] = draw();
    ++placed;
  }
  nextTile = draw();
}

bool Game::slide(const int line[kSide], const bool apply) {
  for (int k = 0; k + 1 < kSide; ++k) {
    const uint16_t here = cells[line[k]];
    const uint16_t behind = cells[line[k + 1]];
    if (behind == 0) continue;
    if (here != 0 && !merges(here, behind)) continue;
    // The tile behind moves in (or merges), and everything after it follows.
    if (apply) {
      cells[line[k]] = static_cast<uint16_t>(here + behind);
      for (int j = k + 1; j + 1 < kSide; ++j) cells[line[j]] = cells[line[j + 1]];
      cells[line[kSide - 1]] = 0;
    }
    return true;
  }
  return false;
}

bool Game::canMove(const Dir dir) const {
  Game probe = *this;
  for (int i = 0; i < kSide; ++i) {
    int line[kSide];
    lineOf(dir, i, line);
    if (probe.slide(line, false)) return true;
  }
  return false;
}

bool Game::move(const Dir dir, int* entered) {
  int moved[kSide];
  int movedCount = 0;
  for (int i = 0; i < kSide; ++i) {
    int line[kSide];
    lineOf(dir, i, line);
    if (slide(line, true)) moved[movedCount++] = i;
  }
  if (movedCount == 0) return false;
  // The new tile comes in at the far end of one of the lines that moved.
  int line[kSide];
  lineOf(dir, moved[rng.below(movedCount)], line);
  cells[line[kSide - 1]] = nextTile;
  if (entered) *entered = line[kSide - 1];
  nextTile = draw();
  return true;
}

bool Game::over() const {
  return !canMove(Dir::Left) && !canMove(Dir::Right) && !canMove(Dir::Up) && !canMove(Dir::Down);
}

uint32_t Game::score() const {
  uint32_t total = 0;
  for (int i = 0; i < kCells; ++i) total += tileScore(cells[i]);
  return total;
}

uint16_t Game::highest() const {
  uint16_t top = 0;
  for (int i = 0; i < kCells; ++i) {
    if (cells[i] > top) top = cells[i];
  }
  return top;
}

}  // namespace triples
