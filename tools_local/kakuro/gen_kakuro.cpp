// Generates the Kakuro bank: src/apps_local/kakuro/KakuroPuzzles.h.
//
// Every puzzle is CONSTRUCTED and then PROVED, never sampled and hoped for
// (the same stance as tools_local/picross/gen_picross.py):
//
//   1. A shape. Row 0 and column 0 are blocks (they carry the clues). The rest
//      is a random pattern, symmetric under a half turn, in which every white
//      cell sits in an across run AND a down run of 2-9 cells, and all white
//      cells are connected.
//   2. A fill, by randomised backtracking so no run repeats a digit, biased so
//      runs lean low or high: extreme sums have few combinations, which is
//      what makes a Kakuro solvable by reasoning.
//   3. The proof and the repair. A solver that only deduces -- for every run,
//      the digits that some valid assignment of that run still allows each
//      cell, iterated across all runs until nothing changes -- runs from a
//      blank grid. While it leaves cells open, one open cell's digit is
//      changed and the change is kept if it leaves no more cells open. When it
//      fixes every cell, the puzzle is unique and solvable without guessing:
//      a solver that only ever deduces has shown every cell was forced.
//
// The clues are derived from the fill, so they cannot disagree with it.
// host-tests/kakuro counts solutions for every shipped puzzle with an
// independent exhaustive search and requires exactly one.
//
//   c++ -O2 -std=c++17 tools_local/kakuro/gen_kakuro.cpp -o /tmp/gen_kakuro
//   /tmp/gen_kakuro > src/apps_local/kakuro/KakuroPuzzles.h
//
// Deterministic: a fixed seed per size, so re-running writes the same bank.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

namespace {

struct Rng {
  uint64_t s;
  uint32_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return static_cast<uint32_t>(s >> 11);
  }
  int below(const int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
  double unit() { return (next() & 0xFFFFFF) / static_cast<double>(0x1000000); }
};

constexpr int kMax = 10;

struct Board {
  int n = 0;
  bool white[kMax][kMax] = {};
  std::vector<std::vector<int>> runs;  // cell indices r*n+c
  std::vector<int> cellRuns[kMax * kMax];
  std::vector<int> cells;
};

int popcount(unsigned v) { return __builtin_popcount(v); }

void buildRuns(Board& b) {
  const int n = b.n;
  b.runs.clear();
  b.cells.clear();
  for (auto& v : b.cellRuns) v.clear();
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n;) {
      if (!b.white[r][c]) {
        ++c;
        continue;
      }
      std::vector<int> run;
      while (c < n && b.white[r][c]) run.push_back(r * n + c++);
      b.runs.push_back(run);
    }
  }
  for (int c = 0; c < n; ++c) {
    for (int r = 0; r < n;) {
      if (!b.white[r][c]) {
        ++r;
        continue;
      }
      std::vector<int> run;
      while (r < n && b.white[r][c]) run.push_back(r++ * n + c);
      b.runs.push_back(run);
    }
  }
  for (int i = 0; i < static_cast<int>(b.runs.size()); ++i)
    for (const int cell : b.runs[i]) b.cellRuns[cell].push_back(i);
  for (int i = 0; i < n * n; ++i)
    if (b.white[i / n][i % n]) b.cells.push_back(i);
}

bool connected(const Board& b) {
  const int n = b.n;
  if (b.cells.empty()) return false;
  std::vector<bool> seen(n * n, false);
  std::vector<int> stack{b.cells[0]};
  seen[b.cells[0]] = true;
  int count = 0;
  while (!stack.empty()) {
    const int at = stack.back();
    stack.pop_back();
    ++count;
    const int r = at / n, c = at % n;
    const int dr[4] = {1, -1, 0, 0}, dc[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; ++k) {
      const int rr = r + dr[k], cc = c + dc[k];
      if (rr < 0 || cc < 0 || rr >= n || cc >= n || !b.white[rr][cc]) continue;
      if (!seen[rr * n + cc]) {
        seen[rr * n + cc] = true;
        stack.push_back(rr * n + cc);
      }
    }
  }
  return count == static_cast<int>(b.cells.size());
}

