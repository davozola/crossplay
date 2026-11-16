#include "LogicCore.h"

#include <cstring>

#include "LogicPuzzles.h"

namespace logic {

namespace {

constexpr int kGridSides[kSizes] = {6, 8, 10};
constexpr int kWebSides[kSizes] = {7, 9, 11};

const char* const* tableOf(const Game game, const int size) {
  using namespace logicpuzzles;
  static const char* const* const kTables[kGames][kSizes] = {
      {kSlither6, kSlither8, kSlither10},
      {kShikaku6, kShikaku8, kShikaku10},
      {kAkari6, kAkari8, kAkari10},
      {kHashi7, kHashi9, kHashi11},
  };
  return kTables[static_cast<int>(game)][size];
}

int clampSize(const int size) { return size < 0 ? 0 : (size >= kSizes ? kSizes - 1 : size); }

}  // namespace

const char* gameName(const Game game) {
  switch (game) {
    case Game::Shikaku:
      return "SHIKAKU";
    case Game::Akari:
      return "AKARI";
    case Game::Hashi:
      return "HASHI";
    case Game::Slither:
    default:
      return "SLITHERLINK";
  }
}

const char* gameRule(const Game game) {
  switch (game) {
    case Game::Shikaku:
      return "CUT THE GRID INTO RECTANGLES. EACH HOLDS ONE NUMBER: ITS AREA. TAP TWO CORNERS.";
    case Game::Akari:
      return "LIGHT EVERY CELL. NO BULB MAY SHINE ON ANOTHER. A NUMBER COUNTS THE BULBS BESIDE IT.";
    case Game::Hashi:
      return "JOIN THE ISLANDS. ONE OR TWO BRIDGES A PAIR, NONE CROSSING, ALL IN ONE PIECE.";
    case Game::Slither:
    default:
      return "DRAW ONE CLOSED LOOP. A NUMBER IS HOW MANY SIDES OF ITS CELL THE LOOP USES.";
  }
}

int sideOf(const Game game, const int sizeIndex) {
  return game == Game::Hashi ? kWebSides[clampSize(sizeIndex)] : kGridSides[clampSize(sizeIndex)];
}

int boardsPerSize() { return logicpuzzles::kPerSize; }

Board boardOf(const Game game, const int sizeIndex, int number) {
  const int size = clampSize(sizeIndex);
  if (number < 0) number = 0;
  if (number >= logicpuzzles::kPerSize) number = logicpuzzles::kPerSize - 1;
  Board board;
  board.w = board.h = static_cast<uint8_t>(sideOf(game, size));
  board.cells = tableOf(game, size)[number];
  return board;
}

// ---------------- Slitherlink ----------------

void Slither::load(const Board& board) {
  b = board;
  std::memset(hz, 0, sizeof(hz));
  std::memset(vt, 0, sizeof(vt));
}

void Slither::cycleH(const int x, const int y) {
  if (x < 0 || x >= b.w || y < 0 || y > b.h) return;
  uint8_t& e = hz[y * b.w + x];
  e = static_cast<uint8_t>((e + 1) % 3);
}

void Slither::cycleV(const int x, const int y) {
  if (x < 0 || x > b.w || y < 0 || y >= b.h) return;
  uint8_t& e = vt[y * (b.w + 1) + x];
  e = static_cast<uint8_t>((e + 1) % 3);
}

int Slither::linesAround(const int x, const int y) const {
  return (hz[y * b.w + x] == Line) + (hz[(y + 1) * b.w + x] == Line) + (vt[y * (b.w + 1) + x] == Line) +
         (vt[y * (b.w + 1) + x + 1] == Line);
}

bool Slither::solved() const {
  for (int y = 0; y < b.h; ++y) {
    for (int x = 0; x < b.w; ++x) {
      const char ch = b.at(x, y);
      if (ch != '.' && linesAround(x, y) != ch - '0') return false;
    }
  }
  // Every dot has no line or two, and the lines are one ring: walking from any
  // dot on the loop reaches every dot that is on a line.
  const int vw = b.w + 1;
  auto degree = [&](const int vx, const int vy) {
    int d = 0;
    if (vx > 0 && hz[vy * b.w + vx - 1] == Line) ++d;
    if (vx < b.w && hz[vy * b.w + vx] == Line) ++d;
    if (vy > 0 && vt[(vy - 1) * vw + vx] == Line) ++d;
    if (vy < b.h && vt[vy * vw + vx] == Line) ++d;
    return d;
  };
  int onLoop = 0;
  int start = -1;
  for (int vy = 0; vy <= b.h; ++vy) {
    for (int vx = 0; vx <= b.w; ++vx) {
      const int d = degree(vx, vy);
      if (d != 0 && d != 2) return false;
      if (d == 2) {
        ++onLoop;
        if (start < 0) start = vy * vw + vx;
      }
    }
  }
  if (start < 0) return false;
  bool seen[(kMaxSide + 1) * (kMaxSide + 1)] = {};
  int stack[(kMaxSide + 1) * (kMaxSide + 1)];
  int top = 0;
  int reached = 0;
  stack[top++] = start;
  seen[start] = true;
  while (top > 0) {
    const int v = stack[--top];
    ++reached;
    const int vx = v % vw;
    const int vy = v / vw;
    const int next[4] = {
        vx > 0 && hz[vy * b.w + vx - 1] == Line ? v - 1 : -1,
        vx < b.w && hz[vy * b.w + vx] == Line ? v + 1 : -1,
        vy > 0 && vt[(vy - 1) * vw + vx] == Line ? v - vw : -1,
        vy < b.h && vt[vy * vw + vx] == Line ? v + vw : -1,
    };
    for (const int n : next) {
      if (n >= 0 && !seen[n]) {
        seen[n] = true;
        stack[top++] = n;
      }
    }
  }
  return reached == onLoop;
}

int Slither::save(uint8_t* out, const int cap) const {
  const int nh = (b.h + 1) * b.w;
  const int nv = b.h * (b.w + 1);
  if (cap < nh + nv) return 0;
  std::memcpy(out, hz, static_cast<size_t>(nh));
  std::memcpy(out + nh, vt, static_cast<size_t>(nv));
  return nh + nv;
}

bool Slither::restore(const uint8_t* in, const int len) {
  const int nh = (b.h + 1) * b.w;
  const int nv = b.h * (b.w + 1);
  if (len != nh + nv) return false;
  for (int i = 0; i < len; ++i) {
    if (in[i] > Cross) return false;
  }
  std::memcpy(hz, in, static_cast<size_t>(nh));
  std::memcpy(vt, in + nh, static_cast<size_t>(nv));
  return true;
}

// ---------------- Shikaku ----------------

void Shikaku::load(const Board& board) {
  b = board;
  count = 0;
  anchorX = anchorY = -1;
}

int Shikaku::rectAt(const int x, const int y) const {
  for (int i = 0; i < count; ++i) {
    const Rect& r = rects[i];
    if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) return i;
  }
  return -1;
}

