#pragma once

// The state half of HOW TO PLAY (see ToyboxHowTo.h), on its own so an
// Activity's header can hold a State without pulling in the drawing half,
// which needs the theme headers a .cpp includes first.

#include "ToyboxScreen.h"

namespace toyboxhowto {

namespace fui = freeink::ui;

// Above any game's own actions, which count up from 1.
constexpr fui::ActionId kOpen = 240;
constexpr fui::ActionId kNext = 241;

struct Guide {
  const char* const* pages;
  int count;
};

struct State {
  bool open = false;
  int page = 0;
  void start() {
    open = true;
    page = 0;
  }
  // Back closes it from any page.
  void back() { open = false; }
  // A routed tap while the guide is up. True when it was the guide's own.
  bool tap(const fui::ActionEvent& event, const Guide& guide) {
    if (event.action != kNext) return false;
    if (page + 1 < guide.count) {
      ++page;
    } else {
      open = false;
    }
    return true;
  }
};

}  // namespace toyboxhowto
