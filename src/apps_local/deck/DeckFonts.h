#pragma once

// The Instrument Serif cuts baked for DECK (fonts/README.md), shared with any
// app that sets itself the same way: VOZ uses them too.

#include <GfxRenderer.h>

namespace deckfonts {

constexpr int kNumeralFontId = 0x70B0'0D01;     // numerals and a colon, 79px tall
constexpr int kItalicFontId = 0x70B0'0D02;      // the italic, 37px capitals
constexpr int kItalicSmallFontId = 0x70B0'0D03;  // the italic, 24px capitals

// Ascender and the height of a capital (or a numeral), from the headers.
struct Cut {
  int ascender;
  int cap;
};
constexpr Cut kNumeralCut{108, 79};
constexpr Cut kItalicCut{50, 37};
constexpr Cut kItalicSmallCut{33, 24};

// Registers the three cuts. Idempotent; call from onEnter() before drawing.
void ensure(GfxRenderer& renderer);

}  // namespace deckfonts
