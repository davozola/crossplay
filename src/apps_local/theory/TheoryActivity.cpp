#include "TheoryActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
#include "TheoryCore.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;

namespace {

constexpr char kSavePath[] = "/.crosspoint/theory.sav";
constexpr uint8_t kSaveVersion = 1;

enum : fui::ActionId {
  ActionTab = 1,
  ActionRoot,
  ActionScale,
  ActionChord,
  ActionProgression,
  ActionSteps,
  ActionPulses,
  ActionRotation,
  ActionKey,  // value: pitch class tapped on the keyboard
};

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

int wrap(const int v, const int n) { return ((v % n) + n) % n; }
int clamp(const int v, const int lo, const int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Two octaves of keyboard from C. Notes in `mask` carry a dot; the root's is
// ringed. Returns the y under it.
int keyboard(toybox::Screen& screen, const int y, const uint16_t mask, const int root) {
  static const int kWhite[7] = {0, 2, 4, 5, 7, 9, 11};
  static const int kBlackAfter[5] = {0, 1, 3, 4, 5};  // the white key each black one follows
  static const int kBlack[5] = {1, 3, 6, 8, 10};
  const int inner = screen.device().width - toybox::kMargin * 2;
  const int keyW = inner / 14;
  const int x0 = toybox::kMargin + (inner - keyW * 14) / 2;
  const int h = 150;
  const int blackW = keyW * 3 / 5;
  const int blackH = 92;
  lab::box(screen, x0 - 2, y - 2, keyW * 14 + 4, h + 4, kInk, 6);
  for (int k = 0; k < 14; ++k) {
    const int note = kWhite[k % 7];
    const int x = x0 + k * keyW;
    lab::box(screen, x + 1, y, keyW - 2, h, kPaper, 3);
    if ((mask >> note) & 1) {
      const int16_t cx = static_cast<int16_t>(x + keyW / 2);
      const int16_t cy = static_cast<int16_t>(y + h - 24);
      toybox::disc(screen, cx, cy, 11, fui::Color::Black);
      if (note == root) toybox::disc(screen, cx, cy, 5, fui::Color::White);
    }
  }
  for (int o = 0; o < 2; ++o) {
    for (int b = 0; b < 5; ++b) {
      const int note = kBlack[b];
      const int x = x0 + (o * 7 + kBlackAfter[b] + 1) * keyW - blackW / 2;
      lab::box(screen, x, y, blackW, blackH, kInk, 3);
      if ((mask >> note) & 1) {
        const int16_t cx = static_cast<int16_t>(x + blackW / 2);
        const int16_t cy = static_cast<int16_t>(y + blackH - 18);
        toybox::disc(screen, cx, cy, 9, fui::Color::White);
        if (note == root) toybox::disc(screen, cx, cy, 4, fui::Color::Black);
      }
    }
  }
  return y + h + 2;
}

// The twelve notes as a row of small keys to pick the root from.
int rootRow(toybox::Screen& screen, const int y, const int root) {
  const int inner = screen.device().width - toybox::kMargin * 2;
  const int gap = 4;
  const int w = (inner - gap * 11) / 12;
  for (int n = 0; n < 12; ++n) {
    lab::button(screen, lab::rect(toybox::kMargin + n * (w + gap), y, w, 44), theory::noteName(n), ActionRoot, n,
                n == root);
  }
  return y + 44;
}

// A titled line: a caption over the text in the body cut. Returns the y under it.
int fact(toybox::Screen& screen, const int y, const char* title, const char* text) {
  const int inner = screen.device().width - toybox::kMargin * 2;
  lab::caption(screen, toybox::kMargin, y, inner, title, fui::TextAlign::Left, fui::Color::DarkGray);
  lab::value(screen, toybox::kMargin, y + toybox::kTileCut.inkHeight + 8, inner, text);
  return y + toybox::kTileCut.inkHeight + 8 + toybox::kUiCut.inkHeight + 16;
}

// The chords of a progression, each in a box with its numeral above and its
// notes below, four to a row. Returns the y under them.
int chordBoxes(toybox::Screen& screen, const int y, const theory::Progression& p, const int root) {
  const int inner = screen.device().width - toybox::kMargin * 2;
  const int perRow = 4;
  const int gap = 6;
  const int w = (inner - gap * (perRow - 1)) / perRow;
  const int h = 100;
  int i = 0;
  const char* n = p.numerals;
  for (const char* c = p.chords; *c; ++c) {
    if (*c == ' ') continue;
    const int step = c[0] >= 'A' ? c[0] - 'A' + 10 : c[0] - '0';
    const char quality = c[1];
    ++c;
    // The chord table's rows for major, minor, diminished and dominant seventh.
    const int kind = quality == 'm' ? 1 : (quality == 'd' ? 2 : (quality == '7' ? 8 : 0));
    char name[8];
    std::snprintf(name, sizeof(name), "%s%s", theory::noteName(root + step), theory::chord(kind).suffix);
    char notes[24];
    theory::chordNotes(kind, root + step, notes, sizeof(notes));
    char numeral[6] = {};
    size_t at = 0;
    while (*n && *n != ' ' && at + 1 < sizeof(numeral)) numeral[at++] = *n++;
    while (*n == ' ') ++n;
    const int x = toybox::kMargin + (i % perRow) * (w + gap);
    const int by = y + (i / perRow) * (h + gap);
    lab::box(screen, x, by, w, h, kInk, lab::kRadius);
    lab::box(screen, x + 2, by + 2, w - 4, h - 4, kPaper, lab::kRadius - 2);
    lab::caption(screen, x, by + 12, w, numeral, fui::TextAlign::Center, fui::Color::DarkGray);
    lab::value(screen, x, by + 12 + toybox::kTileCut.inkHeight + 10, w, name, fui::TextAlign::Center);
    lab::caption(screen, x, by + h - 12 - toybox::kTileCut.inkHeight, w, notes, fui::TextAlign::Center);
    ++i;
  }
  return y + ((i + perRow - 1) / perRow) * (h + gap);
}

// A heading in the display cut, or the body cut when it would not fit.
int heading(toybox::Screen& screen, const int y, const int w, const char* text) {
  const bool big = std::strlen(text) <= 11;
  const int top = big ? y : y + (toybox::kDisplayCut.inkHeight - toybox::kUiCut.inkHeight) / 2;
  lab::label(screen, lab::rect(toybox::kMargin, top, w, big ? toybox::kDisplayCut.inkHeight : toybox::kUiCut.inkHeight),
             text, big ? toybox::kDisplayFont : toybox::kUiFont, big ? toybox::kDisplayCut : toybox::kUiCut,
             fui::TextAlign::Left);
  return y + toybox::kDisplayCut.inkHeight;
}

// The seven chords of a scale, one cell each under its degree.
int degreeCells(toybox::Screen& screen, const int y, const char* triads) {
  static const char* const kDegrees[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
  const int inner = screen.device().width - toybox::kMargin * 2;
  const int gap = 4;
  const int w = (inner - gap * 6) / 7;
  const int h = 58;
  const char* c = triads;
  for (int d = 0; d < 7 && *c; ++d) {
    char name[8] = {};
    size_t at = 0;
    while (*c && *c != ' ' && at + 1 < sizeof(name)) name[at++] = *c++;
    while (*c == ' ') ++c;
    const int x = toybox::kMargin + d * (w + gap);
    lab::box(screen, x, y, w, h, kInk, 8);
    lab::box(screen, x + 2, y + 2, w - 4, h - 4, kPaper, 6);
    lab::caption(screen, x, y + 10, w, kDegrees[d], fui::TextAlign::Center, fui::Color::DarkGray);
    lab::caption(screen, x, y + h - 10 - toybox::kTileCut.inkHeight, w, name, fui::TextAlign::Center);
  }
  return y + h;
}

void wheel(toybox::Screen& screen, const int cx, const int cy, const int radius, const int steps,
           const uint16_t pattern) {
  constexpr float kTwoPi = 6.2831853f;
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(radius),
               fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(radius - 3),
               fui::Color::White);
  int px[theory::kMaxSteps];
  int py[theory::kMaxSteps];
  for (int i = 0; i < steps; ++i) {
    const float a = kTwoPi * static_cast<float>(i) / static_cast<float>(steps);
    px[i] = cx + static_cast<int>(std::sin(a) * static_cast<float>(radius - 1));
    py[i] = cy - static_cast<int>(std::cos(a) * static_cast<float>(radius - 1));
  }
  // The polygon the pulses make.
  int first = -1;
  int last = -1;
  auto join = [&](const int a, const int b) {
    const int dx = px[b] - px[a];
    const int dy = py[b] - py[a];
    const int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    for (int s = 0; s <= n; s += 2) {
      lab::ink(screen, px[a] + dx * s / (n ? n : 1) - 1, py[a] + dy * s / (n ? n : 1) - 1, 3, 3);
    }
  };
  for (int i = 0; i < steps; ++i) {
    if (!((pattern >> i) & 1)) continue;
    if (first < 0) first = i;
    if (last >= 0) join(last, i);
    last = i;
  }
  if (first >= 0 && last != first) join(last, first);
  const int16_t dot = static_cast<int16_t>(steps > 12 ? 13 : 16);
  for (int i = 0; i < steps; ++i) {
    const bool on = (pattern >> i) & 1;
    toybox::disc(screen, static_cast<int16_t>(px[i]), static_cast<int16_t>(py[i]), dot, fui::Color::Black);
    if (!on)
      toybox::disc(screen, static_cast<int16_t>(px[i]), static_cast<int16_t>(py[i]), static_cast<int16_t>(dot - 3),
                   fui::Color::White);
  }
  // Step one is at the top; mark it.
  lab::ink(screen, cx - 1, cy - radius - 30, 3, 12);
}

}  // namespace

std::unique_ptr<Activity> TheoryActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<TheoryActivity>(renderer, mappedInput);
}

void TheoryActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  requestUpdate();
}

void TheoryActivity::onExit() {
  saveState();
  Activity::onExit();
}

void TheoryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }
  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  const int step = event.value ? 1 : -1;
  switch (event.action) {
    case ActionTab:
      tab_ = clamp(event.value, 0, 3);
      break;
    case ActionRoot:
      root_ = clamp(event.value, 0, 11);
      break;
    case ActionScale:
      scale_ = wrap(scale_ + step, theory::kScales);
      break;
    case ActionChord:
      chord_ = wrap(chord_ + step, theory::kChords);
      break;
    case ActionProgression:
      progression_ = wrap(progression_ + step, theory::kProgressions);
      break;
    case ActionSteps:
      steps_ = clamp(steps_ + step, 2, theory::kMaxSteps);
      pulses_ = clamp(pulses_, 1, steps_);
      rotation_ = wrap(rotation_, steps_);
      break;
    case ActionPulses:
      pulses_ = clamp(pulses_ + step, 1, steps_);
      break;
    case ActionRotation:
      rotation_ = wrap(rotation_ + step, steps_);
      break;
    default:
      return;
  }
  requestUpdate();
}

void TheoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);

  lab::chrome(screen, "MUSIC THEORY LAB");
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int bottom = device.height - lab::kBottomReserve - 6;
  static const char* const kTabs[4] = {"SCALES", "CHORDS", "CHANGES", "RHYTHM"};
  int y = lab::tabs(screen, toybox::kChromeHeight + 10, kTabs, 4, tab_, ActionTab) + 12;
  char text[160];
  char count[toybox::kIntChars * 2 + 5];

  if (tab_ == 0) {
    y = rootRow(screen, y, root_) + 8;
    std::snprintf(count, sizeof(count), "%d OF %d", scale_ + 1, theory::kScales);
    y = lab::stepper(screen, y, "SCALE", count, ActionScale, 0) + 6;
    std::snprintf(text, sizeof(text), "%s %s", theory::noteName(root_), theory::scale(scale_).name);
    y = heading(screen, y, inner, text) + 16;
    y = keyboard(screen, y, theory::scaleMask(scale_, root_), root_) + 18;
    const theory::Scale& s = theory::scale(scale_);
    size_t at = 0;
    for (int i = 0; i < s.count; ++i) {
      at += static_cast<size_t>(
          std::snprintf(text + at, sizeof(text) - at, "%s%s", i ? "  " : "", theory::noteName(root_ + s.steps[i])));
    }
    y = fact(screen, y, "NOTES", text);
    theory::scaleSteps(scale_, text, sizeof(text));
    y = fact(screen, y, "STEPS  (W WHOLE, H HALF)", text);
    theory::scaleTriads(scale_, root_, text, sizeof(text));
    if (text[0]) {
      lab::caption(screen, toybox::kMargin, y, inner, "THE CHORD ON EACH NOTE  (M MINOR)", fui::TextAlign::Left,
                   fui::Color::DarkGray);
      degreeCells(screen, y + toybox::kTileCut.inkHeight + 10, text);
    }
  } else if (tab_ == 1) {
    y = rootRow(screen, y, root_) + 8;
    std::snprintf(count, sizeof(count), "%d OF %d", chord_ + 1, theory::kChords);
    y = lab::stepper(screen, y, "CHORD", count, ActionChord, 0) + 6;
    const theory::Chord& c = theory::chord(chord_);
    std::snprintf(text, sizeof(text), "%s%s", theory::noteName(root_), c.suffix);
    lab::label(screen, lab::rect(toybox::kMargin, y, inner / 2, toybox::kDisplayCut.inkHeight), text,
               toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    lab::caption(screen, toybox::kMargin + inner / 2,
                 y + (toybox::kDisplayCut.inkHeight - toybox::kTileCut.inkHeight) / 2, inner / 2, c.name,
                 fui::TextAlign::Right);
    y += toybox::kDisplayCut.inkHeight + 16;
    y = keyboard(screen, y, theory::chordMask(chord_, root_), root_) + 18;
    theory::chordNotes(chord_, root_, text, sizeof(text));
    y = fact(screen, y, "NOTES", text);
    y = fact(screen, y, "FORMULA  (B FLAT, # SHARP)", c.formula);
    // Which of the seven-note scales hold every note of it.
    const uint16_t mask = theory::chordMask(chord_, root_);
    size_t at = 0;
    text[0] = 0;
    int listed = 0;
    for (int s = 0; s < theory::kScales && listed < 3; ++s) {
      if (theory::scale(s).count != 7 || (theory::scaleMask(s, root_) & mask) != mask) continue;
      at += static_cast<size_t>(
          std::snprintf(text + at, sizeof(text) - at, "%s%s", listed ? ", " : "", theory::scale(s).name));
      ++listed;
    }
    if (listed) fact(screen, y, "FITS IN", text);
  } else if (tab_ == 2) {
    y = rootRow(screen, y, root_) + 8;
    std::snprintf(count, sizeof(count), "%d OF %d", progression_ + 1, theory::kProgressions);
    y = lab::stepper(screen, y, "CHANGES", count, ActionProgression, 0) + 6;
    const theory::Progression& p = theory::progression(progression_);
    lab::label(screen, lab::rect(toybox::kMargin, y, inner, toybox::kDisplayCut.inkHeight), p.name,
               toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    y += toybox::kDisplayCut.inkHeight + 12;
    std::snprintf(text, sizeof(text), "IN %s %s", theory::noteName(root_), p.minorKey ? "MINOR" : "MAJOR");
    lab::caption(screen, toybox::kMargin, y, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
    y += toybox::kTileCut.inkHeight + 14;
    y = chordBoxes(screen, y, p, root_) + 14;
    if (y + 60 < bottom) {
      lab::paragraph(screen, toybox::kMargin, y, inner,
                     "ROMAN NUMERALS COUNT UP THE SCALE FROM THE KEY NOTE. M IS MINOR, 7 A DOMINANT SEVENTH. "
                     "CHANGE THE KEY AND THE NUMERALS STAY.",
                     3);
    }
  } else {
    char number[toybox::kIntChars + 1];
    std::snprintf(number, sizeof(number), "%d", steps_);
    y = lab::stepper(screen, y, "STEPS", number, ActionSteps, 0) + 6;
    std::snprintf(number, sizeof(number), "%d", pulses_);
    y = lab::stepper(screen, y, "PULSES", number, ActionPulses, 0) + 6;
    std::snprintf(number, sizeof(number), "%d", rotation_);
    y = lab::stepper(screen, y, "TURN", number, ActionRotation, 0) + 10;
    const uint16_t pattern = theory::euclid(steps_, pulses_, rotation_);
    const int stripTop = bottom - 118;
    const int radius = ((stripTop - y - 70) / 2 < inner / 2 - 30) ? (stripTop - y - 70) / 2 : inner / 2 - 30;
    const int cy = y + 40 + radius;
    wheel(screen, width / 2, cy, radius, steps_, pattern);
    char big[toybox::kIntChars * 2 + 5];
    std::snprintf(big, sizeof(big), "E(%d,%d)", pulses_, steps_);
    lab::box(screen, width / 2 - 86, cy - toybox::kDisplayCut.inkHeight / 2 - 22, 172,
             toybox::kDisplayCut.inkHeight + toybox::kTileCut.inkHeight + 38, kPaper, 12);
    lab::label(screen, lab::rect(0, cy - toybox::kDisplayCut.inkHeight / 2 - 10, width, toybox::kDisplayCut.inkHeight),
               big, toybox::kDisplayFont, toybox::kDisplayCut);
    lab::caption(screen, 0, cy + toybox::kDisplayCut.inkHeight / 2 + 2, width, theory::euclidName(steps_, pulses_),
                 fui::TextAlign::Center, fui::Color::DarkGray);
    // The same rhythm as a row of steps, the way a sequencer shows it.
    const int gap = 4;
    const int cell = (inner - gap * (steps_ - 1)) / steps_;
    const int stripW = cell * steps_ + gap * (steps_ - 1);
    const int sx = toybox::kMargin + (inner - stripW) / 2;
    for (int i = 0; i < steps_; ++i) {
      const int cx = sx + i * (cell + gap);
      lab::box(screen, cx, stripTop, cell, 44, kInk, 6);
      if (!((pattern >> i) & 1)) lab::box(screen, cx + 2, stripTop + 2, cell - 4, 40, kPaper, 4);
      if (i % 4 == 0) lab::ink(screen, cx, stripTop + 50, cell, 3);
    }
    lab::paragraph(screen, toybox::kMargin, stripTop + 64, inner,
                   "PULSES SPREAD AS EVENLY AS THE STEPS ALLOW. MANY TRADITIONAL RHYTHMS ARE EXACTLY THIS.", 2);
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Theory");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void TheoryActivity::saveState() {
  const uint8_t data[9] = {kSaveVersion,
                           static_cast<uint8_t>(tab_),
                           static_cast<uint8_t>(root_),
                           static_cast<uint8_t>(scale_),
                           static_cast<uint8_t>(chord_),
                           static_cast<uint8_t>(progression_),
                           static_cast<uint8_t>(steps_),
                           static_cast<uint8_t>(pulses_),
                           static_cast<uint8_t>(rotation_)};
  HalFile file;
  if (!Storage.openFileForWrite("THRY", kSavePath, file)) return;
  file.write(data, sizeof(data));
  file.flush();
}

void TheoryActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("THRY", kSavePath, file)) return;
  uint8_t data[9] = {};
  if (file.read(data, sizeof(data)) != sizeof(data) || data[0] != kSaveVersion) return;
  tab_ = clamp(data[1], 0, 3);
  root_ = clamp(data[2], 0, 11);
  scale_ = clamp(data[3], 0, theory::kScales - 1);
  chord_ = clamp(data[4], 0, theory::kChords - 1);
  progression_ = clamp(data[5], 0, theory::kProgressions - 1);
  steps_ = clamp(data[6], 2, theory::kMaxSteps);
  pulses_ = clamp(data[7], 1, steps_);
  rotation_ = clamp(data[8], 0, steps_ - 1);
}
