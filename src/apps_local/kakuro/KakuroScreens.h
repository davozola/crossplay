#pragma once

// Kakuro on screen. Freestanding builders over plain models.
//
// The grid and the digit pad are hit-tested arithmetically from the same
// geometry that draws them (a 10x10 grid is far past the 24-slot interaction
// buffer); only the menu rows and NEXT are registered controls.

#include "../ui/ToyboxScreen.h"
#include "KakuroCore.h"

namespace kakuroui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenuRow = 1,
  ActionHowToNext = 2,
  ActionNext = 3,
};

enum class MenuRow : int { Continue = 0, Small, Medium, Large, HowTo, Count };

struct MenuModel {
  bool canContinue = false;
  kakuro::Size continueSize = kakuro::Size::Small;
  int solved[kakuro::kSizeCount] = {};
  int total[kakuro::kSizeCount] = {};
};

struct HowToModel {
  int page = 0;
};

struct BoardModel {
  const kakuro::Layout* layout = nullptr;
  const kakuro::Progress* progress = nullptr;
  int selected = -1;
  bool notes = false;
  bool solved = false;
  int number = 0;  // 1-based, for the header
  kakuro::Size size = kakuro::Size::Small;
};

// Pad keys: 1-9 are digits, these two are the rest.
constexpr int kKeyNotes = 10;
constexpr int kKeyErase = 11;

fui::Rect cellRect(const fui::DeviceContext& device, int side, int cell);
int cellAt(const fui::DeviceContext& device, int side, int x, int y);
fui::Rect keyRect(const fui::DeviceContext& device, int key);
int keyAt(const fui::DeviceContext& device, int x, int y);

void buildMenu(toybox::Screen& screen, const MenuModel& model);
void buildHowTo(toybox::Screen& screen, const HowToModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);

int howToPages();
const char* sizeName(kakuro::Size size);

}  // namespace kakuroui
