#pragma once

// The built-in opponent. Freestanding, like the rules it plays by.
//
// Three strengths. The easy one is the two-ply reader this began as, described
// below. The other two search properly -- see chooseMoveAt() -- and differ only
// in how many positions they may look at.
//
// Easy: two ply with a material evaluation: play every legal move, let the opponent
// reply with their best, keep the move whose worst case is best. Checkers
// punishes one-ply greed badly -- a free capture is very often a trap, because
// the mandatory-capture rule means the reply is forced and can be a multi-jump
// -- so one ply here would produce an opponent that hangs pieces constantly.
// That is a different judgement from Knucklebones, where one ply was right
// because the die makes deeper search worthless.
//
// It cannot cheat: `Game` holds no hidden state and no generator. That is a
// property of the design, not a promise -- there is nothing here to peek at.

#include <cstdint>

#include "CheckersCore.h"

namespace checkers {

// A king is worth more than a man, and pieces are worth more than position on a
// board this size. Kept deliberately crude: a heavier evaluation would make the
// opponent stronger and the search slower, and this one already sees traps.
constexpr int kManValue = 10;
constexpr int kKingValue = 18;

inline int material(const Game& game, const uint8_t seat) {
  int score = 0;
  for (int index = 0; index < kCells; ++index) {
    if (!occupied(game, index)) continue;
    const int value = isKing(game, index) ? kKingValue : kManValue;
    score += ownerOf(game, index) == seat ? value : -value;
  }
  return score;
}

// The opponent's best reply to a position, as a score for `seat`.
inline int worstReply(const Game& game, const uint8_t seat) {
  Move list[kMaxMoves];
  const int count = moves(game, list);
  // They have nothing: this position wins for whoever is not to move.
  if (count == 0) return game.turn == seat ? -1000 : 1000;

  int worst = 2000;
  for (int i = 0; i < count; ++i) {
    Game trial = game;
    if (!play(trial, list[i])) continue;
    const int score = material(trial, seat);
    if (score < worst) worst = score;
  }
  return worst;
}

// The move the opponent would play, or false when it has none.
//
// Ties break toward the earlier move in generation order, which is
// deterministic: a random tiebreak would make the same position play
// differently on two devices, and a match has to stay reconstructible.
inline bool chooseMove(const Game& game, Move& out) {
  Move list[kMaxMoves];
  const int count = moves(game, list);
  if (count == 0) return false;

  const uint8_t seat = game.turn;
  int bestScore = -3000;
  int best = -1;
  for (int i = 0; i < count; ++i) {
    Game trial = game;
    if (!play(trial, list[i])) continue;
    const int score = worstReply(trial, seat);
    if (score > bestScore) {
      bestScore = score;
      best = i;
    }
  }
  if (best < 0) return false;
  out = list[best];
  return true;
}

// --- The two that search -----------------------------------------------------
//
// Negamax with alpha-beta, deepened a ply at a time until a budget of
// positions is spent, and the last finished depth is the answer. A line that
// ends with a capture still to be made is followed until it is quiet: the
// capture is compulsory, so stopping there counts pieces that are about to
// leave the board. The budget is counted in positions, not time, so the same
// position gets the same move on every device and in the host tests.

enum class Level : uint8_t { Easy = 0, Medium = 1, Strong = 2 };
constexpr int kLevels = 3;

constexpr int kMaxPly = 40;
// The move lists of a search, one a ply, kept off the stack: 48 moves of 16
// bytes at forty plies is more than a task's stack has to spare. The caller
// owns one and lends it.
struct Scratch {
  Move list[kMaxPly][kMaxMoves];
};

namespace brain {

constexpr int kWin = 100000;
constexpr int kMan = 100;
constexpr int kKing = 165;

// The position for `seat`: pieces first, then where they stand. A man is worth
// more the nearer it is to its crown, a man still on the home row keeps the
// other side from crowning, and the middle of the board is where both men and
// kings have somewhere to go.
inline int evaluate(const Game& game, const uint8_t seat) {
  int mine = 0;
  int theirs = 0;
  int score = 0;
  for (int index = 0; index < kCells; ++index) {
    if (!occupied(game, index)) continue;
    const int file = index % kSize;
    const int rank = index / kSize;
    const uint8_t owner = ownerOf(game, index);
    const bool central = file >= 2 && file <= 5 && rank >= 2 && rank <= 5;
    int value = 0;
    if (isKing(game, index)) {
      value = kKing + (central ? 8 : 0) - ((file == 0 || file == kSize - 1) ? 6 : 0);
    } else {
      // Light climbs toward rank 0, dark toward rank 7.
      const int advanced = owner == kLight ? (kSize - 1 - rank) : rank;
      const bool home = owner == kLight ? rank == kSize - 1 : rank == 0;
      value = kMan + advanced * 3 + (home ? 9 : 0) + (central ? 4 : 0);
    }
    if (owner == seat) {
      ++mine;
      score += value;
    } else {
      ++theirs;
      score -= value;
    }
  }
  // Ahead, exchanges are good: the same lead counts for more with less on the board.
  if (mine != theirs && mine + theirs > 0) score += (mine - theirs) * 24 / (mine + theirs);
  return score;
}

struct Search {
  Scratch* scratch;
  long nodes;
  long budget;
  bool spent;
};

inline int negamax(Search& s, const Game& game, const int depth, const int ply, int alpha, const int beta) {
  if (++s.nodes > s.budget) {
    s.spent = true;
    return 0;
  }
  if (game.idlePlies >= kIdleLimit) return 0;
  Move* list = s.scratch->list[ply];
  const int count = moves(game, list);
  // Nothing to play is the game lost, and sooner is worse.
  if (count == 0) return -kWin + ply;
  // Quiet and out of depth: count it. A capture waiting is never quiet.
  const bool capture = list[0].taken != 0;
  if ((depth <= 0 && !capture) || ply >= kMaxPly - 1) return evaluate(game, game.turn);
  int best = -kWin * 2;
  for (int i = 0; i < count; ++i) {
    Game next = game;
    if (!play(next, list[i])) continue;
    const int score = -negamax(s, next, depth - 1, ply + 1, -beta, -alpha);
    if (s.spent) return 0;
    if (score > best) best = score;
    if (best > alpha) alpha = best;
    if (alpha >= beta) break;
  }
  return best;
}

}  // namespace brain

// How many positions a level may look at for one move.
inline long budgetOf(const Level level) { return level == Level::Strong ? 160000L : 24000L; }

// The move for the side to play at `level`. `positions`, when given, is how
// many were looked at.
inline bool chooseMoveAt(const Game& game, const Level level, Scratch& scratch, Move& out, long* positions = nullptr) {
  if (positions) *positions = 0;
  if (level == Level::Easy) return chooseMove(game, out);
  Move root[kMaxMoves];
  int count = moves(game, root);
  if (count == 0) return false;
  out = root[0];
  // One move is no choice, and a forced capture usually is one.
  if (count == 1) return true;

  brain::Search s{&scratch, 0, budgetOf(level), false};
  for (int depth = 1; depth < kMaxPly - 2; ++depth) {
    int alpha = -brain::kWin * 2;
    int bestAt = -1;
    int bestScore = -brain::kWin * 2;
    for (int i = 0; i < count; ++i) {
      Game next = game;
      if (!play(next, root[i])) continue;
      const int score = -brain::negamax(s, next, depth - 1, 1, -brain::kWin * 2, -alpha);
      if (s.spent) break;
      if (score > bestScore) {
        bestScore = score;
        bestAt = i;
      }
      if (bestScore > alpha) alpha = bestScore;
    }
    if (s.spent || bestAt < 0) break;
    // The best of this depth goes first at the next: it is searched with the
    // widest window, and it is the answer if the budget runs out part way.
    const Move best = root[bestAt];
    for (int i = bestAt; i > 0; --i) root[i] = root[i - 1];
    root[0] = best;
    out = best;
    // A win found, or a loss that cannot be put off, needs no deeper look.
    if (bestScore > brain::kWin - kMaxPly || bestScore < -brain::kWin + kMaxPly) break;
  }
  if (positions) *positions = s.nodes;
  return true;
}

inline const char* levelName(const Level level) {
  switch (level) {
    case Level::Easy:
      return "EASY";
    case Level::Strong:
      return "HARD";
    case Level::Medium:
    default:
      return "MEDIUM";
  }
}

}  // namespace checkers
