#include "ParlemActivity.h"

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

constexpr char kSavePath[] = "/.crosspoint/parlem.sav";
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
  return parlem::dayNumber(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
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

// The cuts a page is tried in, largest first: the phrase and its Spanish.
struct Tier {
  int ca;
  int es;
};
constexpr Tier kTiers[] = {
    {NOTOSERIF_18_FONT_ID, NOTOSERIF_16_FONT_ID},
    {NOTOSERIF_18_FONT_ID, NOTOSERIF_14_FONT_ID},
    {NOTOSERIF_16_FONT_ID, NOTOSERIF_14_FONT_ID},
    {NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID},
};
constexpr int kTierCount = sizeof(kTiers) / sizeof(kTiers[0]);
constexpr int kGap = 20;  // between the phrase and its Spanish, either side of a rule

// The two buttons along the bottom, and the strip they sit in.
constexpr int kBar = 60;
constexpr int kButtonH = 36;
constexpr int kMenuW = 150;
constexpr int kTipW = 84;
constexpr int kTouch = 84;  // how far up from the bottom a tap still means a button

// The menu: a title over two columns of sections.
constexpr int kMenuTop = 62;
constexpr int kMenuCols = 2;
constexpr int kMenuRows = (parlem::kSectionCount + kMenuCols - 1) / kMenuCols;
constexpr int kMenuEdge = 24;
constexpr int kMenuGap = 8;

}  // namespace

std::unique_ptr<Activity> ParlemActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<ParlemActivity>(renderer, mappedInput);
}

void ParlemActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  deckfonts::ensure(renderer);
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  loadState();
  parlem::open(state_, today());
  saveState();
  view_ = View::Page;
  cleanNext_ = true;
  requestUpdate();
}

void ParlemActivity::onExit() {
  saveState();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  Activity::onExit();
}

void ParlemActivity::loadState() {
  uint8_t bytes[parlem::kStateBytes] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kSavePath) && Storage.openFileForRead("PARLEM", kSavePath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  parlem::unpackState(bytes, got, state_);
}

