#pragma once

// HOW TO PLAY, for the games that share one shape of it: a few pages of plain
// rules, one at a time, behind a button on the game's first screen. Header
// only. A game keeps a State, shows a button that sends kOpen, and hands its
// taps and its Back here first while the guide is up.
//

// Include after ToyboxTheme.h and ToyboxFormat.h, as the screen files do.

#include <cstdio>

#include "ToyboxFormat.h"
#include "ToyboxHowToState.h"

namespace toyboxhowto {

// The button that opens it, for a game's menu to place.
inline void button(toybox::Screen& screen, const fui::Rect& r, const char* text = "HOW TO PLAY") {
  fui::ButtonProps b;
  b.label = text;
  b.action = kOpen;
  b.styles = toybox::rowStyles();
  b.text = toybox::buttonText(screen.theme());
  b.radius = toybox::kPillRadius / 2;
  screen.button(b, r);
}

inline void build(toybox::Screen& screen, const Guide& guide, const State& state) {
  const int page = state.page < 0 ? 0 : (state.page >= guide.count ? guide.count - 1 : state.page);
  char progress[2 * toybox::kIntChars + 5];
  std::snprintf(progress, sizeof(progress), "%d OF %d", page + 1, guide.count);
  fui::HeaderProps header;
  header.title = "HOW TO PLAY";
  header.rightLabel = progress;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);

  const fui::DeviceContext device = screen.device();
  const int16_t x = toybox::kMargin;
  const int16_t inner = static_cast<int16_t>(device.width - toybox::kMargin * 2);
  const int16_t top = static_cast<int16_t>(toybox::kChromeHeight + 28);
  const int16_t buttonY = static_cast<int16_t>(device.height - 40 - 6 - toybox::kPillHeight);
  fui::TextStyle body;
  body.font = toybox::kUiFont;
  body.align = fui::TextAlign::Left;
  body.maxLines = 16;
  screen.target().text(fui::makeRect(x, top, inner, static_cast<int16_t>(buttonY - top - 12)), guide.pages[page], body);

  fui::ButtonProps next;
  next.label = page + 1 < guide.count ? "NEXT" : "GOT IT";
  next.action = kNext;
  next.styles = toybox::rowStyles();
  next.radius = toybox::kPillRadius / 2;
  next.state = fui::StateSelected;
  screen.button(next, fui::makeRect(x, buttonY, inner, toybox::kPillHeight));
}

}  // namespace toyboxhowto
