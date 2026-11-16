#include <cstdio>
#include <cstdlib>

#include "KakuroCore.h"

using namespace kakuro;

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

// An exhaustive solution count, written independently of the generator's
// deductive solver: plain backtracking over the white cells with sum bounds.
struct Counter {
  const Layout& l;
  int cells[kMaxCells];
  int n = 0;
  int sum[kMaxRuns] = {};
  int filled[kMaxRuns] = {};
  unsigned used[kMaxRuns] = {};
  int found = 0;

  explicit Counter(const Layout& layout) : l(layout) {
    for (int i = 0; i < l.side * l.side; ++i)
      if (white(l, i)) cells[n++] = i;
  }

  bool fits(const int run, const int d) const {
    const Run& r = l.runs[run];
    if (used[run] & (1u << d)) return false;
    const int s = sum[run] + d;
    const int left = r.length - filled[run] - 1;
    int lo = 0, hi = 0;
    for (int k = 0; k < left; ++k) {
      lo += 1 + k;
      hi += 9 - k;
    }
    if (left == 0) return s == r.sum;
    return s + lo <= r.sum && s + hi >= r.sum;
  }

  void go(const int k) {
    if (found >= 2) return;
    if (k == n) {
      ++found;
      return;
    }
    const int cell = cells[k];
    const int a = l.acrossRun[cell], dn = l.downRun[cell];
    for (int d = 1; d <= 9; ++d) {
      if (!fits(a, d) || !fits(dn, d)) continue;
      sum[a] += d, sum[dn] += d, ++filled[a], ++filled[dn];
      used[a] |= 1u << d, used[dn] |= 1u << d;
      go(k + 1);
      sum[a] -= d, sum[dn] -= d, --filled[a], --filled[dn];
      used[a] &= ~(1u << d), used[dn] &= ~(1u << d);
    }
  }
};

static void everyPuzzleIsSound() {
  for (int s = 0; s < kSizeCount; ++s) {
    const Size size = static_cast<Size>(s);
    for (int i = 0; i < countOf(size); ++i) {
      Layout l;
      CHECK(load(size, i, l));
      const int side = l.side;
      // Clue row and column are blocks.
      for (int k = 0; k < side; ++k) CHECK(!white(l, k) && !white(l, k * side));
      // Every white cell sits in an across and a down run of 2 to 9, no repeats.
      for (int r = 0; r < l.runCount; ++r) {
        const Run& run = l.runs[r];
        CHECK(run.length >= 2 && run.length <= 9);
        unsigned seen = 0;
        for (int k = 0; k < run.length; ++k) {
          const int d = l.solution[runCell(l, run, k)];
          CHECK(!(seen & (1u << d)));
          seen |= 1u << d;
        }
      }
      Counter counter(l);
      counter.go(0);
      if (counter.found != 1) std::printf("  %d #%d has %d solution(s)\n", s, i, counter.found);
      CHECK(counter.found == 1);
    }
  }
}

static void progressChecks() {
  Layout l;
  CHECK(load(Size::Small, 0, l));
  Progress p;
  CHECK(!solved(l, p));
  int first = -1;
  for (int i = 0; i < l.side * l.side && first < 0; ++i)
    if (white(l, i)) first = i;
  const Run& run = l.runs[l.acrossRun[first]];
  const int second = runCell(l, run, 1);
  setDigit(p, first, 3);
  setDigit(p, second, 3);
  CHECK(duplicate(l, p, first) && duplicate(l, p, second));
  // A repeat in a run that is not full yet does not condemn the clue.
  if (run.length > 2) CHECK(!runWrong(l, p, l.acrossRun[first]));
  toggleNote(p, first, 5);
  CHECK(p.notes[first] == (1u << 5));
  setDigit(p, first, 4);
  CHECK(p.notes[first] == 0);
  CHECK(!duplicate(l, p, first));
  // Fill everything with the solution: solved, and no run is wrong.
  for (int i = 0; i < l.side * l.side; ++i)
    if (white(l, i)) setDigit(p, i, l.solution[i]);
  CHECK(solved(l, p));
  for (int r = 0; r < l.runCount; ++r) CHECK(!runWrong(l, p, r));
  CHECK(filledCount(l, p) == whiteCount(l));
  // One wrong digit makes its runs wrong and the puzzle unsolved.
  setDigit(p, first, l.solution[first] == 9 ? 8 : l.solution[first] + 1);
  CHECK(!solved(l, p));
  CHECK(runWrong(l, p, l.acrossRun[first]));
}

static void combos() {
  uint16_t out[16];
  CHECK(combinations(16, 2, out, 16) == 1 && out[0] == ((1u << 7) | (1u << 9)));
  CHECK(combinations(3, 2, out, 16) == 1);
  CHECK(combinations(45, 9, out, 16) == 1);
  CHECK(combinations(10, 2, out, 16) == 4);
  CHECK(combinations(2, 2, out, 16) == 0);
}

int main() {
  combos();
  progressChecks();
  everyPuzzleIsSound();
  if (failures) {
    std::printf("%d kakuro check(s) failed\n", failures);
    return EXIT_FAILURE;
  }
  std::printf("kakuro: all checks passed\n");
  return EXIT_SUCCESS;
}
