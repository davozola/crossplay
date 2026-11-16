#include "RpnActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;

namespace {

constexpr char kSavePath[] = "/.crosspoint/rpn.sav";

// The page, top to bottom: the display, the five financial registers, the keys.
constexpr int kSide = 12;
constexpr int kPanelTop = 14;
constexpr int kPanelHeight = 150;
constexpr int kStripTop = kPanelTop + kPanelHeight + 10;
constexpr int kStripHeight = 42;
constexpr int kKeysTop = kStripTop + kStripHeight + 8;
constexpr int kShiftRow = 17;  // the f and g legends over each key

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

struct Grid {
  int cellW;
  int pitch;
};
Grid gridOf(const fui::DeviceContext& device) {
  Grid g;
  g.cellW = (device.width - kSide * 2) / rpn::kColumns;
  g.pitch = (device.height - lab::kBottomReserve - 4 - kKeysTop) / rpn::kRows;
  return g;
}

// A register's value in the eight characters a strip cell holds.
void shortValue(const double v, char* out, const size_t cap) {
  const double mag = std::fabs(v);
  if (mag == 0) {
    std::snprintf(out, cap, "0");
  } else if (mag < 100000 && mag >= 0.01) {
    std::snprintf(out, cap, "%.2f", v);
  } else if (mag < 10000000 && mag >= 0.01) {
    std::snprintf(out, cap, "%.0f", v);
  } else {
    std::snprintf(out, cap, "%.3G", v);
  }
}

}  // namespace

std::unique_ptr<Activity> RpnActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<RpnActivity>(renderer, mappedInput);
}

void RpnActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  requestUpdate();
}

void RpnActivity::onExit() {
  saveState();
  Activity::onExit();
}

void RpnActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }
  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const Grid g = gridOf(target.deviceContext());
  if (tapY < kKeysTop || tapX < kSide) return;
  const int col = (tapX - kSide) / g.cellW;
  const int row = (tapY - kKeysTop) / g.pitch;
  if (col >= rpn::kColumns || row >= rpn::kRows) return;
  calc_.press(static_cast<rpn::Key>(row * rpn::kColumns + col));
  requestUpdate();
}

void RpnActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  const int width = device.width;
  const int inner = width - kSide * 2;
  const Grid g = gridOf(device);

  // The display: a dark panel, the way the instrument's own window sits in its face.
  lab::box(screen, kSide, kPanelTop, inner, kPanelHeight, kInk, 14);
  {
    const char* mode = calc_.shift() == 1
                           ? "F"
                           : (calc_.shift() == 2 ? "G" : (calc_.storing() ? "STO" : (calc_.recalling() ? "RCL" : "")));
    char status[24];
    std::snprintf(status, sizeof(status), "%s%s%s", mode, mode[0] && calc_.begin() ? "   " : "",
                  calc_.begin() ? "BEGIN" : "");
    lab::caption(screen, kSide + 18, kPanelTop + 16, 160, status, fui::TextAlign::Left, fui::Color::White);
    char number[40];
    char line[48];
    rpn::format(calc_.level(1), calc_.decimals(), number, sizeof(number));
    std::snprintf(line, sizeof(line), "Y  %s", number);
    lab::caption(screen, kSide + 180, kPanelTop + 16, inner - 198, line, fui::TextAlign::Right, fui::Color::White);
    lab::box(screen, kSide + 18, kPanelTop + 42, inner - 36, 1, fui::Paint::dither(fui::Color::DarkGray));
    calc_.display(number, sizeof(number));
    const bool big = std::strlen(number) <= 12;
    const toybox::CutMetrics& cut = big ? toybox::kDisplayCut : toybox::kUiCut;
    lab::label(
        screen,
        lab::rect(kSide + 18, kPanelTop + 42 + (kPanelHeight - 42 - cut.inkHeight) / 2, inner - 36, cut.inkHeight),
        number, big ? toybox::kDisplayFont : toybox::kUiFont, cut, fui::TextAlign::Right, fui::Color::White);
  }

  // What the five financial registers hold, each over its own key.
  for (int f = 0; f < 5; ++f) {
    const int x = kSide + f * g.cellW;
    char text[24];
    shortValue(calc_.fin(f), text, sizeof(text));
    lab::caption(screen, x, kStripTop + 6, g.cellW, rpn::keyLabel(static_cast<rpn::Key>(f)), fui::TextAlign::Center,
                 fui::Color::DarkGray);
    lab::caption(screen, x, kStripTop + 6 + toybox::kTileCut.inkHeight + 7, g.cellW, text, fui::TextAlign::Center);
    if (f > 0) lab::box(screen, x, kStripTop + 4, 1, kStripHeight - 8, fui::Paint::dither(fui::Color::DarkGray));
  }
  lab::ink(screen, kSide, kStripTop + kStripHeight, inner, toybox::kHairline);

  for (int k = 0; k < rpn::KeyCount; ++k) {
    const rpn::Key key = static_cast<rpn::Key>(k);
    if (key == rpn::KeyEnter2) continue;
    const int col = k % rpn::kColumns;
    const int row = k / rpn::kColumns;
    const int span = key == rpn::KeyEnter ? 2 : 1;
    const int x = kSide + col * g.cellW + 3;
    const int y = kKeysTop + row * g.pitch;
    const int w = g.cellW * span - 6;
    const int boxY = y + kShiftRow;
    const int boxH = g.pitch - kShiftRow - 5;
    const char* f = rpn::keyLabelF(key);
    const char* gl = rpn::keyLabelG(key);
    // f legends in a solid tab, g legends plain: the two shift keys are drawn the same way.
    if (f[0]) {
      const int tabW = static_cast<int>(std::strlen(f)) * 13 + 12;
      lab::box(screen, x, y, tabW, kShiftRow - 1, kInk, 4);
      lab::caption(screen, x, y + 2, tabW, f, fui::TextAlign::Center, fui::Color::White);
    }
    if (gl[0]) lab::caption(screen, x, y + 2, w - 2, gl, fui::TextAlign::Right);

    const bool solid = key == rpn::KeyF || key == rpn::KeyEnter;
    const bool held = (key == rpn::KeyF && calc_.shift() == 1) || (key == rpn::KeyG && calc_.shift() == 2) ||
                      (key == rpn::KeySto && calc_.storing()) || (key == rpn::KeyRcl && calc_.recalling());
    lab::box(screen, x, boxY, w, boxH, kInk, 9);
    if (!solid) lab::box(screen, x + 2, boxY + 2, w - 4, boxH - 4, kPaper, 7);
    if (held) lab::box(screen, x + 5, boxY + boxH - 9, w - 10, 4, solid ? kPaper : kInk, 2);
    const char* text = rpn::keyLabel(key);
    const bool large = text[1] == 0;  // digits and the four operations
    const toybox::CutMetrics& cut = large ? toybox::kUiCut : toybox::kTileCut;
    lab::label(screen, lab::rect(x, boxY + (boxH - cut.inkHeight) / 2, w, cut.inkHeight), text,
               large ? toybox::kUiFont : toybox::kTileFont, cut, fui::TextAlign::Center,
               solid ? fui::Color::White : fui::Color::Black);
  }

  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void RpnActivity::saveState() {
  uint8_t blob[rpn::Calculator::kSaveBytes];
  const size_t size = calc_.save(blob, sizeof(blob));
  if (size == 0) return;
  HalFile file;
  if (!Storage.openFileForWrite("RPN", kSavePath, file)) return;
  file.write(blob, size);
  file.flush();
}

void RpnActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("RPN", kSavePath, file)) return;
  uint8_t blob[rpn::Calculator::kSaveBytes];
  const int size = file.read(blob, sizeof(blob));
  if (size > 0) calc_.restore(blob, static_cast<size_t>(size));
}