void Shikaku::tap(const int x, const int y) {
  if (x < 0 || y < 0 || x >= b.w || y >= b.h) return;
  if (anchorX < 0) {
    const int hit = rectAt(x, y);
    if (hit >= 0) {
      rects[hit] = rects[--count];
      return;
    }
    anchorX = static_cast<int8_t>(x);
    anchorY = static_cast<int8_t>(y);
    return;
  }
  Rect r;
  r.x = static_cast<int8_t>(x < anchorX ? x : anchorX);
  r.y = static_cast<int8_t>(y < anchorY ? y : anchorY);
  r.w = static_cast<int8_t>((x < anchorX ? anchorX - x : x - anchorX) + 1);
  r.h = static_cast<int8_t>((y < anchorY ? anchorY - y : y - anchorY) + 1);
  anchorX = anchorY = -1;
  for (int i = count - 1; i >= 0; --i) {
    const Rect& o = rects[i];
    const bool apart = o.x >= r.x + r.w || r.x >= o.x + o.w || o.y >= r.y + r.h || r.y >= o.y + o.h;
    if (!apart) rects[i] = rects[--count];
  }
  if (count < kMaxCells) rects[count++] = r;
}

bool Shikaku::good(const int rect) const {
  if (rect < 0 || rect >= count) return false;
  const Rect& r = rects[rect];
  int clues = 0;
  int value = 0;
  for (int y = r.y; y < r.y + r.h; ++y) {
    for (int x = r.x; x < r.x + r.w; ++x) {
      if (b.at(x, y) != '.') {
        ++clues;
        value = b.at(x, y) - '0';
      }
    }
  }
  return clues == 1 && value == r.w * r.h;
}

bool Shikaku::solved() const {
  int covered = 0;
  for (int i = 0; i < count; ++i) {
    if (!good(i)) return false;
    covered += rects[i].w * rects[i].h;
  }
  // tap() never lets two overlap, so covering the area is covering the grid.
  return covered == b.w * b.h;
}

