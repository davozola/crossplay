#pragma once

// Mastermind on screen. Freestanding builders over plain models.
//
// Colours on a one-bit panel are shapes: disc, square, triangle and diamond,
// each solid or hollow. The feedback pegs keep the board game's own code --
// solid for right symbol in the right place, hollow for right symbol elsewhere.

#include "../ui/ToyboxScreen.h"
#include "MastermindCore.h"

namespace mastermindui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionHowToNext = 2,
  ActionGuess = 3,
  ActionClear = 4,
  ActionAgain = 5,
  ActionMenu = 6,
};

enum class MenuRow : int { Continue = 0, Easy, Classic, Hard, HowTo, Count };

struct Stats {
  uint16_t played = 0;
  uint16_t won = 0;
  uint16_t guessesInWins = 0;
};

struct MenuModel {
  bool canContinue = false;
  mastermind::Mode continueMode = mastermind::Mode::Classic;
  Stats stats[mastermind::kModeCount];
};

struct HowToModel {
  int page = 0;
};

struct BoardModel {
  const mastermind::Game* game = nullptr;
};

void drawSymbol(toybox::Screen& screen, const fui::Rect& box, int symbol);

fui::Rect slotRect(const fui::DeviceContext& device, const mastermind::Game& game, int slot);
int slotAt(const fui::DeviceContext& device, const mastermind::Game& game, int x, int y);
fui::Rect paletteRect(const fui::DeviceContext& device, const mastermind::Game& game, int symbol);
int paletteAt(const fui::DeviceContext& device, const mastermind::Game& game, int x, int y);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);

int howToPages();

}  // namespace mastermindui
