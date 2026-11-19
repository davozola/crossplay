#pragma once

// Tak on screen. Freestanding builders over plain models.

#include "../ui/ToyboxScreen.h"
#include "TakCore.h"
#include "TakEngine.h"
#include "TakFlow.h"

namespace takui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionHowToNext = 2,
  ActionTray = 3,
  ActionCancel = 4,
  ActionAgain = 5,
  ActionDone = 6,
  ActionUndo = 7,
};

enum class MenuRow : int { Resume = 0, PlayComputer, PlayTwo, Size, Level, HowTo, Count };

struct MenuModel {
  bool canResume = false;
  int size = 5;
  tak::Level level = tak::Level::Medium;
  int played = 0;
  int won = 0;
  // Against the chosen level.
  int levelWon = 0;
  int levelLost = 0;
  // The game waiting under RESUME, drawn small as the menu's ornament.
  const tak::Game* current = nullptr;
  bool vsComputer = true;
  uint8_t human = tak::kWhite;
};

struct HowToModel {
  int page = 0;
};

struct BoardModel {
  // The position as drawn: the real one with any slide in progress applied.
  tak::Game shown{};
  // The real one, for the reserves and whose turn it is.
  tak::Game game{};
  bool vsComputer = true;
  uint8_t human = tak::kWhite;
  bool thinking = false;
  bool canUndo = false;
  tak::Stone tray = tak::Stone::Flat;
  bool holding = false;
  uint8_t from = 0;
  uint8_t at = 0;
  int handCount = 0;
  uint64_t handColors = 0;
  tak::Stone handTop = tak::Stone::Flat;
  // Squares the last move touched, as a bitmask.
  uint64_t lastMove = 0;
  // The last move in PTN, or empty.
  char lastPtn[12] = {};
};

struct ResultModel {
  tak::Game game{};
  bool vsComputer = true;
  uint8_t human = tak::kWhite;
  uint64_t road = 0;
};

// Board geometry, shared by drawing and hit-testing.
fui::Rect squareRect(const fui::DeviceContext& device, int size, int square);
// The square under a tap, or -1.
int squareAt(const fui::DeviceContext& device, int size, int x, int y);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);
void buildResult(toybox::Screen& screen, const ResultModel& model);

int howToPages();

}  // namespace takui
