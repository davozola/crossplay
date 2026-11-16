#pragma once

// The handful of drawing helpers the reference tools share (FM Synth Lab,
// Music Theory Lab): ink-centred labels, a tab row, and a "name < value >"
// stepper row. Header-only, over toybox::Screen.

#include "ToyboxScreen.h"

namespace toyboxlab {

namespace fui = freeink::ui;

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = toybox::kPillRadius / 2;

inline fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

inline void box(toybox::Screen& screen, const int x, const int y, const int w, const int h, const fui::Paint& paint,
                const int radius = 0) {
  if (w <= 0 || h <= 0) return;
  screen.target().fill(rect(x, y, w, h), paint, static_cast<uint8_t>(radius));
}

inline void ink(toybox::Screen& screen, const int x, const int y, const int w, const int h) {
  box(screen, x, y, w, h, fui::Paint::solid(fui::Color::Black));
}

// Text whose ink (cap height) is centred in `r` vertically.
inline void label(toybox::Screen& screen, const fui::Rect& r, const char* text, const fui::FontId font,
                  const toybox::CutMetrics& cut, const fui::TextAlign align = fui::TextAlign::Center,
                  const fui::Color color = fui::Color::Black) {
  fui::TextStyle s;
  s.font = font;
  s.align = align;
  s.color = color;
  s.maxLines = 1;
  screen.target().text(toybox::inkCentred(r, cut), text, s);
}

// The small cut, ink top at `y`.
inline void caption(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
                    const fui::TextAlign align = fui::TextAlign::Left, const fui::Color color = fui::Color::Black) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align, color);
}

// The body cut, ink top at `y`.
inline void value(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
                  const fui::TextAlign align = fui::TextAlign::Left, const fui::Color color = fui::Color::Black) {
  label(screen, rect(x, y, w, toybox::kUiCut.inkHeight), text, toybox::kUiFont, toybox::kUiCut, align, color);
}

inline void paragraph(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
                      const int lines = 3) {
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Left;
  s.maxLines = static_cast<uint8_t>(lines);
  screen.target().text(rect(x, y, w, toybox::kTileCut.lineHeight * lines), text, s);
}

inline void chrome(toybox::Screen& screen, const char* title) {
  fui::HeaderProps header;
  header.title = title;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);
}

inline void button(toybox::Screen& screen, const fui::Rect& r, const char* text, const fui::ActionId action,
                   const int actionValue, const bool selected = false, const bool small = true) {
  fui::ButtonProps b;
  b.label = text;
  b.action = action;
  b.value = static_cast<int16_t>(actionValue);
  b.styles = toybox::rowStyles();
  if (small) b.text = toybox::buttonText(screen.theme());
  b.radius = kRadius;
  b.state = selected ? fui::StateSelected : fui::StateNormal;
  screen.button(b, r);
}

// A row of equal tabs across the page, the current one solid. Returns the y
// under them.
inline int tabs(toybox::Screen& screen, const int y, const char* const* names, const int count, const int current,
                const fui::ActionId action) {
  const int inner = screen.device().width - toybox::kMargin * 2;
  const int gap = 6;
  const int w = (inner - gap * (count - 1)) / count;
  for (int i = 0; i < count; ++i) {
    button(screen, rect(toybox::kMargin + i * (w + gap), y, w, 44), names[i], action, i, i == current);
  }
  return y + 44;
}

// "NAME      [<]  value  [>]": a caption, the value in the body cut, and a
// button either side of it that steps it. `action` gets -1 or +1 as its value
// offset from `base`: the left button sends base, the right base + 1.
inline int stepper(toybox::Screen& screen, const int y, const char* name, const char* text, const fui::ActionId action,
                   const int base) {
  const int width = screen.device().width;
  const int h = 48;
  const int buttonW = 56;
  const int right = width - toybox::kMargin;
  const int valueW = 210;
  caption(screen, toybox::kMargin, y + (h - toybox::kTileCut.inkHeight) / 2, 110, name);
  button(screen, rect(right - buttonW * 2 - valueW, y, buttonW, h), "<", action, base, false, false);
  value(screen, right - buttonW - valueW, y + (h - toybox::kUiCut.inkHeight) / 2, valueW, text, fui::TextAlign::Center);
  button(screen, rect(right - buttonW, y, buttonW, h), ">", action, base + 1, false, false);
  return y + h;
}

}  // namespace toyboxlab
