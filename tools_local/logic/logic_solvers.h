#pragma once

// Solvers for the four grid puzzles, for the generator and the host tests only:
// the device never solves, it only checks. Each counts solutions up to a limit
// and hands back the first, so "exactly one" is a number rather than a hope.
//
// A puzzle is a w x h grid of characters:
//   Slitherlink  '.' no clue, '0'..'3' lines around the cell
//   Shikaku      '.' or a clue, the area as char('0' + n)
//   Akari        '.' white, '#' black, '0'..'4' black with a count
//   Hashi        '.' or '1'..'8', an island and its bridges

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace logicsolve {

struct P {
  int w = 0, h = 0;
  std::string c;
  char at(int x, int y) const { return c[y * w + x]; }
};

// ---------------- Shikaku ----------------
struct Rect {
  int x, y, w, h;
};

inline int shikaku(const P& p, std::vector<Rect>* out, int limit = 2) {
  struct Clue {
    int x, y, n;
    std::vector<Rect> cands;
  };
  std::vector<Clue> clues;
  for (int y = 0; y < p.h; ++y)
    for (int x = 0; x < p.w; ++x)
      if (p.at(x, y) != '.') clues.push_back({x, y, p.at(x, y) - '0', {}});
  for (auto& cl : clues) {
    for (int rw = 1; rw <= cl.n; ++rw) {
      if (cl.n % rw) continue;
      const int rh = cl.n / rw;
      for (int rx = cl.x - rw + 1; rx <= cl.x; ++rx)
        for (int ry = cl.y - rh + 1; ry <= cl.y; ++ry) {
          if (rx < 0 || ry < 0 || rx + rw > p.w || ry + rh > p.h) continue;
          int inside = 0;
          for (int y = ry; y < ry + rh; ++y)
            for (int x = rx; x < rx + rw; ++x) inside += p.at(x, y) != '.';
          if (inside == 1) cl.cands.push_back({rx, ry, rw, rh});
        }
    }
  }
  std::vector<int> owner(p.w * p.h, -1);
  std::vector<char> used(clues.size(), 0);
  std::vector<Rect> placed(clues.size());
  int found = 0;
  std::function<void()> go = [&]() {
    if (found >= limit) return;
    int cell = -1;
    for (int i = 0; i < p.w * p.h; ++i)
      if (owner[i] < 0) {
        cell = i;
        break;
      }
    if (cell < 0) {
      if (found == 0 && out) *out = placed;
      ++found;
      return;
    }
    const int cx = cell % p.w, cy = cell / p.w;
    for (size_t k = 0; k < clues.size(); ++k) {
      if (used[k]) continue;
      for (const Rect& r : clues[k].cands) {
        if (cx < r.x || cy < r.y || cx >= r.x + r.w || cy >= r.y + r.h) continue;
        bool free = true;
        for (int y = r.y; y < r.y + r.h && free; ++y)
          for (int x = r.x; x < r.x + r.w; ++x)
            if (owner[y * p.w + x] >= 0) {
              free = false;
              break;
            }
        if (!free) continue;
        for (int y = r.y; y < r.y + r.h; ++y)
          for (int x = r.x; x < r.x + r.w; ++x) owner[y * p.w + x] = static_cast<int>(k);
        used[k] = 1;
        placed[k] = r;
        go();
        used[k] = 0;
        for (int y = r.y; y < r.y + r.h; ++y)
          for (int x = r.x; x < r.x + r.w; ++x) owner[y * p.w + x] = -1;
        if (found >= limit) return;
      }
    }
  };
  go();
  return found;
}

// ---------------- Akari ----------------
inline bool akariWhite(char ch) { return ch == '.'; }

// `out` gets '1' where a bulb stands. With `rng`, branches are tried in a
// random order, which is how the generator finds a lighting to number.
inline int akari(const P& p, std::string* out, int limit = 2, std::mt19937* rng = nullptr) {
  const int n = p.w * p.h;
  std::vector<std::vector<int>> vis(n);
  for (int y = 0; y < p.h; ++y)
    for (int x = 0; x < p.w; ++x) {
      if (!akariWhite(p.at(x, y))) continue;
      static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int d = 0; d < 4; ++d) {
        int nx = x + dx[d], ny = y + dy[d];
        while (nx >= 0 && ny >= 0 && nx < p.w && ny < p.h && akariWhite(p.at(nx, ny))) {
          vis[y * p.w + x].push_back(ny * p.w + nx);
          nx += dx[d];
          ny += dy[d];
        }
      }
    }
  auto neighbours = [&](int i) {
    std::vector<int> v;
    const int x = i % p.w, y = i / p.w;
    if (x > 0 && akariWhite(p.at(x - 1, y))) v.push_back(i - 1);
    if (x + 1 < p.w && akariWhite(p.at(x + 1, y))) v.push_back(i + 1);
    if (y > 0 && akariWhite(p.at(x, y - 1))) v.push_back(i - p.w);
    if (y + 1 < p.h && akariWhite(p.at(x, y + 1))) v.push_back(i + p.w);
    return v;
  };
  int found = 0;
  // 0 unknown, 1 bulb, 2 no bulb
  std::function<bool(std::vector<int8_t>&)> propagate = [&](std::vector<int8_t>& s) {
    bool changed = true;
    while (changed) {
      changed = false;
      for (int i = 0; i < n; ++i) {
        const char ch = p.c[i];
        if (akariWhite(ch)) {
          if (s[i] == 1) {
            for (int v : vis[i]) {
              if (s[v] == 1) return false;
              if (s[v] == 0) {
                s[v] = 2;
                changed = true;
              }
            }
          }
          bool lit = s[i] == 1;
          for (int v : vis[i]) lit = lit || s[v] == 1;
          if (!lit) {
            int cands = s[i] == 0, last = s[i] == 0 ? i : -1;
            for (int v : vis[i])
              if (s[v] == 0) {
                ++cands;
                last = v;
              }
            if (cands == 0) return false;
            if (cands == 1) {
              s[last] = 1;
              changed = true;
            }
          }
        } else if (ch >= '0' && ch <= '4') {
          const int want = ch - '0';
          int b = 0, u = 0;
          const auto nb = neighbours(i);
          for (int v : nb) {
            b += s[v] == 1;
            u += s[v] == 0;
          }
          if (b > want || b + u < want) return false;
          if (u > 0 && (b == want || b + u == want)) {
            for (int v : nb)
              if (s[v] == 0) s[v] = b == want ? 2 : 1;
            changed = true;
          }
        }
      }
    }
    return true;
  };
  std::function<void(std::vector<int8_t>)> go = [&](std::vector<int8_t> s) {
    if (found >= limit || !propagate(s)) return;
    int pick = -1;
    for (int i = 0; i < n; ++i)
      if (akariWhite(p.c[i]) && s[i] == 0) {
        pick = i;
        break;
      }
    if (pick < 0) {
      if (found == 0 && out) {
        out->assign(n, '0');
        for (int i = 0; i < n; ++i)
          if (s[i] == 1) (*out)[i] = '1';
      }
      ++found;
      return;
    }
    int first = 1;
    if (rng) first = ((*rng)() & 3) == 0 ? 1 : 2;
    for (int k = 0; k < 2; ++k) {
      std::vector<int8_t> t = s;
      t[pick] = static_cast<int8_t>(k == 0 ? first : 3 - first);
      go(t);
      if (found >= limit) return;
    }
  };
  go(std::vector<int8_t>(n, 0));
  return found;
}

