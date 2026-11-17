#pragma once

// A folder as its reader sees it: tabs for the kinds of thing in it, rows that
// say what each thing is, and the sheet a long press opens. Drawn here; which
// rows a tab holds is the Activity's to say.

#include <Icon.h>

#include <cstdint>

#include "ui/ToyboxScreen.h"

class GfxRenderer;

namespace shelfcat {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionTab = 20,  // value: the tab
  ActionSheetOpen = 21,
  ActionSheetFavourite = 22,
  ActionSheetHide = 23,
};

constexpr int kRowsPerPage = 8;
// Tabs a folder's own kinds are flanked by: what you use, first, and everything, last.
constexpr const char* kHomeTab = "INICIO";
constexpr const char* kAllTab = "TODO";

struct Row {
  const char* title = "";
  const char* blurb = "";
  const freeink::Icon* icon = nullptr;
  bool star = false;
  int16_t value = 0;  // handed back with ActionOpen
};

struct ListModel {
  const char* title = "";  // the folder's, shouted
  const freeink::Icon* mark = nullptr;
  const char* const* kinds = nullptr;
  int kindCount = 0;
  int tab = 0;  // 0 home, 1..kindCount a kind, kindCount + 1 everything
  const Row* rows = nullptr;
  int rowCount = 0;
  int page = 0;
  int pageCount = 1;
};

void drawList(toybox::Screen& screen, GfxRenderer& renderer, const ListModel& model);

struct SheetModel {
  const char* title = "";
  const char* about = "";
  const char* kind = "";
  bool online = false;
  bool favourite = false;
  // The item's picture, shelfshots-sized, one bit a pixel; null for none.
  const uint8_t* picture = nullptr;
};

void drawSheet(toybox::Screen& screen, GfxRenderer& renderer, const SheetModel& model);

}  // namespace shelfcat
