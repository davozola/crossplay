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

// The cuts a page is tried in, largest first: the phrase, its Spanish, the tip.
struct Tier {
  int ca;
  int es;
  int tip;
};
constexpr Tier kTiers[] = {
    {NOTOSERIF_18_FONT_ID, NOTOSERIF_16_FONT_ID, NOTOSERIF_12_FONT_ID},
    {NOTOSERIF_18_FONT_ID, NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID},
    {NOTOSERIF_16_FONT_ID, NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID},
    {NOTOSERIF_14_FONT_ID, NOTOSERIF_12_FONT_ID, NOTOSERIF_12_FONT_ID},
};
constexpr int kTierCount = sizeof(kTiers) / sizeof(kTiers[0]);
constexpr int kGap = 20;     // between the phrase and its Spanish, either side of a rule
constexpr int kTipGap = 22;  // over the tip

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
  dita::open(state_, today());
  saveState();
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
  uint8_t bytes[dita::kStateBytes] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kSavePath) && Storage.openFileForRead("PARLEM", kSavePath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  dita::unpackState(bytes, got, state_);
}

void ParlemActivity::saveState() {
  uint8_t bytes[dita::kStateBytes];
  dita::packState(state_, bytes);
  HalFile file;
  if (!Storage.openFileForWrite("PARLEM", kSavePath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

void ParlemActivity::turn(const int by) {
  state_.index = dita::step(state_.index, by);
  requestUpdate();
}

void ParlemActivity::restyle(const int by) {
  state_.style = dita::stepStyle(state_.style, by);
  cleanNext_ = true;
  saveState();
  requestUpdate();
}

void ParlemActivity::loop() {
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

struct ParlemActivity::Set {
  Tier tier{};
  std::vector<std::string> ca;
  std::vector<std::string> es;
  std::vector<std::string> tip;
  int height = 0;  // of the three with their gaps
};

// The page in the largest cuts whose lines fit `width` and whose block fits
// `room`. The phrase is bold in every style, so it is measured bold.
bool ParlemActivity::set(const int width, const int room, Set& out) const {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  for (int i = 0; i < kTierCount; ++i) {
    out.tier = kTiers[i];
    wrap(renderer, out.tier.ca, Face::BOLD, p.ca, width, out.ca);
    wrap(renderer, out.tier.es, Face::REGULAR, p.es, width, out.es);
    out.tip.clear();
    if (p.tip[0]) wrap(renderer, out.tier.tip, Face::ITALIC, p.tip, width, out.tip);
    out.height = heightOf(renderer, out.tier.ca, out.ca) + 2 * kGap + 3 + heightOf(renderer, out.tier.es, out.es);
    if (!out.tip.empty()) out.height += kTipGap + heightOf(renderer, out.tier.tip, out.tip);
    if (out.height <= room) return true;
  }
  return false;
}

// A white page: the phrase large and bold from the left margin, a short rule,
// the Spanish under it and the tip in italic.
void ParlemActivity::drawEditorial() {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
  const int width = renderer.getScreenWidth() - 2 * kMargin;
  const int height = renderer.getScreenHeight();

  char meta[32];
  std::snprintf(meta, sizeof(meta), "%d / %d", state_.index + 1, parlem::kCount);
  renderer.drawText(toybox::kTileFontId, kMargin, 24, p.topic);
  renderer.drawText(toybox::kTileFontId, kMargin + width - renderer.getTextWidth(toybox::kTileFontId, meta), 24, meta);

  constexpr int kTop = 52;
  Set page;
  set(width, height - kTop - 16, page);
  int y = kTop + (height - kTop - 16 - page.height) / 2;
  if (y < kTop) y = kTop;
  y = lines(renderer, page.tier.ca, Face::BOLD, page.ca, kMargin, y, true, false);
  y += kGap;
  renderer.fillRect(kMargin, y, 96, 3, true);
  y += 3 + kGap;
  y = lines(renderer, page.tier.es, Face::REGULAR, page.es, kMargin, y, true, false);
  if (!page.tip.empty()) lines(renderer, page.tier.tip, Face::ITALIC, page.tip, kMargin, y + kTipGap, true, false);
}

// A black poster: everything centred and in white.
void ParlemActivity::drawCartell() {
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  renderer.fillRect(0, 0, screenW, screenH, true);
  renderer.drawRect(14, 14, screenW - 28, screenH - 28, false);

  Set page;
  set(screenW - 2 * 56, screenH - 2 * 30, page);
  int y = (screenH - page.height) / 2;
  if (y < 24) y = 24;
  y = lines(renderer, page.tier.ca, Face::BOLD, page.ca, 0, y, false, true);
  y += kGap;
  renderer.fillRect(screenW / 2 - 30, y, 60, 3, false);
  y += 3 + kGap;
  y = lines(renderer, page.tier.es, Face::REGULAR, page.es, 0, y, false, true);
  if (!page.tip.empty()) lines(renderer, page.tier.tip, Face::ITALIC, page.tip, 0, y + kTipGap, false, true);
}

// A card from a box of them: its number and topic down a column, the phrase
// beside it with a rule across before the Spanish.
void ParlemActivity::drawDiccionari() {
  const parlem::Phrase& p = parlem::kPhrases[state_.index];
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
  std::snprintf(of, sizeof(of), "DE %d", parlem::kCount);
  renderer.drawText(toybox::kTileFontId, (kColumn - renderer.getTextWidth(toybox::kTileFontId, of)) / 2, 270, of);
  renderer.drawText(toybox::kTileFontId, (kColumn - renderer.getTextWidth(toybox::kTileFontId, p.topic)) / 2, 40,
                    p.topic);
  renderer.fillRect(kColumn, 36, 2, screenH - 72, true);

  const int x = kColumn + 32;
  const int width = screenW - x - 32;
  Set page;
  set(width, screenH - 2 * 24, page);
  int y = (screenH - page.height) / 2;
  if (y < 16) y = 16;
  y = lines(renderer, page.tier.ca, Face::BOLD, page.ca, x, y, true, false);
  y += kGap;
  renderer.fillRect(x, y, width, 1, true);
  y += 3 + kGap;
  y = lines(renderer, page.tier.es, Face::REGULAR, page.es, x, y, true, false);
  if (!page.tip.empty()) lines(renderer, page.tier.tip, Face::ITALIC, page.tip, x, y + kTipGap, true, false);
}

void ParlemActivity::render(RenderLock&&) {
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