// ---------------- Hashi ----------------
struct Island {
  int x, y, n;
};
struct Bridge {
  int a, b;  // island indices; a is left of or above b
  int count;
};

inline void hashiLayout(const P& p, std::vector<Island>& islands, std::vector<Bridge>& edges) {
  islands.clear();
  edges.clear();
  std::vector<int> at(p.w * p.h, -1);
  for (int y = 0; y < p.h; ++y)
    for (int x = 0; x < p.w; ++x)
      if (p.at(x, y) != '.') {
        at[y * p.w + x] = static_cast<int>(islands.size());
        islands.push_back({x, y, p.at(x, y) - '0'});
      }
  for (size_t i = 0; i < islands.size(); ++i) {
    for (int x = islands[i].x + 1; x < p.w; ++x)
      if (at[islands[i].y * p.w + x] >= 0) {
        edges.push_back({static_cast<int>(i), at[islands[i].y * p.w + x], 0});
        break;
      }
    for (int y = islands[i].y + 1; y < p.h; ++y)
      if (at[y * p.w + islands[i].x] >= 0) {
        edges.push_back({static_cast<int>(i), at[y * p.w + islands[i].x], 0});
        break;
      }
  }
}

inline bool hashiCross(const std::vector<Island>& is, const Bridge& e, const Bridge& f) {
  const bool eh = is[e.a].y == is[e.b].y, fh = is[f.a].y == is[f.b].y;
  if (eh == fh) return false;
  const Bridge& hz = eh ? e : f;
  const Bridge& vt = eh ? f : e;
  return is[vt.a].x > is[hz.a].x && is[vt.a].x < is[hz.b].x && is[hz.a].y > is[vt.a].y && is[hz.a].y < is[vt.b].y;
}