int Shikaku::save(uint8_t* out, const int cap) const {
  if (cap < 1 + count * 4) return 0;
  out[0] = static_cast<uint8_t>(count);
  for (int i = 0; i < count; ++i) {
    out[1 + i * 4] = static_cast<uint8_t>(rects[i].x);
    out[2 + i * 4] = static_cast<uint8_t>(rects[i].y);
    out[3 + i * 4] = static_cast<uint8_t>(rects[i].w);
    out[4 + i * 4] = static_cast<uint8_t>(rects[i].h);
  }
  return 1 + count * 4;
}

bool Shikaku::restore(const uint8_t* in, const int len) {
  if (len < 1 || in[0] > kMaxCells || len != 1 + in[0] * 4) return false;
  count = 0;
  anchorX = anchorY = -1;
  for (int i = 0; i < in[0]; ++i) {
    const uint8_t* r = in + 1 + i * 4;
    if (r[2] == 0 || r[3] == 0 || r[0] + r[2] > b.w || r[1] + r[3] > b.h) {
      count = 0;
      return false;
    }
    // Through tap(), so a damaged file cannot plant overlapping rectangles.
    tap(r[0], r[1]);
    if (anchorX < 0) {
      count = 0;
      return false;
    }
    tap(r[0] + r[2] - 1, r[1] + r[3] - 1);
  }
  return true;
}

// ---------------- Akari ----------------

void Akari::load(const Board& board) {
  b = board;
  std::memset(mark, 0, sizeof(mark));
}

void Akari::tap(const int x, const int y) {
  if (x < 0 || y < 0 || x >= b.w || y >= b.h || !white(x, y)) return;
  uint8_t& m = mark[y * b.w + x];
  m = static_cast<uint8_t>((m + 1) % 3);
}

namespace {
// True when a bulb stands somewhere along the four rays from (x, y), walls
// stopping each.
bool bulbInSight(const Akari& a, const int x, const int y) {
  static const int dx[4] = {1, -1, 0, 0};
  static const int dy[4] = {0, 0, 1, -1};
  for (int d = 0; d < 4; ++d) {
    int nx = x + dx[d];
    int ny = y + dy[d];
    while (nx >= 0 && ny >= 0 && nx < a.b.w && ny < a.b.h && a.white(nx, ny)) {
      if (a.mark[ny * a.b.w + nx] == Akari::Bulb) return true;
      nx += dx[d];
      ny += dy[d];
    }
  }
  return false;
}
}  // namespace

bool Akari::lit(const int x, const int y) const {
  return white(x, y) && (mark[y * b.w + x] == Bulb || bulbInSight(*this, x, y));
}

bool Akari::clash(const int x, const int y) const {
  return white(x, y) && mark[y * b.w + x] == Bulb && bulbInSight(*this, x, y);
}

int Akari::bulbsAround(const int x, const int y) const {
  int n = 0;
  if (x > 0 && mark[y * b.w + x - 1] == Bulb) ++n;
  if (x + 1 < b.w && mark[y * b.w + x + 1] == Bulb) ++n;
  if (y > 0 && mark[(y - 1) * b.w + x] == Bulb) ++n;
  if (y + 1 < b.h && mark[(y + 1) * b.w + x] == Bulb) ++n;
  return n;
}

bool Akari::solved() const {
  for (int y = 0; y < b.h; ++y) {
    for (int x = 0; x < b.w; ++x) {
      const char ch = b.at(x, y);
      if (ch == '.') {
        if (!lit(x, y) || clash(x, y)) return false;
      } else if (ch != '#' && bulbsAround(x, y) != ch - '0') {
        return false;
      }
    }
  }
  return true;
}

int Akari::save(uint8_t* out, const int cap) const {
  const int n = b.w * b.h;
  if (cap < n) return 0;
  std::memcpy(out, mark, static_cast<size_t>(n));
  return n;
}

bool Akari::restore(const uint8_t* in, const int len) {
  if (len != b.w * b.h) return false;
  for (int i = 0; i < len; ++i) {
    if (in[i] > Dot || (in[i] != Empty && b.cells[i] != '.')) return false;
  }
  std::memcpy(mark, in, static_cast<size_t>(len));
  return true;
}

// ---------------- Hashi ----------------

