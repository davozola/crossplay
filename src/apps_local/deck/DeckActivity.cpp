#include "DeckActivity.h"

#include <BatteryMonitor.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../../SilentRestart.h"
#include "../Shelf.h"
#include "../presenter/PresenterLink.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTokens.h"
#include "DeckFonts.h"

namespace radio = presenter::link;

namespace {

// The page is set like a printed table: Instrument Serif, the italic for the
// template's name and the roman for each button, with the chords in Jersey,
// which is the device's own voice.
using deckfonts::Cut;
using deckfonts::kItalicCut;
using deckfonts::kItalicFontId;
using deckfonts::kItalicSmallCut;
using deckfonts::kItalicSmallFontId;
using deckfonts::kNumeralCut;
using deckfonts::kNumeralFontId;
constexpr Cut kLabelCut{toybox::kSerifTileCut.ascender, toybox::kSerifTileCut.inkHeight};
constexpr Cut kLabelSmallCut{toybox::kSerifSmallCut.ascender, toybox::kSerifSmallCut.inkHeight};
constexpr Cut kSmallCut{toybox::kTileCut.ascender, toybox::kTileCut.inkHeight};
constexpr Cut kKeyCut{toybox::kButtonCut.ascender, toybox::kButtonCut.inkHeight};
constexpr Cut kBandCut{toybox::kUiCut.ascender, toybox::kUiCut.inkHeight};
constexpr Cut kBoldCut{toybox::kReadingBoldCut.ascender, toybox::kReadingBoldCut.inkHeight};
constexpr Cut kBoldSmallCut{toybox::kReadingBoldSmallCut.ascender, toybox::kReadingBoldSmallCut.inkHeight};

constexpr char kSavePath[] = "/.crosspoint/deck.sav";
constexpr char kDeviceName[] = "Sticky Deck";
constexpr int kSide = 186;   // the left column
constexpr int kMargin = 18;  // round the page
constexpr int kBackSwipeEdge = 28;
constexpr uint32_t kLitMs = 350;

// Teclado: a strip over five by three key caps.
constexpr int kStrip = 54;
constexpr int kKeyGap = 8;
constexpr int kKeyEdge = 14;
constexpr int kKeyRadius = 12;
// Indice: a band over three columns of five rows.
constexpr int kBand = 58;
constexpr int kIndexEdge = 16;

uint8_t batteryPercent() {
#if defined(SIMULATOR)
  return 100;
#else
  static const BatteryMonitor battery;
  uint16_t percent = 0;
  if (!battery.readPercentageChecked(percent) || percent > 100) return 100;
  return static_cast<uint8_t>(percent);
#endif
}

// Text whose baseline is at `baseline`, from its left edge.
void put(const GfxRenderer& r, const int font, const Cut& cut, const int x, const int baseline, const char* s,
         const bool ink = true) {
  r.drawText(font, x, baseline - cut.ascender, s, ink);
}

// "PESTAÑA SIGUIENTE" as "Pestaña siguiente": capital first letter, the rest
// small. Spanish capitals with accents are two bytes, C3 then 81..9A, and their
// small letters are the same second byte plus 0x20.
void sentenceCase(const char* in, char* out, const size_t cap) {
  size_t n = 0;
  bool first = true;
  for (const char* p = in; *p && n + 2 < cap; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    if (c == 0xC3 && p[1]) {
      unsigned char next = static_cast<unsigned char>(p[1]);
      if (!first && next >= 0x80 && next <= 0x9E) next = static_cast<unsigned char>(next + 0x20);
      out[n++] = static_cast<char>(c);
      out[n++] = static_cast<char>(next);
      ++p;
    } else if (!first && c >= 'A' && c <= 'Z') {
      out[n++] = static_cast<char>(c - 'A' + 'a');
    } else {
      out[n++] = static_cast<char>(c);
    }
    first = false;
  }
  out[n] = 0;
}

// A label in the largest roman cut its widest word fits, as one or two lines.
void label(const GfxRenderer& r, const char* text, const int x, const int baseline, const int width,
           const bool ink = true) {
  char words[2][40] = {};
  // A label that fits whole stays on one line: "Ir a", not "Ir" over "a".
  const char* space = r.getTextWidth(toybox::kSerifTileFontId, text) <= width - 8 ? nullptr : std::strchr(text, ' ');
  if (space) {
    std::snprintf(words[0], sizeof(words[0]), "%.*s", static_cast<int>(space - text), text);
    std::snprintf(words[1], sizeof(words[1]), "%s", space + 1);
  } else {
    std::snprintf(words[0], sizeof(words[0]), "%s", text);
  }
  int font = toybox::kSerifTileFontId;
  Cut cut = kLabelCut;
  for (const char* w : {words[0], words[1]}) {
    if (w[0] && r.getTextWidth(font, w) > width) {
      font = toybox::kSerifSmallFontId;
      cut = kLabelSmallCut;
    }
  }
  const int lead = cut.cap + 9;
  put(r, font, cut, x, baseline, words[0], ink);
  if (words[1][0]) put(r, font, cut, x, baseline + lead, words[1], ink);
}

// Words set on as few lines as they fit, at most `maxLines`; a word wider than
// the line is left to run. Returns how many lines it made.
int wrapWords(const GfxRenderer& r, const int font, const char* text, const int width, char lines[][40],
              const int maxLines) {
  int count = 0;
  lines[0][0] = 0;
  const char* p = text;
  while (*p) {
    const char* end = std::strchr(p, ' ');
    if (!end) end = p + std::strlen(p);
    const int length = static_cast<int>(end - p);
    char trial[96];
    if (lines[count][0]) {
      std::snprintf(trial, sizeof(trial), "%s %.*s", lines[count], length, p);
    } else {
      std::snprintf(trial, sizeof(trial), "%.*s", length, p);
    }
    if (lines[count][0] && r.getTextWidth(font, trial) > width && count + 1 < maxLines) {
      ++count;
      std::snprintf(lines[count], 40, "%.*s", length > 39 ? 39 : length, p);
    } else {
      std::snprintf(lines[count], 40, "%.39s", trial);
    }
    p = *end ? end + 1 : end;
  }
  return lines[0][0] ? count + 1 : 0;
}

// A rule of dots, for the column breaks: lighter than a line, so the eye reads
// the rows first, the way a printed table is read.
void dottedColumn(const GfxRenderer& r, const int x, const int top, const int bottom) {
  for (int y = top; y < bottom; y += 6) r.drawPixel(x, y, true);
}

const char* osName(const deck::Os os) { return os == deck::Os::Mac ? "MAC" : "WINDOWS"; }

const char* linkLine(const radio::State state) {
  switch (state) {
    case radio::State::Linked:
      return "ENLAZADO";
    case radio::State::Waiting:
      return "ESPERANDO";
    case radio::State::Failed:
      return "SIN BLUETOOTH";
    case radio::State::Off:
      break;
  }
  return "APAGADO";
}

}  // namespace

