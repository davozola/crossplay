#pragma once

// TRIPLES on screen: one page, the board and what is coming next.

#include "../ui/ToyboxScreen.h"
#include "TriplesCore.h"

namespace triplesui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionNew = 1,
};

// Lends the builder the large cut for the tiles; see weatherui::FaceSwap.
struct FaceSwap {
  void* ctx = nullptr;
  void (*bindTitle)(void* ctx, bool large) = nullptr;
};

struct Model {
  const triples::Game* game = nullptr;
  uint32_t best = 0;
  int entered = -1;  // the cell the last new tile came in at, or -1
  bool over = false;
  bool newArmed = false;
  FaceSwap faces;
};

void build(toybox::Screen& screen, const Model& model);

}  // namespace triplesui