void Hashi::load(const Board& board) {
  b = board;
  islandCount = 0;
  edgeCount = 0;
  int8_t at[kMaxCells];
  std::memset(at, -1, sizeof(at));
  for (int y = 0; y < b.h; ++y) {
    for (int x = 0; x < b.w; ++x) {
      if (b.at(x, y) == '.' || islandCount >= kMaxIslands) continue;
      at[y * b.w + x] = static_cast<int8_t>(islandCount);
      islands[islandCount++] =
          Island{static_cast<int8_t>(x), static_cast<int8_t>(y), static_cast<int8_t>(b.at(x, y) - '0')};
    }
  }
  // Each island's nearest neighbour to the right and below, in scan order: the
  // same order the generator's solver lists them in.
  for (int i = 0; i < islandCount; ++i) {
    for (int x = islands[i].x + 1; x < b.w; ++x) {
      const int other = at[islands[i].y * b.w + x];
      if (other >= 0) {
        if (edgeCount < kMaxEdges) edges[edgeCount++] = Edge{static_cast<uint8_t>(i), static_cast<uint8_t>(other), 0};
        break;
      }
    }
    for (int y = islands[i].y + 1; y < b.h; ++y) {
      const int other = at[y * b.w + islands[i].x];
      if (other >= 0) {
        if (edgeCount < kMaxEdges) edges[edgeCount++] = Edge{static_cast<uint8_t>(i), static_cast<uint8_t>(other), 0};
        break;
      }
    }
  }
}

int Hashi::sumOf(const int island) const {
  int sum = 0;
  for (int e = 0; e < edgeCount; ++e) {
    if (edges[e].a == island || edges[e].b == island) sum += edges[e].count;
  }
  return sum;
}

bool Hashi::blocked(const int edge) const {
  const bool flat = horizontal(edge);
  for (int o = 0; o < edgeCount; ++o) {
    if (o == edge || edges[o].count == 0 || horizontal(o) == flat) continue;
    const Edge& hzE = flat ? edges[edge] : edges[o];
    const Edge& vtE = flat ? edges[o] : edges[edge];
    const Island& l = islands[hzE.a];
    const Island& r = islands[hzE.b];
    const Island& t = islands[vtE.a];
    const Island& d = islands[vtE.b];
    if (t.x > l.x && t.x < r.x && l.y > t.y && l.y < d.y) return true;
  }
  return false;
}

bool Hashi::cycle(const int edge) {
  if (edge < 0 || edge >= edgeCount) return false;
  if (edges[edge].count == 0 && blocked(edge)) return false;
  edges[edge].count = static_cast<uint8_t>((edges[edge].count + 1) % 3);
  return true;
}

int Hashi::edgeNear(const float gx, const float gy) const {
  int best = -1;
  float bestDistance = 0.5f;
  for (int e = 0; e < edgeCount; ++e) {
    const Island& a = islands[edges[e].a];
    const Island& c = islands[edges[e].b];
    const float ax = static_cast<float>(a.x) + 0.5f;
    const float ay = static_cast<float>(a.y) + 0.5f;
    const float cx = static_cast<float>(c.x) + 0.5f;
    const float cy = static_cast<float>(c.y) + 0.5f;
    float distance = 0;
    if (horizontal(e)) {
      // Between the islands, not on them: a tap on a disc is not a tap on a bridge.
      if (gx < ax + 0.5f || gx > cx - 0.5f) continue;
      distance = gy > ay ? gy - ay : ay - gy;
    } else {
      if (gy < ay + 0.5f || gy > cy - 0.5f) continue;
      distance = gx > ax ? gx - ax : ax - gx;
    }
    if (distance <= bestDistance) {
      bestDistance = distance;
      best = e;
    }
  }
  return best;
}

bool Hashi::solved() const {
  if (islandCount == 0) return false;
  for (int i = 0; i < islandCount; ++i) {
    if (sumOf(i) != islands[i].n) return false;
  }
  bool seen[kMaxIslands] = {};
  int stack[kMaxIslands];
  int top = 0;
  int reached = 0;
  stack[top++] = 0;
  seen[0] = true;
  while (top > 0) {
    const int v = stack[--top];
    ++reached;
    for (int e = 0; e < edgeCount; ++e) {
      if (edges[e].count == 0) continue;
      const int other = edges[e].a == v ? edges[e].b : (edges[e].b == v ? edges[e].a : -1);
      if (other >= 0 && !seen[other]) {
        seen[other] = true;
        stack[top++] = other;
      }
    }
  }
  return reached == islandCount;
}

int Hashi::save(uint8_t* out, const int cap) const {
  if (cap < edgeCount) return 0;
  for (int e = 0; e < edgeCount; ++e) out[e] = edges[e].count;
  return edgeCount;
}

bool Hashi::restore(const uint8_t* in, const int len) {
  if (len != edgeCount) return false;
  for (int e = 0; e < edgeCount; ++e) edges[e].count = 0;
  for (int e = 0; e < edgeCount; ++e) {
    if (in[e] > 2) return false;
    // Through cycle(), so a damaged file cannot lay crossing bridges.
    for (int k = 0; k < in[e]; ++k) cycle(e);
  }
  return true;
}

}  // namespace logic
