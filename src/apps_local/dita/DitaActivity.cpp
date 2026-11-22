#include "DitaActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "../Shelf.h"
#include "../deck/DeckFonts.h"
#include "../ui/SerifFontIds.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTokens.h"

namespace {

constexpr char kSavePath[] = "/.crosspoint/dita.sav";
constexpr int kMargin = 48;
constexpr int kBackSwipeEdge = 28;
constexpr int kColumn = 196;  // Diccionari: the number's column

using Face = EpdFontFamily::Style;

// The device's clock as a day, or -1 while it has never been set.
long today() {
  const time_t now = time(nullptr);
  if (now < 1700000000) return -1;
  struct tm local{};
  localtime_r(&now, &local);
  return dita::dayNumber(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

// Words broken into lines no wider than `width`.
void wrap(const GfxRenderer& r, const int font, const Face face, const char* text, const int width,
          std::vector<std::string>& out) {
  out.clear();
  out.reserve(6);
  std::string line;
  const char* p = text;
  while (*p) {
    while (*p == ' ') ++p;
    const char* end = p;
    while (*end && *end != ' ') ++end;
    if (end == p) break;
    std::string candidate = line;
    if (!candidate.empty()) candidate.push_back(' ');
    candidate.append(p, static_cast<size_t>(end - p));
    if (!line.empty() && r.getTextWidth(font, candidate.c_str(), face) > width) {
      out.push_back(std::move(line));
      line.assign(p, static_cast<size_t>(end - p));
    } else {
      line = std::move(candidate);
    }
    p = end;
  }
  if (!line.empty()) out.push_back(std::move(line));
}

// The largest of `fonts` in which the text takes no more than `maxLines`; the
// last one is used whatever it takes.
int fit(const GfxRenderer& r, const int* fonts, const int count, const Face face, const char* text, const int width,
        const int maxLines, std::vector<std::string>& out) {
  for (int i = 0; i < count; ++i) {
    wrap(r, fonts[i], face, text, width, out);
    if (static_cast<int>(out.size()) <= maxLines) return fonts[i];
  }
  return fonts[count - 1];
}

// Lines from `top` down, from the left edge or centred on the page; returns
// where the next thing goes.
int lines(const GfxRenderer& r, const int font, const Face face, const std::vector<std::string>& text, const int x,
          int top, const bool ink, const bool centred, const int leading = 0) {
  const int step = r.getLineHeight(font) + leading;
  for (const std::string& line : text) {
    const int at = centred ? (r.getScreenWidth() - r.getTextWidth(font, line.c_str(), face)) / 2 : x;
    r.drawText(font, at, top, line.c_str(), ink, face);
    top += step;
  }
  return top;
}

int heightOf(const GfxRenderer& r, const int font, const std::vector<std::string>& text, const int leading = 0) {
  return static_cast<int>(text.size()) * (r.getLineHeight(font) + leading);
}

const char* extraLabel(const dita::Saying& s) { return s.kind == dita::Extra::Literal ? "LITERALMENTE" : "NOTA"; }

constexpr int kBig[] = {NOTOSERIF_18_FONT_ID, NOTOSERIF_16_FONT_ID, NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID};
constexpr int kMid[] = {NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID};

}  // namespace

std::unique_ptr<Activity> DitaActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DitaActivity>(renderer, mappedInput);
}

void DitaActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  deckfonts::ensure(renderer);
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  loadState();
  dita::open(state_, today());
  saveState();
  cleanNext_ = true;
  requestUpdate();
}

void DitaActivity::onExit() {
  saveState();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  Activity::onExit();
}

void DitaActivity::loadState() {
  uint8_t bytes[dita::kStateBytes] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kSavePath) && Storage.openFileForRead("DITA", kSavePath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  dita::unpackState(bytes, got, state_);
}

