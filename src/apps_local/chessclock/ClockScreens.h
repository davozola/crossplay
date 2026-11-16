#pragma once

// The chess clock on screen. Freestanding builders over plain models.
//
// The clock face is the whole panel: the device lies flat between the two
// players, the bottom half faces one and the top half, drawn upside down,
// faces the other. The running side is solid black, so who is on the clock
// reads from across the table without reading a single digit.

#include "../ui/ToyboxScreen.h"
#include "ClockCore.h"

namespace clockui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionPause = 2,
  ActionNew = 3,
};

enum class MenuRow : int { Start = 0, Resume, Time, Seconds, Count };

struct MenuModel {
  int preset = chessclock::kDefaultPreset;
  bool alwaysSeconds = false;
  bool canResume = false;
};

struct FaceModel {
  chessclock::State state = chessclock::State::Ready;
  int preset = chessclock::kDefaultPreset;
  int active = -1;
  int flagged = -1;
  chessclock::Shown shown[2];
  int moves[2] = {0, 0};
};

// The half of the panel that belongs to `side` (0 bottom, 1 top). Presses
// are hit-tested against these, from the same function that draws them.
fui::Rect halfRect(const fui::DeviceContext& device, int side);
// -1 when the point is in neither half (the strip between them).
int sideAt(const fui::DeviceContext& device, int x, int y);

void presetLabel(int preset, char* out, int outSize);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildFace(toybox::Screen& screen, const FaceModel& model);

}  // namespace clockui
