#pragma once

// The four grid puzzles on screen: one picker and one board page, with a
// drawer per game.

#include "../ui/ToyboxScreen.h"
#include "LogicCore.h"

namespace logicui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionSize = 1,  // value: size index
  ActionReset,
  ActionNext,
};

// Where the board sits. Shared with the activity, which turns a tap into a
// cell, an edge or a bridge with the same numbers the drawing used.
struct Geometry {
  int x = 0;
  int y = 0;
  int cell = 0;
  int side = 0;
};
Geometry boardGeometry(const fui::DeviceContext& device, int side);
// The picker's thirty tiles are too many for the interaction table, so they
// are drawn bare and found by arithmetic: the board under a tap, or -1.
int pickerBoardAt(const fui::DeviceContext& device, int x, int y);

struct PickerModel {
  logic::Game game = logic::Game::Slither;
  int sizeIndex = 0;
  int lastOpened = -1;               // in this size, or -1
  const uint32_t* solved = nullptr;  // one bit a board, for this size
};

struct BoardModel {
  logic::Game game = logic::Game::Slither;
  int sizeIndex = 0;
  int number = 0;
  bool solved = false;
  bool resetArmed = false;
  const logic::Slither* slither = nullptr;
  const logic::Shikaku* shikaku = nullptr;
  const logic::Akari* akari = nullptr;
  const logic::Hashi* hashi = nullptr;
};

void buildPicker(toybox::Screen& screen, const PickerModel& model);
void buildBoard(toybox::Screen& screen, const BoardModel& model);

}  // namespace logicui
