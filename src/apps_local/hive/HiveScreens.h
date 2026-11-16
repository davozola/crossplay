#pragma once

// HIVE on screen: a menu, and the hive drawn as large as its shape allows with
// the hand of whoever is to move along the foot.

#include "../ui/ToyboxScreen.h"
#include "HiveCore.h"

namespace hiveui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionStart = 1,  // value: 0..2 a level against the device, 3 two players
  ActionContinue,
  ActionHand,  // value: hive::Bug
};

// Where the hive sits and how big a cell is. The activity turns taps into
// cells with the same numbers the drawing used.
struct Layout {
  float size = 40;  // centre to corner
  float originX = 0;
  float originY = 0;
  void centre(int q, int r, int& x, int& y) const;
};
Layout layoutOf(const fui::DeviceContext& device, const hive::Game& game);

struct MenuModel {
  bool canContinue = false;
};

struct PlayModel {
  const hive::Game* game = nullptr;
  const char* status = "";
  const char* modeLabel = "";
  int handColor = 0;                    // whose hand the tray shows
  int selectedPiece = -1;               // on the table or in the hand
  const hive::Move* targets = nullptr;  // where the selected piece may go
  int targetCount = 0;
  int lastPiece = -1;  // the piece that moved last
  bool trayEnabled = true;
};

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildPlay(toybox::Screen& screen, const PlayModel& model);

}  // namespace hiveui
