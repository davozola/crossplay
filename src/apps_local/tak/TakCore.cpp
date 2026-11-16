#include "TakCore.h"

#include <cstdio>

namespace tak {

namespace {

constexpr int kDr[4] = {-1, 0, 1, 0};
constexpr int kDc[4] = {0, 1, 0, -1};

void push(MoveList& out, const Move& m) {
  if (out.count < kMaxMoves) out.moves[out.count++] = m;
}

// Every way to drop `pick` pieces over at most `room` squares, plus, when
// `smash` is set, the patterns that spend exactly one extra square on the
// wall at the end with a single piece (the capstone).
void slides(const int square, const Direction dir, const int pick, const int room, const bool smash, uint32_t drops,
            const int depth, const int left, MoveList& out) {
  if (left == 0) {
    Move m;
    m.kind = MoveKind::Slide;
    m.square = static_cast<uint8_t>(square);
    m.what = dir;
    m.pick = static_cast<uint8_t>(pick);
    m.drops = drops;
    push(out, m);
    return;
  }
  if (depth < room) {
    for (int n = 1; n <= left; ++n) {
      slides(square, dir, pick, room, smash, drops | (static_cast<uint32_t>(n) << (4 * depth)), depth + 1, left - n,
             out);
    }
    return;
  }
  // Out of open squares: the only way on is the capstone alone onto the wall.
  if (smash && depth == room && left == 1) {
    slides(square, dir, pick, room, smash, drops | (1u << (4 * depth)), depth + 1, 0, out);
  }
}

uint64_t roadMask(const Game& game, const uint8_t color) {
  uint64_t mask = 0;
  const int n = game.size * game.size;
  for (int i = 0; i < n; ++i) {
    const Square& s = game.sq[i];
    if (roadPiece(s) && topColor(s) == color) mask |= 1ull << i;
  }
  return mask;
}

// Flood fill inside `mask` from `seed`, four-connected.
uint64_t flood(const Game& game, const uint64_t mask, uint64_t seed) {
  const int size = game.size;
  uint64_t leftCol = 0;
  uint64_t rightCol = 0;
  for (int r = 0; r < size; ++r) {
    leftCol |= 1ull << (r * size);
    rightCol |= 1ull << (r * size + size - 1);
  }
  const uint64_t all = (size * size == 64) ? ~0ull : ((1ull << (size * size)) - 1);
  seed &= mask;
  for (;;) {
    uint64_t grown = seed;
    grown |= (seed << size) & all;
    grown |= seed >> size;
    grown |= (seed & ~rightCol) << 1;
    grown |= (seed & ~leftCol) >> 1;
    grown &= mask;
    if (grown == seed) return seed;
    seed = grown;
  }
}

uint64_t edge(const Game& game, const int which) {
  const int size = game.size;
  uint64_t m = 0;
  for (int i = 0; i < size; ++i) {
    switch (which) {
      case 0:
        m |= 1ull << i;
        break;  // top row
      case 1:
        m |= 1ull << ((size - 1) * size + i);
        break;  // bottom row
      case 2:
        m |= 1ull << (i * size);
        break;  // left column
      default:
        m |= 1ull << (i * size + size - 1);
        break;  // right column
    }
  }
  return m;
}

void settle(Game& game, const uint8_t mover) {
  const bool moverRoad = hasRoad(game, mover);
  const bool otherRoad = hasRoad(game, other(mover));
  if (moverRoad || otherRoad) {
    // Both at once goes to the side that made the move.
    const uint8_t w = moverRoad ? mover : other(mover);
    game.result = w == kWhite ? Result::WhiteRoad : Result::BlackRoad;
    return;
  }
  bool full = true;
  const int n = game.size * game.size;
  for (int i = 0; i < n && full; ++i) full = !empty(game.sq[i]);
  const bool spent =
      (game.stones[kWhite] == 0 && game.caps[kWhite] == 0) || (game.stones[kBlack] == 0 && game.caps[kBlack] == 0);
  if (!full && !spent) return;
  const int w = flatCount(game, kWhite);
  const int b = flatCount(game, kBlack);
  game.result = w > b ? Result::WhiteFlats : (b > w ? Result::BlackFlats : Result::Draw);
}

}  // namespace

bool operator==(const Move& a, const Move& b) {
  if (a.kind != b.kind || a.square != b.square || a.what != b.what) return false;
  if (a.kind == MoveKind::Place) return true;
  return a.pick == b.pick && a.drops == b.drops;
}

Move slide(const int square, const Direction dir, const uint8_t* drops, const int count) {
  Move m;
  m.kind = MoveKind::Slide;
  m.square = static_cast<uint8_t>(square);
  m.what = dir;
  int pick = 0;
  for (int i = 0; i < count && i < 8; ++i) {
    m.drops |= static_cast<uint32_t>(drops[i] & 0xF) << (4 * i);
    pick += drops[i];
  }
  m.pick = static_cast<uint8_t>(pick);
  return m;
}

int dropCount(const Move& m) {
  int n = 0;
  while (n < 8 && ((m.drops >> (4 * n)) & 0xFu) != 0) ++n;
  return n;
}

void reset(Game& game, const int size) {
  game = Game{};
  const int s = size < kMinSize ? kMinSize : (size > kMaxSize ? kMaxSize : size);
  game.size = static_cast<uint8_t>(s);
  game.stones[kWhite] = game.stones[kBlack] = static_cast<uint8_t>(stonesFor(s));
  game.caps[kWhite] = game.caps[kBlack] = static_cast<uint8_t>(capsFor(s));
}

bool openingPly(const Game& game) { return game.ply < 2; }

uint8_t placingColor(const Game& game) { return openingPly(game) ? other(game.turn) : game.turn; }

int step(const Game& game, const int square, const Direction dir) {
  const int size = game.size;
  const int r = square / size + kDr[dir];
  const int c = square % size + kDc[dir];
  if (r < 0 || c < 0 || r >= size || c >= size) return -1;
  return r * size + c;
}

int generate(const Game& game, MoveList& out) {
  out.count = 0;
  if (over(game.result)) return 0;
  const int n = game.size * game.size;
  const uint8_t me = game.turn;
  const bool opening = openingPly(game);
  for (int i = 0; i < n; ++i) {
    if (!empty(game.sq[i])) continue;
    if (opening) {
      push(out, place(i, Stone::Flat));
      continue;
    }
    if (game.stones[me] > 0) {
      push(out, place(i, Stone::Flat));
      push(out, place(i, Stone::Wall));
    }
    if (game.caps[me] > 0) push(out, place(i, Stone::Cap));
  }
  if (opening) return out.count;
  for (int i = 0; i < n; ++i) {
    const Square& s = game.sq[i];
    if (empty(s) || topColor(s) != me) continue;
    const int carry = s.height < game.size ? s.height : game.size;
    for (int d = 0; d < 4; ++d) {
      const Direction dir = static_cast<Direction>(d);
      int room = 0;
      bool smash = false;
      for (int at = step(game, i, dir); at >= 0; at = step(game, at, dir)) {
        const Square& t = game.sq[at];
        if (!empty(t) && t.top == Stone::Cap) break;
        if (!empty(t) && t.top == Stone::Wall) {
          smash = s.top == Stone::Cap;
          break;
        }
        ++room;
      }
      for (int pick = 1; pick <= carry; ++pick) slides(i, dir, pick, room, smash, 0, 0, pick, out);
    }
  }
  return out.count;
}

bool isLegal(const Game& game, const Move& move) {
  // A MoveList is 32KB, too big for a stack frame on the device, so legality
  // is checked directly rather than by generating and searching.
  if (over(game.result)) return false;
  const int n = game.size * game.size;
  if (move.square >= n) return false;
  const uint8_t me = game.turn;
  if (move.kind == MoveKind::Place) {
    if (!empty(game.sq[move.square])) return false;
    const Stone stone = static_cast<Stone>(move.what);
    if (openingPly(game)) return stone == Stone::Flat;
    if (stone == Stone::Cap) return game.caps[me] > 0;
    if (stone == Stone::Flat || stone == Stone::Wall) return game.stones[me] > 0;
    return false;
  }
  if (openingPly(game)) return false;
  const Square& s = game.sq[move.square];
  if (empty(s) || topColor(s) != me) return false;
  if (move.what > West) return false;
  const int carry = s.height < game.size ? s.height : game.size;
  if (move.pick < 1 || move.pick > carry) return false;
  const int count = dropCount(move);
  if (count == 0) return false;
  int total = 0;
  int at = move.square;
  for (int i = 0; i < count; ++i) {
    at = step(game, at, static_cast<Direction>(move.what));
    if (at < 0) return false;
    const int drop = dropAt(move, i);
    total += drop;
    const Square& t = game.sq[at];
    if (!empty(t) && t.top == Stone::Cap) return false;
    if (!empty(t) && t.top == Stone::Wall) {
      // Only the capstone, alone, on the last square of the slide.
      if (!(i == count - 1 && drop == 1 && s.top == Stone::Cap)) return false;
    }
  }
  if ((move.drops >> (4 * count)) != 0) return false;
  return total == move.pick;
}

void apply(Game& game, const Move& move) {
  const uint8_t mover = game.turn;
  if (move.kind == MoveKind::Place) {
    const uint8_t color = placingColor(game);
    const Stone stone = static_cast<Stone>(move.what);
    Square& s = game.sq[move.square];
    s.colors = color;
    s.height = 1;
    s.top = stone;
    if (stone == Stone::Cap) {
      --game.caps[color];
    } else {
      --game.stones[color];
    }
  } else {
    Square& from = game.sq[move.square];
    const int pick = move.pick;
    const Stone moving = from.top;
    // The picked pieces, bottom of the hand first.
    uint64_t hand = from.colors >> (from.height - pick);
    from.height = static_cast<uint8_t>(from.height - pick);
    from.colors &= from.height == 0 ? 0 : ((1ull << from.height) - 1);
    from.top = Stone::Flat;
    int at = move.square;
    int left = pick;
    const int count = dropCount(move);
    for (int i = 0; i < count; ++i) {
      at = step(game, at, static_cast<Direction>(move.what));
      const int drop = dropAt(move, i);
      Square& t = game.sq[at];
      const uint64_t piece = hand & ((1ull << drop) - 1);
      hand >>= drop;
      left -= drop;
      // A wall under a capstone is flattened; anything under anything is a flat.
      t.colors |= piece << t.height;
      t.height = static_cast<uint8_t>(t.height + drop);
      t.top = left == 0 ? moving : Stone::Flat;
    }
  }
  ++game.ply;
  game.turn = other(game.turn);
  settle(game, mover);
}

bool hasRoad(const Game& game, const uint8_t color) { return roadSquares(game, color) != 0; }

uint64_t roadSquares(const Game& game, const uint8_t color) {
  const uint64_t mask = roadMask(game, color);
  if (mask == 0) return 0;
  const uint64_t pairs[2][2] = {{edge(game, 0), edge(game, 1)}, {edge(game, 2), edge(game, 3)}};
  for (const auto& pair : pairs) {
    const uint64_t reached = flood(game, mask, pair[0]);
    // Flood back from the far edge, so groups that touch only the near edge
    // are not marked as part of the road.
    if (reached & pair[1]) return flood(game, mask, reached & pair[1]);
  }
  return 0;
}

int flatCount(const Game& game, const uint8_t color) {
  int count = 0;
  const int n = game.size * game.size;
  for (int i = 0; i < n; ++i) {
    const Square& s = game.sq[i];
    if (!empty(s) && s.top == Stone::Flat && topColor(s) == color) ++count;
  }
  return count;
}

void toPtn(const Game& game, const Move& move, char* out, const int outSize) {
  const int size = game.size;
  const char file = static_cast<char>('a' + move.square % size);
  const int rank = size - move.square / size;
  if (move.kind == MoveKind::Place) {
    const Stone stone = static_cast<Stone>(move.what);
    const char* prefix = stone == Stone::Wall ? "S" : (stone == Stone::Cap ? "C" : "");
    std::snprintf(out, outSize, "%s%c%d", prefix, file, rank);
    return;
  }
  static const char kArrows[4] = {'+', '>', '-', '<'};
  char drops[9] = {};
  const int count = dropCount(move);
  for (int i = 0; i < count; ++i) drops[i] = static_cast<char>('0' + dropAt(move, i));
  char pick[2] = {};
  if (move.pick > 1) pick[0] = static_cast<char>('0' + (move.pick & 0xF));
  // A single drop of everything picked is written without the drop list.
  if (count == 1) drops[0] = '\0';
  std::snprintf(out, outSize, "%s%c%d%c%s", pick, file, rank, kArrows[move.what], drops);
}

bool fromPtn(const Game& game, const char* text, Move& out) {
  const int size = game.size;
  const char* p = text;
  int pick = 0;
  if (*p >= '1' && *p <= '8') {
    pick = *p - '0';
    ++p;
  }
  Stone stone = Stone::Flat;
  bool typed = false;
  if (*p == 'S' || *p == 'C' || *p == 'F') {
    stone = *p == 'S' ? Stone::Wall : (*p == 'C' ? Stone::Cap : Stone::Flat);
    typed = true;
    ++p;
  }
  if (*p < 'a' || *p >= 'a' + size) return false;
  const int col = *p++ - 'a';
  if (*p < '1' || *p >= '1' + size) return false;
  const int rank = *p++ - '0';
  const int square = (size - rank) * size + col;
  if (*p == '\0') {
    if (pick != 0) return false;
    out = place(square, stone);
    return true;
  }
  if (typed) return false;
  static const char kArrows[4] = {'+', '>', '-', '<'};
  int dir = -1;
  for (int d = 0; d < 4; ++d) {
    if (*p == kArrows[d]) dir = d;
  }
  if (dir < 0) return false;
  ++p;
  if (pick == 0) pick = 1;
  uint8_t drops[8] = {};
  int count = 0;
  while (*p >= '1' && *p <= '8' && count < 8) drops[count++] = static_cast<uint8_t>(*p++ - '0');
  if (*p != '\0') return false;
  if (count == 0) drops[count++] = static_cast<uint8_t>(pick);
  out = slide(square, static_cast<Direction>(dir), drops, count);
  return out.pick == pick;
}

}  // namespace tak
