#pragma once

// Tak's navigation and the hand: how taps on the board become a move.
// Freestanding: no renderer, no Activity, so host-tests/tak drives it.
//
// A placement is one tap on an empty square, with whichever stone the tray
// has selected. A slide is built the way playtak's board builds it:
//
//   tap your stack        lift min(height, size) pieces
//   tap it again          put one back (down to none, which cancels)
//   tap a neighbour       drop one there; that fixes the direction
//   tap the same square   drop one more there
//   tap the next one on   carry on and drop one
//
// and the move is played the moment the hand is empty.

#include <cstdint>

#include "TakCore.h"

namespace tak {

enum class Screen : uint8_t { Menu, HowTo, Board, Result };

constexpr Screen back(const Screen screen) {
  switch (screen) {
    case Screen::Menu:
      return Screen::Menu;
    case Screen::HowTo:
      return Screen::Menu;
    case Screen::Board:
      return Screen::Menu;
    case Screen::Result:
      return Screen::Menu;
  }
  return Screen::Menu;
}

constexpr bool leavesApp(const Screen screen) { return screen == Screen::Menu; }

struct Hand {
  bool holding = false;
  uint8_t from = 0;
  uint8_t pick = 0;
  // -1 until the first drop chooses it.
  int8_t dir = -1;
  uint8_t count = 0;
  uint8_t drops[8] = {};
  // Where the last drop landed, or `from` before the first.
  uint8_t at = 0;
};

inline int inHand(const Hand& hand) {
  int dropped = 0;
  for (int i = 0; i < hand.count; ++i) dropped += hand.drops[i];
  return hand.pick - dropped;
}

enum class TapResult : uint8_t {
  // Nothing changed; the screen must not repaint.
  Ignored,
  // The hand changed; repaint, no move yet.
  Changed,
  // `move` is complete and legal; the caller plays it.
  Played,
};

// Whether the tray may offer `stone` right now.
inline bool trayAvailable(const Game& game, const Stone stone) {
  if (openingPly(game)) return stone == Stone::Flat;
  if (stone == Stone::Cap) return game.caps[game.turn] > 0;
  return game.stones[game.turn] > 0;
}

// What the tray selects at the start of a turn: a flat, unless there are none
// left and only the capstone can be placed.
inline Stone defaultTray(const Game& game) { return trayAvailable(game, Stone::Flat) ? Stone::Flat : Stone::Cap; }

// One tap on `square` while it is the human's turn. `stone` is the tray's
// choice for a placement.
TapResult tapSquare(const Game& game, Hand& hand, int square, Stone stone, Move& move);

// The position as it looks with the hand's drops so far made and the lifted
// pieces off their stack, for drawing a slide in progress.
void preview(const Game& game, const Hand& hand, Game& out);

// The colours of the pieces still in the hand, bottom first, as bits; and the
// stone on top of them. For the tray.
uint64_t handColors(const Game& game, const Hand& hand);
Stone handTop(const Game& game, const Hand& hand);

}  // namespace tak
