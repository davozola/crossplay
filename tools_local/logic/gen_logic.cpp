// Generates src/apps_local/logic/LogicPuzzles.h: Slitherlink, Shikaku, Akari
// and Hashi boards in three sizes each, every one checked to have exactly one
// solution by the solvers beside this file.
//
//   c++ -O2 -std=c++17 tools_local/logic/gen_logic.cpp -o /tmp/gen_logic
//   /tmp/gen_logic > src/apps_local/logic/LogicPuzzles.h

#include <algorithm>
#include <cstdio>

#include "logic_solvers.h"

using namespace logicsolve;

namespace {

std::mt19937 rng(20261116);
int below(int n) { return static_cast<int>(rng() % static_cast<unsigned>(n)); }

// ---- Shikaku: cut the grid into rectangles, put each one's area in it ----
void cut(int x, int y, int w, int h, int maxArea, std::vector<Rect>& out) {
  const int area = w * h;
  const bool mustSplit = area > maxArea || (w > 1 && h > 1 && (w > 2 * h + 1 || h > 2 * w + 1));
  if (!mustSplit && (area <= 3 || below(100) < (area <= 8 ? 50 : 10))) {
    out.push_back({x, y, w, h});
    return;
  }
  const bool vertical = w > h ? true : (h > w ? false : below(2) == 0);
  if (vertical && w > 1) {
    const int at = 1 + below(w - 1);
    cut(x, y, at, h, maxArea, out);
    cut(x + at, y, w - at, h, maxArea, out);
  } else if (h > 1) {
    const int at = 1 + below(h - 1);
    cut(x, y, w, at, maxArea, out);
    cut(x, y + at, w, h - at, maxArea, out);
  } else {
    out.push_back({x, y, w, h});
  }
}

bool makeShikaku(int size, P& p) {
  std::vector<Rect> rects;
  cut(0, 0, size, size, size + size / 2, rects);
  int ones = 0;
  for (const Rect& r : rects) ones += r.w * r.h == 1;
  if (ones > 1 || static_cast<int>(rects.size()) < size) return false;
  p.w = p.h = size;
  p.c.assign(size * size, '.');
  for (const Rect& r : rects) p.c[(r.y + below(r.h)) * size + r.x + below(r.w)] = static_cast<char>('0' + r.w * r.h);
  return shikaku(p, nullptr) == 1;
}

// ---- Akari: scatter walls, light it some way, number the walls, then take
// numbers away while the lighting stays the only one ----
bool makeAkari(int size, P& p) {
  p.w = p.h = size;
  p.c.assign(size * size, '.');
  const int walls = size * size * (18 + below(8)) / 100;
  for (int k = 0; k < walls / 2; ++k) {
    const int i = below(size * size);
    p.c[i] = '#';
    p.c[size * size - 1 - i] = '#';  // half-turn symmetry
  }
  std::string bulbs;
  if (akari(p, &bulbs, 1, &rng) != 1) return false;
  std::vector<int> wallCells;
  for (int i = 0; i < size * size; ++i) {
    if (p.c[i] != '#') continue;
    const int x = i % size, y = i / size;
    int n = 0;
    if (x > 0) n += bulbs[i - 1] == '1';
    if (x + 1 < size) n += bulbs[i + 1] == '1';
    if (y > 0) n += bulbs[i - size] == '1';
    if (y + 1 < size) n += bulbs[i + size] == '1';
    p.c[i] = static_cast<char>('0' + n);
    wallCells.push_back(i);
  }
  if (akari(p, nullptr) != 1) return false;
  std::shuffle(wallCells.begin(), wallCells.end(), rng);
  for (int i : wallCells) {
    const char keep = p.c[i];
    p.c[i] = '#';
    if (akari(p, nullptr) != 1) p.c[i] = keep;
  }
  int numbered = 0;
  for (char ch : p.c) numbered += ch >= '0' && ch <= '4';
  return numbered >= 2;
}

// ---- Hashi: grow a web of islands a bridge at a time ----
bool makeHashi(int size, P& p) {
  const int target = size * size / 5 + below(size / 2);
  std::vector<int> grid(size * size, 0);  // 0 free, 1 island, 2 bridge
  struct Is {
    int x, y, n;
  };
  std::vector<Is> is;
  is.push_back({below(size), below(size), 0});
  grid[is[0].y * size + is[0].x] = 1;
  static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
  for (int tries = 0; tries < 4000 && static_cast<int>(is.size()) < target; ++tries) {
    const int from = below(static_cast<int>(is.size()));
    const int d = below(4);
    const int len = 2 + below(size / 2);
    const int ex = is[from].x + dx[d] * len, ey = is[from].y + dy[d] * len;
    if (ex < 0 || ey < 0 || ex >= size || ey >= size) continue;
    bool clear = true;
    for (int k = 1; k < len && clear; ++k) clear = grid[(is[from].y + dy[d] * k) * size + is[from].x + dx[d] * k] == 0;
    if (!clear) continue;
    const int end = grid[ey * size + ex];
    if (end == 2) continue;
    // A new island must not sit right beside another: the gap is the bridge.
    if (end == 0) {
      bool crowded = false;
      for (int k = 0; k < 4; ++k) {
        const int nx = ex + dx[k], ny = ey + dy[k];
        if (nx < 0 || ny < 0 || nx >= size || ny >= size) continue;
        if (grid[ny * size + nx] == 1) crowded = true;
      }
      if (crowded) continue;
    }
    if (end == 1 && below(100) < 60) continue;  // loops, but not everywhere
    const int count = below(100) < 45 ? 2 : 1;
    int to = -1;
    if (end == 1) {
      for (size_t k = 0; k < is.size(); ++k)
        if (is[k].x == ex && is[k].y == ey) to = static_cast<int>(k);
    } else {
      to = static_cast<int>(is.size());
      is.push_back({ex, ey, 0});
      grid[ey * size + ex] = 1;
    }
    if (is[from].n + count > 8 || is[to].n + count > 8) continue;
    for (int k = 1; k < len; ++k) grid[(is[from].y + dy[d] * k) * size + is[from].x + dx[d] * k] = 2;
    is[from].n += count;
    is[to].n += count;
  }
  if (static_cast<int>(is.size()) < target - 1) return false;
  p.w = p.h = size;
  p.c.assign(size * size, '.');
  for (const Is& i : is) p.c[i.y * size + i.x] = static_cast<char>('0' + i.n);
  return hashi(p, nullptr) == 1;
}

// ---- Slitherlink: grow a blob, read its edge off as clues, then rub clues
// out while the loop stays the only one ----
bool makeSlither(int size, P& p) {
  std::string in(size * size, '0');
  in[below(size * size)] = '1';
  int inside = 1;
  const int want = size * size * (45 + below(15)) / 100;
  for (int tries = 0; tries < size * size * 200 && inside < want; ++tries) {
    const int i = below(size * size);
    if (in[i] == '1') continue;
    const int x = i % size, y = i / size;
    const bool touches = (x > 0 && in[i - 1] == '1') || (x + 1 < size && in[i + 1] == '1') ||
                         (y > 0 && in[i - size] == '1') || (y + 1 < size && in[i + size] == '1');
    if (!touches) continue;
    // Prefer cells with one inside neighbour: the blob grows arms, and the
    // loop winds instead of bulging into a field of zeros.
    const int around = (x > 0 && in[i - 1] == '1') + (x + 1 < size && in[i + 1] == '1') +
                       (y > 0 && in[i - size] == '1') + (y + 1 < size && in[i + size] == '1');
    if (around == 2 && below(100) >= 22) continue;
    if (around >= 3 && below(100) >= 3) continue;
    in[i] = '1';
    if (slitherShapeOk(size, size, in)) {
      ++inside;
    } else {
      in[i] = '0';
    }
  }
  if (inside < want * 8 / 10) return false;
  p.w = p.h = size;
  p.c.assign(size * size, '.');
  for (int i = 0; i < size * size; ++i)
    p.c[i] = static_cast<char>('0' + slitherClue(size, size, in, i % size, i / size));
  if (slither(p, nullptr) != 1) return false;
  std::vector<int> order(size * size);
  for (int i = 0; i < size * size; ++i) order[i] = i;
  std::shuffle(order.begin(), order.end(), rng);
  int removed = 0;
  const int maxRemoved = size * size * 42 / 100;
  for (int i : order) {
    if (removed >= maxRemoved) break;
    const char keep = p.c[i];
    p.c[i] = '.';
    if (slither(p, nullptr, 2, 400000) == 1) {
      ++removed;
    } else {
      p.c[i] = keep;
    }
  }
  return removed >= size * size / 5;
}

void emit(const char* name, const int sizes[3], int perSize, bool (*make)(int, P&)) {
  for (int s = 0; s < 3; ++s) {
    std::printf("inline constexpr const char* k%s%d[] = {\n", name, sizes[s]);
    int made = 0;
    std::vector<std::string> seen;
    while (made < perSize) {
      P p;
      if (!make(sizes[s], p)) continue;
      if (std::find(seen.begin(), seen.end(), p.c) != seen.end()) continue;
      seen.push_back(p.c);
      std::printf("    \"%s\",\n", p.c.c_str());
      ++made;
    }
    std::printf("};\n");
  }
}

}  // namespace

int main() {
  constexpr int kPerSize = 30;
  std::printf(
      "#pragma once\n\n"
      "// Generated by tools_local/logic/gen_logic.cpp. Do not edit: regenerate.\n"
      "// Every board has exactly one solution; host-tests/logic solves them all again.\n"
      "//\n"
      "// A board is a square grid of characters, row by row:\n"
      "//   Slitherlink  '.' no clue, '0'..'3' lines around the cell\n"
      "//   Shikaku      '.' or a clue, the area as char('0' + n)\n"
      "//   Akari        '.' white, '#' black, '0'..'4' black with a count\n"
      "//   Hashi        '.' or '1'..'8', an island and its bridges\n\n"
      "namespace logicpuzzles {\n\n"
      "constexpr int kPerSize = %d;\n\n",
      kPerSize);
  const int grid[3] = {6, 8, 10};
  const int web[3] = {7, 9, 11};
  emit("Slither", grid, kPerSize, makeSlither);
  emit("Shikaku", grid, kPerSize, makeShikaku);
  emit("Akari", grid, kPerSize, makeAkari);
  emit("Hashi", web, kPerSize, makeHashi);
  std::printf("\n}  // namespace logicpuzzles\n");
  return 0;
}