bool shape(Board& b, const int n, const double share, Rng& rng) {
  for (int attempt = 0; attempt < 20000; ++attempt) {
    b.n = n;
    for (int r = 0; r < n; ++r)
      for (int c = 0; c < n; ++c) b.white[r][c] = r > 0 && c > 0;
    const int interior = (n - 1) * (n - 1);
    const int target = static_cast<int>(interior * share);
    int blocks = 0;
    while (blocks < target) {
      const int r = 1 + rng.below(n - 1), c = 1 + rng.below(n - 1);
      if (!b.white[r][c]) continue;
      b.white[r][c] = false;
      ++blocks;
      if (b.white[n - r][n - c]) {
        b.white[n - r][n - c] = false;
        ++blocks;
      }
    }
    // Close every cell that is not in both an across and a down run of 2+.
    for (bool changed = true; changed;) {
      changed = false;
      for (int r = 1; r < n; ++r) {
        for (int c = 1; c < n; ++c) {
          if (!b.white[r][c]) continue;
          const bool across = b.white[r][c - 1] || (c + 1 < n && b.white[r][c + 1]);
          const bool down = b.white[r - 1][c] || (r + 1 < n && b.white[r + 1][c]);
          if (!(across && down)) {
            b.white[r][c] = false;
            b.white[n - r][n - c] = false;
            changed = true;
          }
        }
      }
    }
    buildRuns(b);
    if (static_cast<int>(b.cells.size()) < interior * (1.0 - share - 0.12)) continue;
    bool tooLong = false;
    for (const auto& run : b.runs) tooLong |= run.size() > 9;
    if (tooLong || !connected(b)) continue;
    return true;
  }
  return false;
}

bool fillBiased(const Board& b, Rng& rng, int* value) {
  const int nr = static_cast<int>(b.runs.size());
  std::vector<int> pol(nr);
  for (int i = 0; i < nr; ++i) pol[i] = rng.below(2) ? 1 : -1;
  for (int i = 0; i < b.n * b.n; ++i) value[i] = 0;
  std::vector<int> order = b.cells;
  // Depth-first over cells in order, with a step limit so one bad shape
  // cannot stall the run.
  long steps = 0;
  std::vector<std::vector<int>> choices(order.size());
  int k = 0;
  std::vector<int> pos(order.size(), 0);
  auto candidates = [&](const int cell) {
    unsigned used = 0;
    for (const int i : b.cellRuns[cell])
      for (const int x : b.runs[i])
        if (value[x]) used |= 1u << value[x];
    int p = 0;
    for (const int i : b.cellRuns[cell]) p += pol[i];
    std::vector<std::pair<double, int>> scored;
    for (int d = 1; d <= 9; ++d) {
      if (used & (1u << d)) continue;
      double key = p < 0 ? d : (p > 0 ? -d : (d > 5 ? d - 5 : 5 - d));
      key += rng.unit() * 2.5;
      scored.push_back({key, d});
    }
    std::sort(scored.begin(), scored.end());
    std::vector<int> out;
    for (auto& s : scored) out.push_back(s.second);
    return out;
  };
  choices[0] = candidates(order[0]);
  while (k >= 0 && k < static_cast<int>(order.size())) {
    if (++steps > 200000) return false;
    const int cell = order[k];
    if (pos[k] >= static_cast<int>(choices[k].size())) {
      value[cell] = 0;
      pos[k] = 0;
      --k;
      if (k >= 0) {
        value[order[k]] = 0;
        ++pos[k];
      }
      continue;
    }
    value[cell] = choices[k][pos[k]];
    ++k;
    if (k < static_cast<int>(order.size())) {
      choices[k] = candidates(order[k]);
      pos[k] = 0;
    }
  }
  return k == static_cast<int>(order.size());
}

// Generalised arc consistency per run, iterated. dom[cell] is a bitmask of
// digits (bit d). Returns false on a contradiction.
bool deduce(const Board& b, const int* sums, unsigned* dom) {
  for (const int cell : b.cells) dom[cell] = 0x3FE;
  for (bool changed = true; changed;) {
    changed = false;
    for (int i = 0; i < static_cast<int>(b.runs.size()); ++i) {
      const auto& run = b.runs[i];
      const int len = static_cast<int>(run.size());
      unsigned allowed[9] = {};
      int vals[9];
      // Depth-first over the run's cells.
      struct Local {
        static void go(const std::vector<int>& run, const unsigned* dom, int k, int len, unsigned used, int sum,
                       int target, int* vals, unsigned* allowed) {
          if (k == len) {
            if (sum == target)
              for (int j = 0; j < len; ++j) allowed[j] |= 1u << vals[j];
            return;
          }
          const int rem = len - k - 1;
          for (int d = 1; d <= 9; ++d) {
            if (!(dom[run[k]] & (1u << d)) || (used & (1u << d))) continue;
            const int s = sum + d;
            if (s + rem * (rem + 1) / 2 > target) break;
            int maxRest = 0;
            for (int q = 0; q < rem; ++q) maxRest += 9 - q;
            if (s + maxRest < target) continue;
            vals[k] = d;
            go(run, dom, k + 1, len, used | (1u << d), s, target, vals, allowed);
          }
        }
      };
      Local::go(run, dom, 0, len, 0, 0, sums[i], vals, allowed);
      for (int j = 0; j < len; ++j) {
        if (!allowed[j]) return false;
        if (allowed[j] != dom[run[j]]) {
          dom[run[j]] = allowed[j];
          changed = true;
        }
      }
    }
  }
  return true;
}