std::unique_ptr<Activity> DeckActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DeckActivity>(renderer, mappedInput);
}

void DeckActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  deckfonts::ensure(renderer);
  renderer.setOrientation(GfxRenderer::Orientation::LandscapeCounterClockwise);
  view_ = loadState() ? View::Keys : View::Ask;
  radioUsed_ = radio::begin(kDeviceName, batteryPercent());
  linkShown_ = static_cast<uint8_t>(radio::state());
  cleanNext_ = true;
  requestUpdate();
}

void DeckActivity::onExit() {
  saveState();
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  Activity::onExit();
  radio::end();
  // The radio's memory only comes back whole with a restart, as in PRESENTER.
  if (radioUsed_ && radio::real()) silentRestart();
}

bool DeckActivity::loadState() {
  uint8_t bytes[deck::kStateBytes] = {};
  int got = 0;
  HalFile file;
  if (Storage.exists(kSavePath) && Storage.openFileForRead("DECK", kSavePath, file)) {
    got = file.read(bytes, sizeof(bytes));
  }
  bool asked = false;
  deck::unpackState(bytes, got, os_, template_, asked, style_);
  return asked;
}

void DeckActivity::saveState() {
  uint8_t bytes[deck::kStateBytes];
  deck::packState(os_, template_, view_ == View::Keys, style_, bytes);
  HalFile file;
  if (!Storage.openFileForWrite("DECK", kSavePath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

void DeckActivity::turn(const int by) {
  template_ = ((template_ + by) % deck::kTemplateCount + deck::kTemplateCount) % deck::kTemplateCount;
  lit_ = -1;
  note_ = nullptr;
  cleanNext_ = true;
  saveState();
  requestUpdate();
}

void DeckActivity::restyle(const int by) {
  style_ = deck::stepStyle(style_, by);
  lit_ = -1;
  cleanNext_ = true;
  saveState();
  requestUpdate();
}

const char* DeckActivity::statusLine() const { return note_ ? note_ : linkLine(static_cast<radio::State>(linkShown_)); }

void DeckActivity::press(const int button) {
  const deck::Chord chord = deck::kTemplates[template_].buttons[button].on(os_);
  const bool sent = radio::chord(chord.mods, chord.usage);
  note_ = sent ? nullptr : (radio::state() == radio::State::Failed ? "SIN BLUETOOTH" : "SIN ENLACE");
  lit_ = button;
  litUntilMs_ = static_cast<uint32_t>(millis()) + kLitMs;
  requestUpdate();
}

// Each style lays its own grid; deck::buttonIn says which button a cell holds.
int DeckActivity::buttonAt(const int x, const int y) const {
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  const int cols = deck::kColumns(style_);
  const int rows = deck::kRows(style_);
  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  switch (style_) {
    case deck::Style::Editorial:
      left = kSide + 1;
      top = kMargin;
      right = w - kMargin + 1;
      bottom = h - kMargin;
      break;
    case deck::Style::Teclado:
      // The gaps between caps count for the cap beside them: a finger that
      // lands on one meant a key.
      left = kKeyEdge - kKeyGap / 2;
      top = kStrip + 2;
      right = w - kKeyEdge + kKeyGap / 2;
      bottom = h - 8;
      break;
    case deck::Style::Indice:
    case deck::Style::Count:
      left = kIndexEdge;
      top = kBand;
      right = w - kIndexEdge;
      bottom = h - 6;
      break;
  }
  if (x < left || x >= right || y < top || y >= bottom) return -1;
  const int col = (x - left) * cols / (right - left);
  const int row = (y - top) * rows / (bottom - top);
  if (col < 0 || col >= cols || row < 0 || row >= rows) return -1;
  return deck::buttonIn(style_, col, row);
}

bool DeckActivity::onOsName(const int x, const int y) const {
  if (style_ == deck::Style::Editorial) return x <= kSide && y >= osLineY_ - 34 && y <= osLineY_ + 30;
  // Teclado and Indice name the computer at the right of their top line.
  const int band = style_ == deck::Style::Teclado ? kStrip : kBand;
  return y < band && x > renderer.getScreenWidth() / 2;
}

void DeckActivity::loop() {
  int px = 0;
  int py = 0;
  if (mappedInput.wasScreenTouchPressed(px, py)) pressX_ = px;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Keys && mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge) {
      pressX_ = -1;
      turn(-1);
      return;
    }
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  // The link coming and going is news for the column.
  const uint8_t link = static_cast<uint8_t>(radio::state());
  if (link != linkShown_) {
    linkShown_ = link;
    if (link == static_cast<uint8_t>(radio::State::Linked)) note_ = nullptr;
    requestUpdate();
  }

  if (lit_ >= 0 && static_cast<int32_t>(static_cast<uint32_t>(millis()) - litUntilMs_) >= 0) {
    lit_ = -1;
    requestUpdate();
  }

  if (view_ == View::Ask) {
    int x = 0;
    int y = 0;
    if (!mappedInput.wasScreenTapped(x, y)) return;
    if (x <= kSide) return;
    // The two answers split what is right of the column down the middle.
    os_ = x < kSide + (renderer.getScreenWidth() - kSide) / 2 ? deck::Os::Mac : deck::Os::Windows;
    view_ = View::Keys;
    cleanNext_ = true;
    saveState();
    requestUpdate();
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

  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  // The computer's name is where it is changed.
  if (onOsName(x, y)) {
    view_ = View::Ask;
    cleanNext_ = true;
    requestUpdate();
    return;
  }
  const int button = buttonAt(x, y);
  if (button >= 0) press(button);
}

void DeckActivity::drawAsk() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  int base = kMargin + 20 + kItalicCut.cap;
  put(r, kItalicFontId, kItalicCut, kMargin + 6, base, "Deck");
  base += 16;
  r.fillRect(kMargin + 6, base, kSide - kMargin - 20, 2, true);
  base += 28;
  for (const char* line : {"LOS ATAJOS", "CAMBIAN", "SEGÚN EL", "SISTEMA"}) {
    put(r, toybox::kTileFontId, kSmallCut, kMargin + 6, base, line);
    base += 20;
  }
  r.fillRect(kSide, kMargin, 3, h - 2 * kMargin, true);

  const int half = (w - kSide) / 2;
  dottedColumn(r, kSide + half, kMargin + 30, h - kMargin - 30);
  struct Answer {
    const char* name;
    const char* keys;
    const char* more;
  };
  const Answer answers[2] = {{"Mac", "CMD  OPT  CTRL", "MACOS"}, {"Windows", "CTRL  ALT  WIN", "WINDOWS 10 Y 11"}};
  for (int i = 0; i < 2; ++i) {
    const int x = kSide + i * half + 34;
    const int mid = h / 2;
    put(r, kNumeralFontId, kNumeralCut, x - 4, mid - 30, i == 0 ? "1" : "2");
    put(r, kItalicFontId, kItalicCut, x, mid + 30, answers[i].name);
    r.fillRect(x, mid + 48, half - 68, 1, true);
    put(r, toybox::kTileFontId, kSmallCut, x, mid + 76, answers[i].keys);
    put(r, toybox::kTileFontId, kSmallCut, x, mid + 98, answers[i].more);
  }
  put(r, toybox::kTileFontId, kSmallCut, kSide + 34, h - kMargin - 6, "TOCA EL TUYO");
}

void DeckActivity::drawEditorial() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  const deck::Template& tpl = deck::kTemplates[template_];

  // The column: which page, which program, which computer, the link.
  char number[4];
  std::snprintf(number, sizeof(number), "%d", template_ + 1);
  int base = kMargin + kNumeralCut.ascender - 10;
  put(r, kNumeralFontId, kNumeralCut, kMargin + 4, base, number);
  base += 18 + kItalicCut.cap;
  const char* title = deck::kTitles[template_];
  // The italic leans past its advance, so the large cut keeps clear of the rule.
  const int titleWidth = kSide - kMargin - 14;
  if (r.getTextWidth(kItalicFontId, title) <= titleWidth - 10) {
    put(r, kItalicFontId, kItalicCut, kMargin + 6, base, title);
    base += kItalicCut.cap;
  } else {
    // A long name in the small italic, broken at its last space if it must be.
    char first[32];
    std::snprintf(first, sizeof(first), "%s", title);
    const char* second = nullptr;
    if (r.getTextWidth(kItalicSmallFontId, title) > titleWidth) {
      char* space = std::strrchr(first, ' ');
      if (space) {
        *space = 0;
        second = title + (space - first) + 1;
      }
    }
    put(r, kItalicSmallFontId, kItalicSmallCut, kMargin + 6, base - kItalicCut.cap + kItalicSmallCut.cap, first);
    base += kItalicSmallCut.cap - kItalicCut.cap + kItalicSmallCut.cap;
    if (second) {
      base += 12;
      put(r, kItalicSmallFontId, kItalicSmallCut, kMargin + 6, base, second);
    }
    base += kItalicCut.cap;
  }
  base += 14;
  r.fillRect(kMargin + 6, base, kSide - kMargin - 20, 2, true);
  base += 28;
  osLineY_ = base;
  put(r, toybox::kTileFontId, kSmallCut, kMargin + 6, base, osName(os_));
  base += 20;
  put(r, toybox::kTileFontId, kSmallCut, kMargin + 6, base, statusLine());

  char of[16];
  std::snprintf(of, sizeof(of), "de %d", deck::kTemplateCount);
  put(r, kItalicSmallFontId, kItalicSmallCut, kMargin + 6, h - kMargin - 30, of);
  put(r, toybox::kTileFontId, kSmallCut, kMargin + 6, h - kMargin - 6, "DESLIZA");
  r.fillRect(kSide, kMargin, 3, h - 2 * kMargin, true);

  // The table.
  const int x0 = kSide + 3;
  const int cellW = (w - x0 - kMargin + 4) / 5;
  const int cellH = (h - 2 * kMargin) / 3;
  for (int row = 1; row < 3; ++row) r.fillRect(x0 + 14, kMargin + row * cellH, w - x0 - 14 - kMargin + 4, 1, true);
  for (int col = 1; col < 5; ++col) dottedColumn(r, x0 + col * cellW, kMargin + 8, h - kMargin - 8);

  for (int i = 0; i < deck::kButtons; ++i) {
    const int x = x0 + (i % 5) * cellW;
    const int y = kMargin + (i / 5) * cellH;
    const bool lit = i == lit_;
    if (lit) r.fillRect(x + 6, y + 6, cellW - 10, cellH - 11, true);
    // Drawn in paper on the pressed cell, so it reads as a key held down.
    const bool ink = !lit;
    char index[4];
    std::snprintf(index, sizeof(index), "%02d", i + 1);
    r.drawText(toybox::kTileFontId, x + 16, y + 12, index, ink);

    const deck::Button& b = tpl.buttons[i];
    char text[48];
    sentenceCase(b.label, text, sizeof(text));
    label(r, text, x + 16, y + 54, cellW - 18, ink);

    char said[32];
    deck::describe(b.on(os_), os_, said, sizeof(said));
    // Two lines at most: a third would run into a label that takes two.
    char lines[2][40];
    const int count = wrapWords(r, toybox::kTileFontId, said, cellW - 24, lines, 2);
    const int lead = kSmallCut.cap + 7;
    for (int n = 0; n < count; ++n) {
      r.drawText(toybox::kTileFontId, x + 16, y + cellH - 16 - kSmallCut.ascender - (count - 1 - n) * lead, lines[n],
                 ink);
    }
  }
}

// A black page of key caps. The cap is paper and its legend ink; a cap held
// down turns over, to an outline with its legend in paper.
void DeckActivity::drawTeclado() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  const deck::Template& tpl = deck::kTemplates[template_];
  r.fillRect(0, 0, w, h, true);

  // The strip: the program, then the computer, the link and the page.
  put(r, toybox::kUiFontId, kBandCut, kKeyEdge + 2, 14 + kBandCut.cap, tpl.name, false);
  char info[64];
  std::snprintf(info, sizeof(info), "%s  ·  %s  ·  %d/%d", osName(os_), statusLine(), template_ + 1,
                deck::kTemplateCount);
  put(r, toybox::kTileFontId, kSmallCut, w - kKeyEdge - 2 - r.getTextWidth(toybox::kTileFontId, info),
      14 + kBandCut.cap - 4, info, false);

  const int keyW = (w - 2 * kKeyEdge - 4 * kKeyGap) / 5;
  const int keyH = (h - kStrip - 10 - 2 * kKeyGap) / 3;
  for (int i = 0; i < deck::kButtons; ++i) {
    const deck::Place at = deck::placeOf(style_, i);
    const int x = kKeyEdge + at.col * (keyW + kKeyGap);
    const int y = kStrip + 4 + at.row * (keyH + kKeyGap);
    const bool lit = i == lit_;
    if (lit) {
      r.drawRoundedRect(x, y, keyW, keyH, 3, kKeyRadius, false);
    } else {
      r.fillRoundedRect(x, y, keyW, keyH, kKeyRadius, Color::White);
    }
    const bool ink = !lit;
    const deck::Button& b = tpl.buttons[i];

    // The legend, in the largest capitals every word of it fits.
    char words[2][40];
    const int inner = keyW - 16;
    int font = toybox::kButtonFontId;
    Cut cut = kKeyCut;
    int count = wrapWords(r, font, b.label, inner, words, 2);
    for (int n = 0; n < count; ++n) {
      if (r.getTextWidth(font, words[n]) > inner) {
        font = toybox::kTileFontId;
        cut = kSmallCut;
      }
    }
    if (font == toybox::kTileFontId) count = wrapWords(r, font, b.label, inner, words, 2);
    const int lead = cut.cap + 10;
    // One line sits where the first of two would, so the legends line up
    // along a row of keys.
    const int first = y + 22 + cut.cap;
    for (int n = 0; n < count; ++n) {
      put(r, font, cut, x + (keyW - r.getTextWidth(font, words[n])) / 2, first + n * lead, words[n], ink);
    }

    char said[32];
    deck::describe(b.on(os_), os_, said, sizeof(said));
    char lines[2][40];
    const int chordLines = wrapWords(r, toybox::kTileFontId, said, inner, lines, 2);
    const int chordLead = kSmallCut.cap + 6;
    for (int n = 0; n < chordLines; ++n) {
      put(r, toybox::kTileFontId, kSmallCut, x + (keyW - r.getTextWidth(toybox::kTileFontId, lines[n])) / 2,
          y + keyH - 13 - (chordLines - 1 - n) * chordLead, lines[n], ink);
    }
  }
}

