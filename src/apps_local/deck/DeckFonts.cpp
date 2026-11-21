#include "DeckFonts.h"

#include <EpdFont.h>
#include <EpdFontFamily.h>

#include "fonts/deck_italic_16.h"
#include "fonts/deck_italic_24.h"
#include "fonts/deck_numeral_52.h"

namespace deckfonts {
namespace {

EpdFont numeral52(&deck_numeral_52);
EpdFont italic24(&deck_italic_24);
EpdFont italic16(&deck_italic_16);
EpdFontFamily numeralFamily(&numeral52);
EpdFontFamily italicFamily(&italic24);
EpdFontFamily italicSmallFamily(&italic16);

}  // namespace

void ensure(GfxRenderer& renderer) {
  renderer.insertFont(kNumeralFontId, numeralFamily);
  renderer.insertFont(kItalicFontId, italicFamily);
  renderer.insertFont(kItalicSmallFontId, italicSmallFamily);
}

}  // namespace deckfonts
