#include "TakFlow.h"

namespace tak {

namespace {

Move partial(const Hand& hand) {
  Move m;
  m.kind = MoveKind::Slide;
  m.square = hand.from;
  m.what = static_cast<uint8_t>(hand.dir < 0 ? 0 : hand.dir);
  m.pick = hand.pick;
  for (int i = 0; i < hand.count; ++i) m.drops |= static_cast<uint32_t>(hand.drops[i]) << (4 * i);
  return m;
}

// Whether dropping one piece on `target` is allowed with `left` pieces in the
// hand before the drop.
bool canDropOn(const Game& game, const Hand& hand, const int target, const int left) {
  const Square& t = game.sq[target];
  if (empty(t)) return true;
  if (t.top == Stone::Cap) return false;
  if (t.top == Stone::Wall) {
    // Only the capstone, alone, as the very last piece of the slide.
    return left == 1 && game.sq[hand.from].top == Stone::Cap;
  }
  return true;
}

TapResult finish(const Game& game, Hand& hand, Move& move) {
  if (inHand(hand) != 0) return TapResult::Changed;
  const Move m = partial(hand);
  if (!isLegal(game, m)) {
    hand = Hand{};
    return TapResult::Changed;
  }
  move = m;
  hand = Hand{};
  return TapResult::Played;
}

}  // namespace

TapResult tapSquare(const Game& game, Hand& hand, const int square, const Stone stone, Move& move) {
  const int n = game.size * game.size;
  if (square < 0 || square >= n || over(game.result)) return TapResult::Ignored;
  const Square& s = game.sq[square];

  if (!hand.holding) {
    if (empty(s)) {
      const Move m = place(square, openingPly(game) ? Stone::Flat : stone);
      if (!isLegal(game, m)) return TapResult::Ignored;
      move = m;
      return TapResult::Played;
    }
    if (openingPly(game) || topColor(s) != game.turn) return TapResult::Ignored;
    hand.holding = true;
    hand.from = static_cast<uint8_t>(square);
    hand.pick = static_cast<uint8_t>(s.height < game.size ? s.height : game.size);
    hand.dir = -1;
    hand.count = 0;
    hand.at = hand.from;
    return TapResult::Changed;
  }

  // Back on the source before anything is dropped: one fewer lifted.
  if (square == hand.from && hand.count == 0) {
    if (--hand.pick == 0) hand = Hand{};
    return TapResult::Changed;
  }

  const int left = inHand(hand);
  // Again on the square just dropped on: one more there.
  if (hand.count > 0 && square == hand.at) {
    if (!canDropOn(game, hand, square, left)) return TapResult::Ignored;
    // A capstone onto a wall is the end of the slide by definition.
    if (game.sq[square].top == Stone::Wall && !empty(game.sq[square])) return TapResult::Ignored;
    ++hand.drops[hand.count - 1];
    return finish(game, hand, move);
  }

  // The next square on.
  int next = -1;
  if (hand.dir < 0) {
    for (int d = 0; d < 4; ++d) {
      if (step(game, hand.from, static_cast<Direction>(d)) == square) {
        next = square;
        hand.dir = static_cast<int8_t>(d);
      }
    }
  } else if (step(game, hand.at, static_cast<Direction>(hand.dir)) == square) {
    next = square;
  }
  if (next < 0) return TapResult::Ignored;
  if (!canDropOn(game, hand, next, left)) {
    if (hand.count == 0) hand.dir = -1;
    return TapResult::Ignored;
  }
  if (hand.count >= 8) return TapResult::Ignored;
  hand.drops[hand.count++] = 1;
  hand.at = static_cast<uint8_t>(next);
  return finish(game, hand, move);
}

void preview(const Game& game, const Hand& hand, Game& out) {
  out = game;
  if (!hand.holding) return;
  Square& from = out.sq[hand.from];
  const Stone moving = from.top;
  uint64_t lifted = from.colors >> (from.height - hand.pick);
  from.height = static_cast<uint8_t>(from.height - hand.pick);
  from.colors &= from.height == 0 ? 0 : ((1ull << from.height) - 1);
  from.top = Stone::Flat;
  int at = hand.from;
  int left = hand.pick;
  for (int i = 0; i < hand.count; ++i) {
    at = step(out, at, static_cast<Direction>(hand.dir));
    if (at < 0) return;
    Square& t = out.sq[at];
    const int drop = hand.drops[i];
    t.colors |= (lifted & ((1ull << drop) - 1)) << t.height;
    t.height = static_cast<uint8_t>(t.height + drop);
    lifted >>= drop;
    left -= drop;
    t.top = left == 0 ? moving : Stone::Flat;
  }
}

uint64_t handColors(const Game& game, const Hand& hand) {
  if (!hand.holding) return 0;
  const Square& from = game.sq[hand.from];
  uint64_t lifted = from.colors >> (from.height - hand.pick);
  int dropped = 0;
  for (int i = 0; i < hand.count; ++i) dropped += hand.drops[i];
  return lifted >> dropped;
}

Stone handTop(const Game& game, const Hand& hand) { return hand.holding ? game.sq[hand.from].top : Stone::Flat; }

}  // namespace tak