void ParlemActivity::saveState() {
  uint8_t bytes[parlem::kStateBytes];
  parlem::packState(state_, bytes);
  HalFile file;
  if (!Storage.openFileForWrite("PARLEM", kSavePath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

void ParlemActivity::turn(const int by) {
  state_.index = parlem::stepIn(state_.index, state_.section, by);
  requestUpdate();
}

void ParlemActivity::restyle(const int by) {
  state_.style = parlem::stepStyle(state_.style, by);
  cleanNext_ = true;
  saveState();
  requestUpdate();
}

void ParlemActivity::show(const View view) {
  view_ = view;
  cleanNext_ = true;
  requestUpdate();
}

int ParlemActivity::sectionAt(const int x, const int y) const {
  const int cellW = (renderer.getScreenWidth() - 2 * kMenuEdge - (kMenuCols - 1) * kMenuGap) / kMenuCols;
  const int cellH = (renderer.getScreenHeight() - kMenuTop - 12) / kMenuRows;
  if (y < kMenuTop || x < kMenuEdge) return -1;
  const int col = (x - kMenuEdge) / (cellW + kMenuGap);
  const int row = (y - kMenuTop) / cellH;
  if (col >= kMenuCols || row >= kMenuRows) return -1;
  // Down the first column, then down the second.
  const int section = col * kMenuRows + row;
  return section < parlem::kSectionCount ? section : -1;
}

void ParlemActivity::loop() {
  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py)) pressX_ = px;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ != View::Page) {
      show(View::Page);
      return;
    }
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

  int x = 0;
  int y = 0;
  if (view_ == View::Tip) {
    if (mappedInput.wasScreenTapped(x, y) || mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      show(View::Page);
    }
    return;
  }
  if (view_ == View::Menu) {
    if (!mappedInput.wasScreenTapped(x, y)) return;
    const int section = sectionAt(x, y);
    if (section < 0) return;
    parlem::choose(state_, section);
    saveState();
    show(View::Page);
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
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    turn(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    show(View::Tip);
    return;
  }

  if (!mappedInput.wasScreenTapped(x, y)) return;
  // The buttons are small; the corners round them are generous.
  if (y >= renderer.getScreenHeight() - kTouch) {
    if (x <= kMargin + kMenuW + 40) {
      show(View::Menu);
      return;
    }
    if (x >= renderer.getScreenWidth() - kMargin - kTipW - 40) {
      show(View::Tip);
      return;
    }
  }
  turn(1);
}

struct ParlemActivity::Set {
  Tier tier{};
  std::vector<std::string> ca;
  std::vector<std::string> es;
  int height = 0;  // of the two with the rule between them
};

// The page in the largest cuts whose lines fit `width` and whose block fits
// `room`. The phrase is bold in every style, so it is measured bold.
bool ParlemActivity::set(const int width, const int room, Set& out) const {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  for (int i = 0; i < kTierCount; ++i) {
    out.tier = kTiers[i];
    wrap(renderer, out.tier.ca, Face::BOLD, p.ca, width, out.ca);
    wrap(renderer, out.tier.es, Face::REGULAR, p.es, width, out.es);
    out.height = heightOf(renderer, out.tier.ca, out.ca) + 2 * kGap + 3 + heightOf(renderer, out.tier.es, out.es);
    if (out.height <= room) return true;
  }
  return false;
}

// SECCIONES at the left and TIP at the right, outlined, in ink or in white.
void ParlemActivity::drawButtons(const bool ink) {
  const int top = renderer.getScreenHeight() - kBar + (kBar - kButtonH) / 2 - 4;
  const int textTop =
      top + (kButtonH - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight);
  const auto button = [&](const int x, const int w, const char* label) {
    renderer.drawRect(x, top, w, kButtonH, 2, ink);
    renderer.drawText(toybox::kTileFontId, x + (w - renderer.getTextWidth(toybox::kTileFontId, label)) / 2, textTop,
                      label, ink);
  };
  button(kMargin, kMenuW, "SECCIONES");
  button(renderer.getScreenWidth() - kMargin - kTipW, kTipW, "TIP");
}

// A white page: the phrase large and bold from the left margin, a short rule
// and the Spanish under it.
void ParlemActivity::drawEditorial() {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  const int width = renderer.getScreenWidth() - 2 * kMargin;
  const int height = renderer.getScreenHeight();

  char meta[32];
  std::snprintf(meta, sizeof(meta), "%d / %d", parlem::placeIn(state_.index, state_.section) + 1,
                parlem::countIn(state_.section));
  renderer.drawText(toybox::kTileFontId, kMargin, 24, parlem::kTopics[p.topic]);
  renderer.drawText(toybox::kTileFontId, kMargin + width - renderer.getTextWidth(toybox::kTileFontId, meta), 24, meta);

  constexpr int kTop = 52;
  const int room = height - kTop - kBar;
  Set page;
  set(width, room, page);
  int y = kTop + (room - page.height) / 2;
  if (y < kTop) y = kTop;
  y = lines(renderer, page.tier.ca, Face::BOLD, page.ca, kMargin, y, true, false);
  y += kGap;
  renderer.fillRect(kMargin, y, 96, 3, true);
  y += 3 + kGap;
  lines(renderer, page.tier.es, Face::REGULAR, page.es, kMargin, y, true, false);
  drawButtons(true);
}

// A black poster: everything centred and in white.
void ParlemActivity::drawCartell() {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  renderer.fillRect(0, 0, screenW, screenH, true);
  const char* topic = parlem::kTopics[p.topic];
  renderer.drawText(toybox::kTileFontId, (screenW - renderer.getTextWidth(toybox::kTileFontId, topic)) / 2, 24, topic,
                    false);

  constexpr int kTop = 52;
  const int room = screenH - kTop - kBar;
  Set page;
  set(screenW - 2 * kMargin, room, page);
  int y = kTop + (room - page.height) / 2;
  if (y < kTop) y = kTop;
  y = lines(renderer, page.tier.ca, Face::BOLD, page.ca, 0, y, false, true);
  y += kGap;
  renderer.fillRect(screenW / 2 - 30, y, 60, 3, false);
  y += 3 + kGap;
  lines(renderer, page.tier.es, Face::REGULAR, page.es, 0, y, false, true);
  drawButtons(false);
}

// A card from a box of them: its number and topic down a column, the phrase
// beside it with a rule across before the Spanish.
void ParlemActivity::drawDiccionari() {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  const int bottom = screenH - kBar;

  char number[8];
  std::snprintf(number, sizeof(number), "%d", parlem::placeIn(state_.index, state_.section) + 1);
  const int numberW = renderer.getTextWidth(deckfonts::kNumeralFontId, number);
  if (numberW <= kColumn - 24) {
    renderer.drawText(deckfonts::kNumeralFontId, (kColumn - numberW) / 2, 120, number);
  } else {
    const int w = renderer.getTextWidth(NOTOSERIF_18_FONT_ID, number, Face::BOLD);
    renderer.drawText(NOTOSERIF_18_FONT_ID, (kColumn - w) / 2, 140, number, true, Face::BOLD);
  }
  char of[16];
  std::snprintf(of, sizeof(of), "DE %d", parlem::countIn(state_.section));
  renderer.drawText(toybox::kTileFontId, (kColumn - renderer.getTextWidth(toybox::kTileFontId, of)) / 2, 240, of);
  const char* topic = parlem::kTopics[p.topic];
  renderer.drawText(toybox::kTileFontId, (kColumn - renderer.getTextWidth(toybox::kTileFontId, topic)) / 2, 40, topic);
  renderer.fillRect(kColumn, 36, 2, bottom - 36 - 16, true);

  const int x = kColumn + 32;
  const int width = screenW - x - 32;
  const int room = bottom - 2 * 20;
  Set page;
  set(width, room, page);
  int y = 20 + (room - page.height) / 2;
  if (y < 16) y = 16;
  y = lines(renderer, page.tier.ca, Face::BOLD, page.ca, x, y, true, false);
  y += kGap;
  renderer.fillRect(x, y, width, 1, true);
  y += 3 + kGap;
  lines(renderer, page.tier.es, Face::REGULAR, page.es, x, y, true, false);
  drawButtons(true);
}

// The tip: the phrase again, smaller, and what there is to know about it.
void ParlemActivity::drawTip() {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  const int width = renderer.getScreenWidth() - 2 * kMargin;
  const int height = renderer.getScreenHeight();
  renderer.drawText(toybox::kTileFontId, kMargin, 24, "TIP");
  const char* back = "TOCA PARA VOLVER";
  renderer.drawText(toybox::kTileFontId, kMargin + width - renderer.getTextWidth(toybox::kTileFontId, back), 24, back);

  std::vector<std::string> ca;
  std::vector<std::string> es;
  std::vector<std::string> tip;
  wrap(renderer, NOTOSERIF_14_FONT_ID, Face::BOLD, p.ca, width, ca);
  wrap(renderer, NOTOSERIF_12_FONT_ID, Face::ITALIC, p.es, width, es);
  constexpr int kTop = 52;
  const int room = height - kTop - 16;
  constexpr int kCuts[] = {NOTOSERIF_16_FONT_ID, NOTOSERIF_14_FONT_ID};
  int font = kCuts[2];
  int total = 0;
  for (const int cut : kCuts) {
    font = cut;
    wrap(renderer, font, Face::REGULAR, p.tip, width, tip);
    total = heightOf(renderer, NOTOSERIF_14_FONT_ID, ca) + heightOf(renderer, NOTOSERIF_12_FONT_ID, es) + 2 * kGap + 3 +
            heightOf(renderer, font, tip);
    if (total <= room) break;
  }
  int y = kTop + (room - total) / 2;
  if (y < kTop) y = kTop;
  y = lines(renderer, NOTOSERIF_14_FONT_ID, Face::BOLD, ca, kMargin, y, true, false);
  y = lines(renderer, NOTOSERIF_12_FONT_ID, Face::ITALIC, es, kMargin, y, true, false);
  y += kGap;
  renderer.fillRect(kMargin, y, 96, 3, true);
  y += 3 + kGap;
  lines(renderer, font, Face::REGULAR, tip, kMargin, y, true, false);
}

// The sections: all of them first, then each topic with how many it holds.
// The one being kept to is in white on black.
void ParlemActivity::drawMenu() {
  const int screenW = renderer.getScreenWidth();
  const int cellW = (screenW - 2 * kMenuEdge - (kMenuCols - 1) * kMenuGap) / kMenuCols;
  const int cellH = (renderer.getScreenHeight() - kMenuTop - 12) / kMenuRows;
  renderer.drawText(NOTOSERIF_12_FONT_ID, kMenuEdge, 10, "Secciones", true, Face::BOLD);
  const char* back = "ATRÁS PARA VOLVER";
  renderer.drawText(toybox::kTileFontId, screenW - kMenuEdge - renderer.getTextWidth(toybox::kTileFontId, back), 24,
                    back);

  const int textTop = (cellH - kMenuGap - renderer.getLineHeight(NOTOSERIF_12_FONT_ID)) / 2;
  for (int section = 0; section < parlem::kSectionCount; ++section) {
    const int x = kMenuEdge + (section / kMenuRows) * (cellW + kMenuGap);
    const int y = kMenuTop + (section % kMenuRows) * cellH;
    const bool chosen = section == state_.section;
    if (chosen) {
      renderer.fillRect(x, y, cellW, cellH - kMenuGap, true);
    } else {
      renderer.drawRect(x, y, cellW, cellH - kMenuGap, true);
    }
    char count[8];
    std::snprintf(count, sizeof(count), "%d", parlem::countIn(section));
    renderer.drawText(NOTOSERIF_12_FONT_ID, x + 16, y + textTop, parlem::sectionName(section), !chosen);
    renderer.drawText(NOTOSERIF_12_FONT_ID, x + cellW - 16 - renderer.getTextWidth(NOTOSERIF_12_FONT_ID, count),
                      y + textTop, count, !chosen);
  }
}

void ParlemActivity::render(RenderLock&&) {
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.clearScreen();
  if (view_ == View::Tip) {
    drawTip();
  } else if (view_ == View::Menu) {
    drawMenu();
  } else if (state_.style == parlem::Style::Cartell) {
    drawCartell();
  } else if (state_.style == parlem::Style::Diccionari) {
    drawDiccionari();
  } else {
    drawEditorial();
  }
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
