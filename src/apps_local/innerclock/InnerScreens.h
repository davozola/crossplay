#pragma once

// INNER CLOCK on screen. One screen, three states, a freestanding builder.

#include "../ui/ToyboxScreen.h"
#include "InnerCore.h"

namespace innerui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionTarget = 1,
};

struct Model {
  innerclock::State state = innerclock::State::Ready;
  int targetIndex = 0;
  uint32_t elapsedMs = 0;
  int32_t diffMs = 0;
  bool newBest = false;
  const innerclock::Record* record = nullptr;
};

// The big tap area, shared by drawing and hit-testing.
fui::Rect padRect(const fui::DeviceContext& device);

void build(toybox::Screen& screen, const Model& model);

}  // namespace innerui
