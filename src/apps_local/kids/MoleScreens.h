#pragma once

// EL TALP on screen: the page where a grown-up sets it up, the meadow, and
// the celebration every ten stars.

#include <GfxRenderer.h>

#include "../ui/ToyboxScreen.h"
#include "MoleCore.h"

namespace moleui {

namespace fui = freeink::ui;

// Above KidsScreens' own actions, which share the frame.
enum : fui::ActionId {
  ActionHoles = 20,
  ActionWho,
  ActionHides,
  ActionSound,
  ActionPlay,
};

// Where the holes are. One function for the drawing and the finger.
struct Meadow {
  int columns, rows;
  int cell;          // the picture's side
  int stepX, stepY;  // a hole's whole slot, all of it a target
  int left, top;
};
Meadow meadowFor(const mole::Config& config, int width, int height);
int holeAt(const Meadow& meadow, int x, int y);  // -1 off the meadow

// The small mole that stands for the game on ILYA's menu, centred on (cx, cy).
void drawIcon(GfxRenderer& renderer, int cx, int cy);

void buildSetup(toybox::Screen& screen, GfxRenderer& renderer, const mole::Config& config);
void drawMeadow(toybox::Screen& screen, GfxRenderer& renderer, const mole::Game& game);
void drawParty(toybox::Screen& screen, GfxRenderer& renderer, const mole::Game& game);

}  // namespace moleui