// Three columns of five under a black band: a template's three rows, each set
// as a column, a button to a line with its chord under it.
void DeckActivity::drawIndice() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  const deck::Template& tpl = deck::kTemplates[template_];

  r.fillRect(0, 0, w, kBand - 8, true);
  put(r, toybox::kReadingBoldFontId, kBoldCut, kIndexEdge + 4, (kBand - 8 + kBoldCut.cap) / 2, deck::kTitles[template_],
      false);
  char info[64];
  std::snprintf(info, sizeof(info), "%s  ·  %s  ·  %d DE %d", osName(os_), statusLine(), template_ + 1,
                deck::kTemplateCount);
  put(r, toybox::kTileFontId, kSmallCut, w - kIndexEdge - 4 - r.getTextWidth(toybox::kTileFontId, info),
      (kBand - 8 + kSmallCut.cap) / 2, info, false);

  const int colW = (w - 2 * kIndexEdge) / 3;
  const int rowH = (h - kBand - 6) / 5;
  for (int col = 1; col < 3; ++col) r.fillRect(kIndexEdge + col * colW, kBand + 6, 2, 5 * rowH - 12, true);

  for (int i = 0; i < deck::kButtons; ++i) {
    const deck::Place at = deck::placeOf(style_, i);
    const int x = kIndexEdge + at.col * colW;
    const int y = kBand + at.row * rowH;
    const bool lit = i == lit_;
    if (lit) r.fillRect(x + 4, y + 2, colW - 6, rowH - 3, true);
    const bool ink = !lit;
    if (at.row > 0 && !lit && i - 1 != lit_) r.fillRect(x + 14, y, colW - 26, 1, true);

    const deck::Button& b = tpl.buttons[i];
    char text[48];
    sentenceCase(b.label, text, sizeof(text));
    const int inner = colW - 30;
    // On one line in the larger bold when it fits, in the smaller when it
    // does not, and on two lines only when neither holds it.
    char lines[2][40];
    int font = toybox::kReadingBoldFontId;
    Cut cut = kBoldCut;
    if (r.getTextWidth(font, text) > inner) {
      font = toybox::kReadingBoldSmallFontId;
      cut = kBoldSmallCut;
    }
    const int count = wrapWords(r, font, text, inner, lines, 2);

    char said[32];
    deck::describe(b.on(os_), os_, said, sizeof(said));
    // The label and its chord, centred in the row as one block.
    const int labelLead = cut.cap + 8;
    const int block = cut.cap + (count - 1) * labelLead + 12 + kSmallCut.cap;
    int base = y + (rowH - block) / 2 + cut.cap;
    for (int n = 0; n < count; ++n) {
      put(r, font, cut, x + 16, base, lines[n], ink);
      base += labelLead;
    }
    put(r, toybox::kTileFontId, kSmallCut, x + 16, base - labelLead + 12 + kSmallCut.cap, said, ink);
  }
}

void DeckActivity::render(RenderLock&&) {
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.clearScreen();
  if (view_ == View::Ask) {
    drawAsk();
  } else if (style_ == deck::Style::Teclado) {
    drawTeclado();
  } else if (style_ == deck::Style::Indice) {
    drawIndice();
  } else {
    drawEditorial();
  }
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
