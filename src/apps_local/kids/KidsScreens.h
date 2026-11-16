#pragma once

// ILYA on screen. Everything is big and nothing is small enough to miss: a
// picture is a whole tile, and a tile is the whole target.

#include <GfxRenderer.h>

#include "../ui/ToyboxScreen.h"
#include "KidsCore.h"

namespace kidsui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionGame = 1,  // value: kids::Game
  ActionOption,    // value: option index
  ActionNext,
  ActionPrevious,
};

// Lends the builder the two cuts above the display one; see weatherui::FaceSwap.
enum class Face : uint8_t { Display, Large, Huge };
struct FaceSwap {
  void* ctx = nullptr;
  void (*bindTitle)(void* ctx, Face face) = nullptr;
};

struct CardModel {
  int picture = 0;
  int index = 0;  // position in the deck, for the counter
  int total = 0;
  char letter = 0;  // non-zero: the Letters game, with this letter shown huge
  FaceSwap faces;
};

struct RoundModel {
  kids::Game game = kids::Game::Find;
  const kids::Round* round = nullptr;
  int stars = 0;  // right answers this visit
  FaceSwap faces;
};

// The mole on the menu is a picture of its own, drawn straight to the panel.
struct MenuModel {
  GfxRenderer* renderer = nullptr;
};

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildCard(toybox::Screen& screen, const CardModel& model);
void buildRound(toybox::Screen& screen, const RoundModel& model);

}  // namespace kidsui