inline int hashi(const P& p, std::vector<Bridge>* out, int limit = 2) {
  std::vector<Island> is;
  std::vector<Bridge> ed;
  hashiLayout(p, is, ed);
  const int m = static_cast<int>(ed.size());
  std::vector<int> sum(is.size(), 0), left(is.size(), 0);
  for (const auto& e : ed) {
    ++left[e.a];
    ++left[e.b];
  }
  int found = 0;
  long nodes = 0;
  std::function<void(int)> go = [&](int k) {
    if (found >= limit || ++nodes > 4000000) return;
    if (k == m) {
      std::vector<int> root(is.size());
      for (size_t i = 0; i < is.size(); ++i) root[i] = static_cast<int>(i);
      std::function<int(int)> find = [&](int v) { return root[v] == v ? v : root[v] = find(root[v]); };
      for (const auto& e : ed)
        if (e.count > 0) root[find(e.a)] = find(e.b);
      for (size_t i = 1; i < is.size(); ++i)
        if (find(static_cast<int>(i)) != find(0)) return;
      if (found == 0 && out) *out = ed;
      ++found;
      return;
    }
    Bridge& e = ed[k];
    --left[e.a];
    --left[e.b];
    for (int v = 2; v >= 0; --v) {
      if (sum[e.a] + v > is[e.a].n || sum[e.b] + v > is[e.b].n) continue;
      if (sum[e.a] + v + 2 * left[e.a] < is[e.a].n || sum[e.b] + v + 2 * left[e.b] < is[e.b].n) continue;
      if (v > 0) {
        bool cross = false;
        for (int j = 0; j < k && !cross; ++j) cross = ed[j].count > 0 && hashiCross(is, e, ed[j]);
        if (cross) continue;
      }
      e.count = v;
      sum[e.a] += v;
      sum[e.b] += v;
      go(k + 1);
      sum[e.a] -= v;
      sum[e.b] -= v;
      e.count = 0;
      if (found >= limit) break;
    }
    ++left[e.a];
    ++left[e.b];
  };
  go(0);
  // Out of budget counts as "not known to be unique".
  return nodes > 4000000 ? limit : found;
}

// ---------------- Slitherlink ----------------
// Solved over cells: the loop is the edge of a set of "inside" cells that is
// connected, whose outside reaches the border, and that never touches itself
// corner to corner. `out` gets '1' for inside.
inline bool slitherShapeOk(int w, int h, const std::string& in) {
  // No 2x2 checkerboard, inside connected, outside connected to the border.
  for (int y = 0; y + 1 < h; ++y)
    for (int x = 0; x + 1 < w; ++x) {
      const char a = in[y * w + x], b = in[y * w + x + 1], c = in[(y + 1) * w + x], d = in[(y + 1) * w + x + 1];
      if (a == d && b == c && a != b) return false;
    }
  auto flood = [&](char want, bool fromBorder) {
    std::vector<char> seen(w * h, 0);
    std::vector<int> stack;
    int total = 0;
    for (int i = 0; i < w * h; ++i) total += in[i] == want;
    if (total == 0) return !fromBorder ? false : true;
    for (int i = 0; i < w * h; ++i) {
      if (in[i] != want) continue;
      const int x = i % w, y = i / w;
      if (fromBorder && !(x == 0 || y == 0 || x == w - 1 || y == h - 1)) continue;
      seen[i] = 1;
      stack.push_back(i);
      if (!fromBorder) break;
    }
    int got = 0;
    while (!stack.empty()) {
      const int i = stack.back();
      stack.pop_back();
      ++got;
      const int x = i % w, y = i / w;
      const int nb[4] = {x > 0 ? i - 1 : -1, x + 1 < w ? i + 1 : -1, y > 0 ? i - w : -1, y + 1 < h ? i + w : -1};
      for (int v : nb)
        if (v >= 0 && !seen[v] && in[v] == want) {
          seen[v] = 1;
          stack.push_back(v);
        }
    }
    return got == total;
  };
  return flood('1', false) && flood('0', true);
}

inline int slitherClue(int w, int h, const std::string& in, int x, int y) {
  const char me = in[y * w + x];
  int n = 0;
  n += (x > 0 ? in[y * w + x - 1] : '0') != me;
  n += (x + 1 < w ? in[y * w + x + 1] : '0') != me;
  n += (y > 0 ? in[(y - 1) * w + x] : '0') != me;
  n += (y + 1 < h ? in[(y + 1) * w + x] : '0') != me;
  return n;
}

inline int slither(const P& p, std::string* out, int limit = 2, long budget = 3000000) {
  const int w = p.w, h = p.h;
  std::string in(w * h, '0');
  int found = 0;
  long nodes = 0;
  auto clueOk = [&](int x, int y) {
    const char ch = p.at(x, y);
    return ch == '.' || slitherClue(w, h, in, x, y) == ch - '0';
  };
  std::function<void(int)> go = [&](int i) {
    if (found >= limit || ++nodes > budget) return;
    if (i == w * h) {
      for (int x = 0; x < w; ++x)
        if (!clueOk(x, h - 1)) return;
      if (!slitherShapeOk(w, h, in)) return;
      if (found == 0 && out) *out = in;
      ++found;
      return;
    }
    const int x = i % w, y = i / w;
    for (char v = '0'; v <= '1'; ++v) {
      in[i] = v;
      bool ok = true;
      if (x > 0 && y > 0) {
        const char a = in[i - w - 1], b = in[i - w], c = in[i - 1];
        if (a == v && b == c && a != b) ok = false;
      }
      // The cell above-left has all four neighbours now; so does the one
      // above once the row's last cell is placed.
      if (ok && y > 0 && x > 0 && !clueOk(x - 1, y - 1)) ok = false;
      if (ok && y > 0 && x == w - 1 && !clueOk(x, y - 1)) ok = false;
      if (ok) go(i + 1);
      if (found >= limit) break;
    }
    in[i] = '0';
  };
  go(0);
  return nodes > budget ? limit : found;
}

}  // namespace logicsolve
