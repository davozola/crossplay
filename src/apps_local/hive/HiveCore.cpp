#include "HiveCore.h"

namespace hive {

namespace {

struct Cell {
  int8_t q;
  int8_t r;
};

// A short list of cells with no repeats. Everything a move can reach is next
// to the hive, so a hundred is room to spare for twenty-two pieces.
struct CellSet {
  static constexpr int kCap = 128;
  Cell cells[kCap];
  int count = 0;
  bool has(const int q, const int r) const {
    for (int i = 0; i < count; ++i) {
      if (cells[i].q == q && cells[i].r == r) return true;
    }
    return false;
  }
  bool add(const int q, const int r) {
    if (count >= kCap || has(q, r)) return false;
    cells[count++] = Cell{static_cast<int8_t>(q), static_cast<int8_t>(r)};
    return true;
  }
};

int emit(const int piece, const CellSet& set, Move* out, const int cap) {
  int n = 0;
  for (int i = 0; i < set.count && n < cap; ++i) {
    out[n].piece = static_cast<int8_t>(piece);
    out[n].q = set.cells[i].q;
    out[n].r = set.cells[i].r;
    ++n;
  }
  return n;
}

}  // namespace

const char* bugName(const Bug bug) {
  switch (bug) {
    case Bug::Beetle:
      return "BEETLE";
    case Bug::Spider:
      return "SPIDER";
    case Bug::Hopper:
      return "HOPPER";
    case Bug::Ant:
      return "ANT";
    case Bug::Queen:
    default:
      return "QUEEN";
  }
}

char bugLetter(const Bug bug) {
  static const char kLetters[kBugTypes] = {'Q', 'B', 'S', 'G', 'A'};
  return kLetters[static_cast<int>(bug) % kBugTypes];
}

void Game::reset() {
  int at = 0;
  for (int color = 0; color < 2; ++color) {
    for (int bug = 0; bug < kBugTypes; ++bug) {
      for (int k = 0; k < kBugCount[bug]; ++k) {
        pieces[at] = Piece{};
        pieces[at].bug = static_cast<uint8_t>(bug);
        pieces[at].color = static_cast<uint8_t>(color);
        ++at;
      }
    }
  }
  turn = 0;
  turnsTaken[0] = turnsTaken[1] = 0;
}

int Game::topAt(const int q, const int r, const int skip) const {
  int top = -1;
  for (int i = 0; i < kPieces; ++i) {
    const Piece& p = pieces[i];
    if (i == skip || !p.placed || p.q != q || p.r != r) continue;
    if (top < 0 || p.z > pieces[top].z) top = i;
  }
  return top;
}

int Game::heightAt(const int q, const int r, const int skip) const {
  int h = 0;
  for (int i = 0; i < kPieces; ++i) {
    const Piece& p = pieces[i];
    if (i != skip && p.placed && p.q == q && p.r == r) ++h;
  }
  return h;
}

int Game::inHand(const int color, const Bug bug) const {
  int n = 0;
  for (int i = 0; i < kPieces; ++i) {
    const Piece& p = pieces[i];
    if (!p.placed && p.color == color && p.bug == static_cast<uint8_t>(bug)) ++n;
  }
  return n;
}

int Game::handPiece(const int color, const Bug bug) const {
  for (int i = 0; i < kPieces; ++i) {
    const Piece& p = pieces[i];
    if (!p.placed && p.color == color && p.bug == static_cast<uint8_t>(bug)) return i;
  }
  return -1;
}

bool Game::queenPlaced(const int color) const { return pieces[color * kPerPlayer].placed != 0; }

int Game::placedCount() const {
  int n = 0;
  for (int i = 0; i < kPieces; ++i) n += pieces[i].placed ? 1 : 0;
  return n;
}

bool Game::connected(const int skip) const {
  Cell cells[kPieces];
  int count = 0;
  for (int i = 0; i < kPieces; ++i) {
    const Piece& p = pieces[i];
    if (i == skip || !p.placed) continue;
    bool seen = false;
    for (int k = 0; k < count; ++k) seen = seen || (cells[k].q == p.q && cells[k].r == p.r);
    if (!seen) cells[count++] = Cell{p.q, p.r};
  }
  if (count <= 1) return true;
  bool reached[kPieces] = {};
  int stack[kPieces];
  int top = 0;
  int got = 0;
  stack[top++] = 0;
  reached[0] = true;
  while (top > 0) {
    const int at = stack[--top];
    ++got;
    for (int k = 0; k < count; ++k) {
      if (reached[k]) continue;
      const int dq = cells[k].q - cells[at].q;
      const int dr = cells[k].r - cells[at].r;
      bool beside = false;
      for (int d = 0; d < 6; ++d) beside = beside || (dq == kDirQ[d] && dr == kDirR[d]);
      if (beside) {
        reached[k] = true;
        stack[top++] = k;
      }
    }
  }
  return got == count;
}

int Game::placements(const int color, const int piece, Move* out, const int cap) const {
  CellSet set;
  int own = 0;
  int total = 0;
  for (int i = 0; i < kPieces; ++i) {
    if (!pieces[i].placed) continue;
    ++total;
    if (pieces[i].color == color) ++own;
  }
  if (total == 0) {
    set.add(0, 0);
    return emit(piece, set, out, cap);
  }
  for (int i = 0; i < kPieces; ++i) {
    const Piece& p = pieces[i];
    if (!p.placed) continue;
    for (int d = 0; d < 6; ++d) {
      const int q = p.q + kDirQ[d];
      const int r = p.r + kDirR[d];
      if (heightAt(q, r) > 0 || set.has(q, r)) continue;
      // The first piece a side puts down may touch anything. After that a new
      // piece touches its own colour only, judged by what is on top.
      bool ok = true;
      if (own > 0) {
        for (int e = 0; e < 6 && ok; ++e) {
          const int top = topAt(q + kDirQ[e], r + kDirR[e]);
          if (top >= 0 && pieces[top].color != color) ok = false;
        }
      }
      if (ok) set.add(q, r);
    }
  }
  return emit(piece, set, out, cap);
}

// A ground-level step from (q, r) in `dir`: the cell must be free, and of the
// two cells either side of the way exactly one is taken: both would be a gap
// too narrow to pass, neither would be letting go of the hive.
bool Game::slides(const int q, const int r, const int dir, const int skip) const {
  if (heightAt(q + kDirQ[dir], r + kDirR[dir], skip) > 0) return false;
  const int left = (dir + 5) % 6;
  const int right = (dir + 1) % 6;
  const bool l = heightAt(q + kDirQ[left], r + kDirR[left], skip) > 0;
  const bool rr = heightAt(q + kDirQ[right], r + kDirR[right], skip) > 0;
  return l != rr;
}

int Game::movements(const int piece, Move* out, const int cap) const {
  const Piece& p = pieces[piece];
  if (!queenPlaced(p.color)) return 0;
  if (topAt(p.q, p.r) != piece) return 0;       // something sits on it
  if (p.z == 0 && !connected(piece)) return 0;  // lifting it would split the hive
  CellSet set;
  switch (static_cast<Bug>(p.bug)) {
    case Bug::Queen:
      for (int d = 0; d < 6; ++d) {
        if (slides(p.q, p.r, d, piece)) set.add(p.q + kDirQ[d], p.r + kDirR[d]);
      }
      break;
    case Bug::Beetle:
      for (int d = 0; d < 6; ++d) {
        const int q = p.q + kDirQ[d];
        const int r = p.r + kDirR[d];
        const int from = p.z;
        const int to = heightAt(q, r, piece);
        if (from == 0 && to == 0) {
          if (slides(p.q, p.r, d, piece)) set.add(q, r);
          continue;
        }
        // Up, down or across the top: the gate is shut only when both sides
        // stand higher than the step itself.
        const int left = (d + 5) % 6;
        const int right = (d + 1) % 6;
        const int hl = heightAt(p.q + kDirQ[left], p.r + kDirR[left], piece);
        const int hr = heightAt(p.q + kDirQ[right], p.r + kDirR[right], piece);
        const int step = from > to ? from : to;
        if ((hl < hr ? hl : hr) > step) continue;
        set.add(q, r);
      }
      break;
    case Bug::Hopper:
      for (int d = 0; d < 6; ++d) {
        int q = p.q + kDirQ[d];
        int r = p.r + kDirR[d];
        if (heightAt(q, r, piece) == 0) continue;  // nothing to jump
        while (heightAt(q, r, piece) > 0) {
          q += kDirQ[d];
          r += kDirR[d];
        }
        set.add(q, r);
      }
      break;
    case Bug::Spider: {
      // Exactly three steps, never back over its own path.
      Cell path[4];
      path[0] = Cell{p.q, p.r};
      int dirAt[3] = {0, 0, 0};
      int depth = 0;
      while (depth >= 0) {
        if (dirAt[depth] >= 6) {
          --depth;
          if (depth >= 0) ++dirAt[depth];
          continue;
        }
        const int d = dirAt[depth];
        const Cell at = path[depth];
        const int q = at.q + kDirQ[d];
        const int r = at.r + kDirR[d];
        bool fresh = slides(at.q, at.r, d, piece);
        for (int k = 0; k <= depth && fresh; ++k) fresh = !(path[k].q == q && path[k].r == r);
        if (!fresh) {
          ++dirAt[depth];
          continue;
        }
        if (depth == 2) {
          set.add(q, r);
          ++dirAt[depth];
          continue;
        }
        path[depth + 1] = Cell{static_cast<int8_t>(q), static_cast<int8_t>(r)};
        ++depth;
        dirAt[depth] = 0;
      }
      break;
    }
    case Bug::Ant: {
      // Anywhere it can walk to round the outside.
      CellSet seen;
      seen.add(p.q, p.r);
      for (int at = 0; at < seen.count; ++at) {
        const Cell c = seen.cells[at];
        for (int d = 0; d < 6; ++d) {
          if (slides(c.q, c.r, d, piece)) seen.add(c.q + kDirQ[d], c.r + kDirR[d]);
        }
      }
      for (int i = 1; i < seen.count; ++i) set.add(seen.cells[i].q, seen.cells[i].r);
      break;
    }
  }
  return emit(piece, set, out, cap);
}

int Game::destinations(const int piece, Move* out, const int cap) const {
  if (piece < 0 || piece >= kPieces || winner() >= 0) return 0;
  const Piece& p = pieces[piece];
  if (p.color != turn) return 0;
  const bool queenDue = turnsTaken[turn] >= 3 && !queenPlaced(turn);
  if (p.placed) return queenDue ? 0 : movements(piece, out, cap);
  if (queenDue && p.bug != static_cast<uint8_t>(Bug::Queen)) return 0;
  return placements(turn, piece, out, cap);
}

int Game::legalMoves(Move* out, const int cap) const {
  int n = 0;
  if (winner() < 0) {
    for (int bug = 0; bug < kBugTypes; ++bug) {
      const int piece = handPiece(turn, static_cast<Bug>(bug));
      if (piece >= 0) n += destinations(piece, out + n, cap - n);
    }
    for (int i = turn * kPerPlayer; i < (turn + 1) * kPerPlayer; ++i) {
      if (pieces[i].placed) n += destinations(i, out + n, cap - n);
    }
  }
  if (n == 0 && cap > 0) {
    out[0] = Move{};
    n = 1;
  }
  return n;
}

void Game::apply(const Move& move) {
  if (!move.pass()) {
    Piece& p = pieces[move.piece];
    p.z = static_cast<uint8_t>(heightAt(move.q, move.r, move.piece));
    p.q = move.q;
    p.r = move.r;
    p.placed = 1;
  }
  ++turnsTaken[turn];
  turn = static_cast<uint8_t>(turn ^ 1);
}

int Game::queenNeighbours(const int color) const {
  const Piece& queen = pieces[color * kPerPlayer];
  if (!queen.placed) return 0;
  int n = 0;
  for (int d = 0; d < 6; ++d) n += heightAt(queen.q + kDirQ[d], queen.r + kDirR[d]) > 0 ? 1 : 0;
  return n;
}

int Game::winner() const {
  const bool whiteLost = queenNeighbours(0) == 6;
  const bool blackLost = queenNeighbours(1) == 6;
  if (whiteLost && blackLost) return 2;
  if (whiteLost) return 1;
  if (blackLost) return 0;
  return -1;
}

}  // namespace hive
