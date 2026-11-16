#include "KakuroCore.h"

#include "KakuroPuzzles.h"

namespace kakuro {

int sideOf(const Size size) {
  switch (size) {
    case Size::Small:
      return kSmallSide;
    case Size::Medium:
      return kMediumSide;
    case Size::Large:
      return kLargeSide;
  }
  return kSmallSide;
}

int countOf(const Size size) {
  switch (size) {
    case Size::Small:
      return kSmallCount;
    case Size::Medium:
      return kMediumCount;
    case Size::Large:
      return kLargeCount;
  }
  return 0;
}

namespace {

const uint8_t* entryOf(const Size size, const int index) {
  switch (size) {
    case Size::Small:
      return kSmallPuzzles[index];
    case Size::Medium:
      return kMediumPuzzles[index];
    case Size::Large:
      return kLargePuzzles[index];
  }
  return kSmallPuzzles[0];
}

}  // namespace

int runCell(const Layout& l, const Run& run, const int i) { return run.start + i * (run.down ? l.side : 1); }

bool load(const Size size, const int index, Layout& out) {
  if (index < 0 || index >= countOf(size)) return false;
  out = Layout{};
  const int n = sideOf(size);
  out.side = static_cast<uint8_t>(n);
  const uint8_t* data = entryOf(size, index);
  for (int i = 0; i < n * n; ++i) out.solution[i] = (i % 2 == 0) ? (data[i / 2] & 0xF) : (data[i / 2] >> 4);
  for (int dir = 0; dir < 2; ++dir) {
    for (int a = 0; a < n; ++a) {
      for (int b = 0; b < n;) {
        const int cell = dir == 0 ? a * n + b : b * n + a;
        if (!out.solution[cell]) {
          ++b;
          continue;
        }
        Run run;
        run.start = static_cast<uint8_t>(cell);
        run.down = dir == 1;
        int sum = 0;
        while (b < n) {
          const int c = dir == 0 ? a * n + b : b * n + a;
          if (!out.solution[c]) break;
          sum += out.solution[c];
          if (dir == 0) {
            out.acrossRun[c] = static_cast<uint8_t>(out.runCount);
          } else {
            out.downRun[c] = static_cast<uint8_t>(out.runCount);
          }
          ++run.length;
          ++b;
        }
        run.sum = static_cast<uint8_t>(sum);
        // The clue sits on the block just before the run.
        const int clue = dir == 0 ? cell - 1 : cell - n;
        if (clue >= 0) {
          if (dir == 0) {
            out.acrossClue[clue] = run.sum;
          } else {
            out.downClue[clue] = run.sum;
          }
        }
        if (out.runCount < kMaxRuns) out.runs[out.runCount++] = run;
      }
    }
  }
  return true;
}

void setDigit(Progress& p, const int cell, const int digit) {
  p.entry[cell] = static_cast<uint8_t>(digit >= 1 && digit <= 9 ? digit : 0);
  p.notes[cell] = 0;
}

void toggleNote(Progress& p, const int cell, const int digit) {
  if (digit < 1 || digit > 9) return;
  p.notes[cell] ^= static_cast<uint16_t>(1u << digit);
}

void clearCell(Progress& p, const int cell) {
  p.entry[cell] = 0;
  p.notes[cell] = 0;
}

bool duplicate(const Layout& l, const Progress& p, const int cell) {
  const int d = p.entry[cell];
  if (!white(l, cell) || d == 0) return false;
  const int runs[2] = {l.acrossRun[cell], l.downRun[cell]};
  for (const int r : runs) {
    const Run& run = l.runs[r];
    for (int i = 0; i < run.length; ++i) {
      const int other = runCell(l, run, i);
      if (other != cell && p.entry[other] == d) return true;
    }
  }
  return false;
}

bool runWrong(const Layout& l, const Progress& p, const int run) {
  const Run& r = l.runs[run];
  // Only a full run is judged; a repeat in a run still being filled is the
  // underline's business, not the clue's.
  for (int i = 0; i < r.length; ++i)
    if (p.entry[runCell(l, r, i)] == 0) return false;
  int sum = 0;
  unsigned seen = 0;
  for (int i = 0; i < r.length; ++i) {
    const int d = p.entry[runCell(l, r, i)];
    if (seen & (1u << d)) return true;
    seen |= 1u << d;
    sum += d;
  }
  return sum != r.sum;
}

bool solved(const Layout& l, const Progress& p) {
  const int n = l.side * l.side;
  for (int i = 0; i < n; ++i)
    if (white(l, i) && p.entry[i] != l.solution[i]) return false;
  return true;
}

int filledCount(const Layout& l, const Progress& p) {
  int count = 0;
  const int n = l.side * l.side;
  for (int i = 0; i < n; ++i)
    if (white(l, i) && p.entry[i] != 0) ++count;
  return count;
}

int whiteCount(const Layout& l) {
  int count = 0;
  const int n = l.side * l.side;
  for (int i = 0; i < n; ++i)
    if (white(l, i)) ++count;
  return count;
}

int combinations(const int sum, const int length, uint16_t* out, const int max) {
  int found = 0;
  // Every subset of 1-9, in ascending order of its bitmask's digits.
  for (unsigned mask = 0; mask < 512; ++mask) {
    if (__builtin_popcount(mask) != length) continue;
    int s = 0;
    for (int d = 1; d <= 9; ++d)
      if (mask & (1u << (d - 1))) s += d;
    if (s != sum) continue;
    if (found < max) out[found] = static_cast<uint16_t>(mask << 1);
    ++found;
  }
  return found;
}

}  // namespace kakuro