void DitaActivity::saveState() {
  uint8_t bytes[dita::kStateBytes];
  dita::packState(state_, bytes);
  HalFile file;
  if (!Storage.openFileForWrite("DITA", kSavePath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

void DitaActivity::turn(const int by) {
  state_.index = dita::step(state_.index, by);
  requestUpdate();
}

void DitaActivity::restyle(const int by) {
  state_.style = dita::stepStyle(state_.style, by);
  cleanNext_ = true;
  saveState();
  requestUpdate();
}

void DitaActivity::loop() {
  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py)) pressX_ = px;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // A swipe to the right from the middle of the page is the one before, not
    // the way out.
    if (mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge) {
      pressX_ = -1;
      turn(-1);
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  switch (mappedInput.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
      turn(1);
      return;
    case MappedInputManager::SwipeDir::Right:
      turn(-1);
      return;
    case MappedInputManager::SwipeDir::Up:
      restyle(1);
      return;
    case MappedInputManager::SwipeDir::Down:
      restyle(-1);
      return;
    default:
      break;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    turn(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    turn(1);
    return;
  }

  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y)) turn(1);
}

// A white page: the saying in a large italic from the left margin, a short
// rule, and the Spanish under it.
void DitaActivity::drawEditorial() {
  const dita::Saying& s = dita::kSayings[state_.index];
  const int width = renderer.getScreenWidth() - 2 * kMargin;
  const int height = renderer.getScreenHeight();

  char meta[32];
  std::snprintf(meta, sizeof(meta), "%d / %d", state_.index + 1, dita::kCount);
  renderer.drawText(toybox::kTileFontId, kMargin, 26, "DITA DEL DIA");
  renderer.drawText(toybox::kTileFontId, kMargin + width - renderer.getTextWidth(toybox::kTileFontId, meta), 26, meta);

  std::vector<std::string> ca;
  std::vector<std::string> es;
  std::vector<std::string> extra;
  const int caFont = fit(renderer, kBig, 4, Face::ITALIC, s.ca, width, 3, ca);
  const int esFont = fit(renderer, kMid, 2, Face::REGULAR, s.es, width, 2, es);
  if (s.kind != dita::Extra::None) wrap(renderer, toybox::kSerifTileFontId, Face::REGULAR, s.extra, width, extra);

  const int ruleGap = 22;
  int total = heightOf(renderer, caFont, ca) + 2 * ruleGap + 3 + heightOf(renderer, esFont, es);
  if (!extra.empty())
    total += 18 + renderer.getLineHeight(toybox::kTileFontId) + heightOf(renderer, toybox::kSerifTileFontId, extra);

  int y = 60 + (height - 60 - 24 - total) / 2;
  if (y < 60) y = 60;
  y = lines(renderer, caFont, Face::ITALIC, ca, kMargin, y, true, false);
  y += ruleGap;
  renderer.fillRect(kMargin, y, 96, 3, true);
  y += 3 + ruleGap;
  y = lines(renderer, esFont, Face::REGULAR, es, kMargin, y, true, false);
  if (!extra.empty()) {
    y += 18;
    renderer.drawText(toybox::kTileFontId, kMargin, y, extraLabel(s));
    y += renderer.getLineHeight(toybox::kTileFontId);
    lines(renderer, toybox::kSerifTileFontId, Face::REGULAR, extra, kMargin, y, true, false);
  }
}

// A black poster: the saying bold and centred, everything in white.
void DitaActivity::drawCartell() {
  const dita::Saying& s = dita::kSayings[state_.index];
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int width = screenW - 2 * 64;
  renderer.fillRect(0, 0, screenW, screenH, true);
  // A white line just inside the edge, as a poster's border.
  renderer.drawRect(14, 14, screenW - 28, screenH - 28, false);

  std::vector<std::string> ca;
  std::vector<std::string> es;
  std::vector<std::string> extra;
  const int caFont = fit(renderer, kBig, 4, Face::BOLD, s.ca, width, 3, ca);
  const int esFont = fit(renderer, kMid, 2, Face::ITALIC, s.es, width, 2, es);
  if (s.kind == dita::Extra::Literal) {
    const std::string quoted = std::string("\xC2\xAB") + s.extra + "\xC2\xBB";
    wrap(renderer, toybox::kSerifTileFontId, Face::REGULAR, quoted.c_str(), width, extra);
  } else if (s.kind == dita::Extra::Note) {
    wrap(renderer, toybox::kSerifTileFontId, Face::REGULAR, s.extra, width, extra);
  }

  const int gap = 26;
  int total = heightOf(renderer, caFont, ca) + gap + heightOf(renderer, esFont, es);
  if (!extra.empty()) total += 14 + heightOf(renderer, toybox::kSerifTileFontId, extra);
  int y = (screenH - total) / 2;
  if (y < 30) y = 30;
  y = lines(renderer, caFont, Face::BOLD, ca, 0, y, false, true);
  renderer.fillRect(screenW / 2 - 30, y + gap / 2 - 1, 60, 2, false);
  y += gap;
  y = lines(renderer, esFont, Face::ITALIC, es, 0, y, false, true);
  if (!extra.empty()) lines(renderer, toybox::kSerifTileFontId, Face::REGULAR, extra, 0, y + 14, false, true);
}

// A dictionary's entry: its number down a column, then the saying as the
// headword and each sense under its own small heading.
void DitaActivity::drawDiccionari() {
  const dita::Saying& s = dita::kSayings[state_.index];
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();

  char number[8];
  std::snprintf(number, sizeof(number), "%d", state_.index + 1);
  const int numberW = renderer.getTextWidth(deckfonts::kNumeralFontId, number);
  if (numberW <= kColumn - 24) {
    renderer.drawText(deckfonts::kNumeralFontId, (kColumn - numberW) / 2, 150, number);
  } else {
    const int w = renderer.getTextWidth(NOTOSERIF_18_FONT_ID, number, Face::BOLD);
    renderer.drawText(NOTOSERIF_18_FONT_ID, (kColumn - w) / 2, 170, number, true, Face::BOLD);
  }
  char of[16];
  std::snprintf(of, sizeof(of), "DE %d", dita::kCount);
  renderer.drawText(toybox::kTileFontId, (kColumn - renderer.getTextWidth(toybox::kTileFontId, of)) / 2, 270, of);
  renderer.drawText(toybox::kTileFontId, (kColumn - renderer.getTextWidth(toybox::kTileFontId, "DITA")) / 2, 40,
                    "DITA");
  renderer.fillRect(kColumn, 36, 2, screenH - 72, true);

  const int x = kColumn + 36;
  const int width = screenW - x - 40;
  constexpr int kHead[] = {toybox::kSerifTitleFontId, NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID};
  std::vector<std::string> ca;
  std::vector<std::string> es;
  std::vector<std::string> extra;
  const int caFont = fit(renderer, kHead, 3, Face::REGULAR, s.ca, width, 2, ca);
  const int esFont = fit(renderer, kMid, 2, Face::REGULAR, s.es, width, 2, es);
  if (s.kind != dita::Extra::None) wrap(renderer, toybox::kSerifTileFontId, Face::REGULAR, s.extra, width, extra);

  const int labelH = renderer.getLineHeight(toybox::kTileFontId) + 4;
  int total = heightOf(renderer, caFont, ca) + 16 + 1 + 20 + labelH + heightOf(renderer, esFont, es);
  if (!extra.empty()) total += 18 + labelH + heightOf(renderer, toybox::kSerifTileFontId, extra);
  int y = (screenH - total) / 2;
  if (y < 30) y = 30;
  y = lines(renderer, caFont, Face::REGULAR, ca, x, y, true, false);
  y += 16;
  renderer.fillRect(x, y, width, 1, true);
  y += 1 + 20;
  renderer.drawText(toybox::kTileFontId, x, y, "CASTELLANO");
  y += labelH;
  y = lines(renderer, esFont, Face::REGULAR, es, x, y, true, false);
  if (!extra.empty()) {
    y += 18;
    renderer.drawText(toybox::kTileFontId, x, y, extraLabel(s));
    y += labelH;
    lines(renderer, toybox::kSerifTileFontId, Face::REGULAR, extra, x, y, true, false);
  }
}

void DitaActivity::render(RenderLock&&) {
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.clearScreen();
  if (state_.style == dita::Style::Cartell) {
    drawCartell();
  } else if (state_.style == dita::Style::Diccionari) {
    drawDiccionari();
  } else {
    drawEditorial();
  }
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