int openCells(const Board& b, const int* value, unsigned* dom) {
  std::vector<int> sums(b.runs.size());
  for (size_t i = 0; i < b.runs.size(); ++i) {
    int s = 0;
    for (const int x : b.runs[i]) s += value[x];
    sums[i] = s;
  }
  if (!deduce(b, sums.data(), dom)) return 1 << 20;
  int open = 0;
  for (const int cell : b.cells) open += popcount(dom[cell]) - 1;
  return open;
}

bool make(Board& b, const int n, const double share, Rng& rng, int* value) {
  for (;;) {
    if (!shape(b, n, share, rng)) continue;
    if (!fillBiased(b, rng, value)) continue;
    unsigned dom[kMax * kMax];
    int best = openCells(b, value, dom);
    for (int step = 0; step < 3000 && best > 0; ++step) {
      std::vector<int> open;
      for (const int cell : b.cells)
        if (popcount(dom[cell]) > 1) open.push_back(cell);
      const int cell = open[rng.below(static_cast<int>(open.size()))];
      unsigned used = 0;
      for (const int i : b.cellRuns[cell])
        for (const int x : b.runs[i])
          if (x != cell) used |= 1u << value[x];
      std::vector<int> choices;
      for (int d = 1; d <= 9; ++d)
        if (!(used & (1u << d)) && d != value[cell]) choices.push_back(d);
      if (choices.empty()) continue;
      const int old = value[cell];
      value[cell] = choices[rng.below(static_cast<int>(choices.size()))];
      unsigned trial[kMax * kMax];
      const int score = openCells(b, value, trial);
      if (score <= best) {
        best = score;
        for (const int c : b.cells) dom[c] = trial[c];
      } else {
        value[cell] = old;
      }
    }
    if (best == 0) {
      // The deduced grid is the fill: every domain is the single digit placed.
      for (const int cell : b.cells)
        if (dom[cell] != (1u << value[cell])) return false;
      return true;
    }
  }
}

}  // namespace

int main() {
  struct Size {
    const char* name;
    int n;
    int count;
    uint64_t seed;
    double share;
  };
  const Size sizes[] = {{"Small", 6, 60, 6001, 0.20}, {"Medium", 8, 60, 8001, 0.27}, {"Large", 10, 24, 10001, 0.32}};
  std::printf(
      "#pragma once\n\n"
      "// Every Kakuro puzzle, as flash-resident data.\n"
      "//\n"
      "// GENERATED by tools_local/kakuro/gen_kakuro.cpp. Do not edit by hand.\n"
      "//\n"
      "// Each puzzle is its SOLUTION: side*side cells, row-major, two per byte\n"
      "// (low nibble first), 0 for a block and 1-9 for a white cell's digit. The\n"
      "// clues are derived from it (KakuroCore), so they cannot disagree with it.\n"
      "// Every puzzle was proved unique and solvable by deduction alone before it\n"
      "// was written here, and host-tests/kakuro counts its solutions again.\n\n"
      "#include <cstdint>\n\n"
      "namespace kakuro {\n\n");
  for (const Size& s : sizes) {
    Rng rng{s.seed * 0x9E3779B97F4A7C15ull + 1};
    std::set<std::vector<uint8_t>> seen;
    std::vector<std::vector<uint8_t>> out;
    while (static_cast<int>(out.size()) < s.count) {
      Board b;
      int value[kMax * kMax];
      if (!make(b, s.n, s.share, rng, value)) continue;
      std::vector<uint8_t> bytes;
      for (int i = 0; i < s.n * s.n; i += 2) {
        const int lo = b.white[i / s.n][i % s.n] ? value[i] : 0;
        const int hi = (i + 1 < s.n * s.n && b.white[(i + 1) / s.n][(i + 1) % s.n]) ? value[i + 1] : 0;
        bytes.push_back(static_cast<uint8_t>(lo | (hi << 4)));
      }
      if (!seen.insert(bytes).second) continue;
      out.push_back(bytes);
      std::fprintf(stderr, "\r%s %d/%d", s.name, static_cast<int>(out.size()), s.count);
    }
    std::fprintf(stderr, "\n");
    std::printf("constexpr int k%sSide = %d;\n", s.name, s.n);
    std::printf("constexpr int k%sCount = %d;\n", s.name, s.count);
    std::printf("constexpr int k%sStride = %d;\n", s.name, static_cast<int>(out[0].size()));
    std::printf("static const uint8_t k%sPuzzles[k%sCount][k%sStride] = {\n", s.name, s.name, s.name);
    for (const auto& p : out) {
      std::printf("    {");
      for (size_t i = 0; i < p.size(); ++i) std::printf("%s0x%02X", i ? ", " : "", p[i]);
      std::printf("},\n");
    }
    std::printf("};\n\n");
  }
  std::printf("}  // namespace kakuro\n");
  return 0;
}
