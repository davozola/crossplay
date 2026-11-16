#include "FmLabActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <cmath>
#include <cstdio>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;

namespace {

constexpr char kSavePath[] = "/.crosspoint/fmlab.sav";
constexpr uint8_t kSaveVersion = 1;

enum : fui::ActionId {
  ActionTab = 1,
  ActionAlgorithm,  // value 0 back, 1 on, 2 back five, 3 on five
  ActionCarrier,
  ActionModulator,
  ActionIndex,
  ActionField,   // value: 0..7
  ActionNudge,   // value: 0 -10, 1 -1, 2 +1, 3 +10
  ActionPreset,  // value: preset
};

// The frequency ratios the steppers walk through.
constexpr float kRatios[] = {0.5f, 1, 1.5f, 2, 3, 3.5f, 4, 5, 6, 7, 8, 9, 10, 12, 14};
constexpr int kRatioCount = sizeof(kRatios) / sizeof(kRatios[0]);
constexpr int kMaxIndex = 20;  // in halves

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

void ratioText(const float r, char* out, const size_t cap) {
  if (std::fabs(r - std::lround(r)) < 0.01f) {
    std::snprintf(out, cap, "%ld", std::lround(r));
  } else {
    std::snprintf(out, cap, "%.1f", static_cast<double>(r));
  }
}

int wrap(const int v, const int n) { return ((v % n) + n) % n; }
int clamp(const int v, const int lo, const int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// A line between two points with a corner in it: down, across, down.
void elbow(toybox::Screen& screen, const int x0, const int y0, const int x1, const int y1) {
  const int mid = (y0 + y1) / 2;
  lab::ink(screen, x0 - 1, y0, 3, mid - y0);
  lab::ink(screen, (x0 < x1 ? x0 : x1) - 1, mid - 1, (x0 < x1 ? x1 - x0 : x0 - x1) + 3, 3);
  lab::ink(screen, x1 - 1, mid, 3, y1 - mid);
}

void drawAlgorithm(toybox::Screen& screen, const int number, const int top, const int bottom) {
  const fm::Algorithm a = fm::algorithm(number);
  const fm::Diagram d = fm::layout(a);
  const int width = screen.device().width;
  const int size = 58;
  const int colStep = 74;
  const int rowStep = (bottom - top - size - 22) / 3 < 96 ? (bottom - top - size - 22) / 3 : 96;
  const int span = static_cast<int>((d.columns - 1) * static_cast<float>(colStep));
  const int left = (width - span) / 2;
  const int baseY = bottom - 22 - size;  // top of the carrier row
  auto cx = [&](const int op) { return left + static_cast<int>(d.x[op - 1] * static_cast<float>(colStep)); };
  auto cy = [&](const int op) { return baseY - d.row[op - 1] * rowStep; };

  // The output bus the carriers stand on.
  int firstCarrier = 0, lastCarrier = 0;
  for (int op = 1; op <= fm::kOps; ++op) {
    if (!a.isCarrier(op)) continue;
    if (!firstCarrier) firstCarrier = op;
    lastCarrier = op;
    lab::ink(screen, cx(op) - 1, cy(op) + size, 3, 14);
  }
  int busL = cx(firstCarrier), busR = cx(lastCarrier);
  for (int op = 1; op <= fm::kOps; ++op) {
    if (!a.isCarrier(op)) continue;
    if (cx(op) < busL) busL = cx(op);
    if (cx(op) > busR) busR = cx(op);
  }
  lab::ink(screen, busL - 14, baseY + size + 12, busR - busL + 28, 4);

  for (int t = 1; t <= fm::kOps; ++t) {
    for (int m = 1; m <= fm::kOps; ++m) {
      if ((a.modulators[t - 1] >> (m - 1)) & 1) elbow(screen, cx(m), cy(m) + size, cx(t), cy(t));
    }
  }
  // Feedback: a loop out of the right side and back into the top.
  {
    const int from = a.feedbackFrom;
    const int to = a.feedbackTo;
    const int x = (cx(from) > cx(to) ? cx(from) : cx(to)) + size / 2 + 12;
    const int yFrom = cy(from) + size / 2;
    const int yTo = cy(to) - 12;
    lab::ink(screen, cx(from) + size / 2, yFrom - 1, x - cx(from) - size / 2, 3);
    lab::ink(screen, x - 1, yTo, 3, yFrom - yTo + 1);
    lab::ink(screen, cx(to), yTo - 1, x - cx(to) + 1, 3);
    lab::ink(screen, cx(to) - 1, yTo, 3, 12);
  }
  for (int op = 1; op <= fm::kOps; ++op) {
    const int x = cx(op) - size / 2;
    const int y = cy(op);
    lab::box(screen, x, y, size, size, kInk, 10);
    if (!a.isCarrier(op)) lab::box(screen, x + 3, y + 3, size - 6, size - 6, kPaper, 7);
    char n[2] = {static_cast<char>('0' + op), 0};
    lab::label(screen,
               lab::rect(x, y + (size - toybox::kDisplayCut.inkHeight) / 2, size, toybox::kDisplayCut.inkHeight), n,
               toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Center,
               a.isCarrier(op) ? fui::Color::White : fui::Color::Black);
  }
}

}  // namespace

std::unique_ptr<Activity> FmLabActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<FmLabActivity>(renderer, mappedInput);
}

void FmLabActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  envelope_ = fm::envelopePreset(1);
  loadState();
  requestUpdate();
}

void FmLabActivity::onExit() {
  saveState();
  Activity::onExit();
}

void FmLabActivity::loop() {
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
  const int v = event.value;
  switch (event.action) {
    case ActionTab:
      tab_ = clamp(v, 0, 2);
      break;
    case ActionAlgorithm: {
      static const int kSteps[4] = {-1, 1, -5, 5};
      algorithm_ = wrap(algorithm_ - 1 + kSteps[clamp(v, 0, 3)], fm::kAlgorithms) + 1;
      break;
    }
    case ActionCarrier:
      carrier_ = wrap(carrier_ + (v ? 1 : -1), kRatioCount);
      break;
    case ActionModulator:
      modulator_ = wrap(modulator_ + (v ? 1 : -1), kRatioCount);
      break;
    case ActionIndex:
      index_ = clamp(index_ + (v ? 1 : -1), 0, kMaxIndex);
      break;
    case ActionField:
      envelopeField_ = clamp(v, 0, 7);
      break;
    case ActionNudge: {
      static const int kSteps[4] = {-10, -1, 1, 10};
      uint8_t& field = envelopeField_ < 4 ? envelope_.rate[envelopeField_] : envelope_.level[envelopeField_ - 4];
      field = static_cast<uint8_t>(clamp(field + kSteps[clamp(v, 0, 3)], 0, 99));
      break;
    }
    case ActionPreset:
      envelope_ = fm::envelopePreset(v);
      break;
    default:
      return;
  }
  requestUpdate();
}

void FmLabActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);

  lab::chrome(screen, "FM SYNTH LAB");
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int x = toybox::kMargin;
  const int bottom = device.height - lab::kBottomReserve - 6;
  static const char* const kTabs[3] = {"ALGORITHM", "RATIO", "ENVELOPE"};
  int y = lab::tabs(screen, toybox::kChromeHeight + 10, kTabs, 3, tab_, ActionTab) + 12;

  if (tab_ == 0) {
    const fm::Algorithm a = fm::algorithm(algorithm_);
    char text[toybox::kIntChars * 2 + 6];
    std::snprintf(text, sizeof(text), "%d OF %d", algorithm_, fm::kAlgorithms);
    y = lab::stepper(screen, y, "ALGORITHM", text, ActionAlgorithm, 0) + 8;
    const int gap = 8;
    const int half = (inner - gap) / 2;
    lab::button(screen, lab::rect(x, y, half, 40), "BACK 5", ActionAlgorithm, 2);
    lab::button(screen, lab::rect(x + half + gap, y, half, 40), "ON 5", ActionAlgorithm, 3);
    y += 40 + 10;
    const int notesTop = bottom - 118;
    drawAlgorithm(screen, algorithm_, y, notesTop - 8);
    lab::ink(screen, x, notesTop, inner, toybox::kHairline);
    char line[64] = "CARRIERS";
    int at = 8;
    for (int op = 1; op <= fm::kOps; ++op) {
      if (a.isCarrier(op)) at += std::snprintf(line + at, sizeof(line) - static_cast<size_t>(at), " %d", op);
    }
    lab::caption(screen, x, notesTop + 12, inner / 2, line);
    if (a.feedbackFrom == a.feedbackTo) {
      std::snprintf(line, sizeof(line), "FEEDBACK ON %d", a.feedbackFrom);
    } else {
      std::snprintf(line, sizeof(line), "FEEDBACK %d INTO %d", a.feedbackFrom, a.feedbackTo);
    }
    lab::caption(screen, x + inner / 2, notesTop + 12, inner / 2, line, fui::TextAlign::Right);
    lab::paragraph(screen, x, notesTop + 12 + toybox::kTileCut.inkHeight + 12, inner, fm::character(a), 3);
  } else if (tab_ == 1) {
    const double c = static_cast<double>(kRatios[carrier_]);
    const double m = static_cast<double>(kRatios[modulator_]);
    const double index = index_ / 2.0;
    char text[24];
    ratioText(kRatios[carrier_], text, sizeof(text));
    y = lab::stepper(screen, y, "CARRIER", text, ActionCarrier, 0) + 6;
    ratioText(kRatios[modulator_], text, sizeof(text));
    y = lab::stepper(screen, y, "MODULATOR", text, ActionModulator, 0) + 6;
    std::snprintf(text, sizeof(text), "%.1f", index);
    y = lab::stepper(screen, y, "INDEX", text, ActionIndex, 0) + 12;

    // The spectrum: a bar for each partial, at multiples of the base frequency.
    lab::caption(screen, x, y, inner, "SPECTRUM, IN MULTIPLES OF THE BASE NOTE");
    y += toybox::kTileCut.inkHeight + 8;
    double partials[fm::kPartials];
    fm::spectrum(c, m, index, partials);
    const int plotH = 150;
    const int slot = inner / 32;
    const int plotX = x + (inner - slot * 32) / 2;
    for (int k = 1; k < fm::kPartials; ++k) {
      const int h = static_cast<int>(partials[k] * plotH);
      if (h <= 0) continue;
      lab::box(screen, plotX + (k - 1) * slot + 2, y + plotH - h, slot - 4, h, kInk, 2);
    }
    lab::ink(screen, x, y + plotH, inner, 2);
    for (int n = 1; n <= 16; ++n) {
      const int tx = plotX + (n * 2 - 1) * slot + slot / 2;
      lab::ink(screen, tx, y + plotH, 1, 5);
      if (n == 1 || n % 2 == 0) {
        char tick[toybox::kIntChars + 1];
        std::snprintf(tick, sizeof(tick), "%d", n);
        lab::caption(screen, tx - 20, y + plotH + 9, 40, tick, fui::TextAlign::Center);
      }
    }
    y += plotH + 9 + toybox::kTileCut.inkHeight + 12;

    // One cycle of the wave.
    lab::caption(screen, x, y, inner, "ONE CYCLE OF THE WAVE");
    y += toybox::kTileCut.inkHeight + 6;
    const int waveH = bottom - 78 - y;
    const int mid = y + waveH / 2;
    lab::box(screen, x, mid, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
    int lastY = mid;
    for (int px = 0; px < inner; ++px) {
      const double s = fm::wave(c, m, index, static_cast<double>(px) / inner);
      const int py = mid - static_cast<int>(s * (waveH / 2 - 3));
      const int lo = py < lastY ? py : lastY;
      const int hi = py < lastY ? lastY : py;
      lab::ink(screen, x + px, lo - 1, 2, hi - lo + 3);
      lastY = py;
    }
    lab::paragraph(screen, x, bottom - 66, inner, fm::ratioCharacter(c, m), 3);
  } else {
    // The shape, then the eight numbers that make it.
    const int plotH = 190;
    float ex[6];
    float ey[6];
    fm::envelopeShape(envelope_, ex, ey);
    const int plotX = x + 6;
    const int plotW = inner - 12;
    auto px = [&](const int i) { return plotX + static_cast<int>(ex[i] * static_cast<float>(plotW)); };
    auto py = [&](const int i) { return y + plotH - 4 - static_cast<int>(ey[i] * static_cast<float>(plotH - 12)); };
    // Shade under the line, then the line.
    for (int i = 0; i < 5; ++i) {
      for (int sx = px(i); sx <= px(i + 1); ++sx) {
        const float t = px(i + 1) == px(i) ? 0 : static_cast<float>(sx - px(i)) / static_cast<float>(px(i + 1) - px(i));
        const int sy = py(i) + static_cast<int>(static_cast<float>(py(i + 1) - py(i)) * t);
        lab::box(screen, sx, sy, 1, y + plotH - sy, fui::Paint::dither(fui::Color::LightGray));
        lab::ink(screen, sx, sy - 2, 2, 4);
      }
    }
    lab::ink(screen, x, y + plotH, inner, 2);
    // Where the key lets go.
    for (int dy = y; dy < y + plotH; dy += 6) lab::ink(screen, px(4), dy, 1, 3);
    lab::caption(screen, px(4) - 150, y + 2, 144, "KEY HELD", fui::TextAlign::Right);
    for (int i = 1; i <= 4; ++i) {
      toybox::disc(screen, static_cast<int16_t>(px(i)), static_cast<int16_t>(py(i)), 6, fui::Color::Black);
      toybox::disc(screen, static_cast<int16_t>(px(i)), static_cast<int16_t>(py(i)), 3, fui::Color::White);
    }
    y += plotH + 14;

    const int gap = 6;
    const int cellW = (inner - gap * 3) / 4;
    for (int f = 0; f < 8; ++f) {
      const bool rate = f < 4;
      const fui::Rect r = lab::rect(x + (f % 4) * (cellW + gap), y + (f / 4) * (62 + gap), cellW, 62);
      const bool on = f == envelopeField_;
      lab::button(screen, r, "", ActionField, f, on);
      const fui::Color colour = on ? fui::Color::White : fui::Color::Black;
      char name[16];
      std::snprintf(name, sizeof(name), "%s %d", rate ? "RATE" : "LEVEL", f % 4 + 1);
      lab::caption(screen, r.x, r.y + 10, cellW, name, fui::TextAlign::Center, colour);
      char number[toybox::kIntChars + 1];
      std::snprintf(number, sizeof(number), "%d", rate ? envelope_.rate[f] : envelope_.level[f - 4]);
      lab::value(screen, r.x, r.y + 10 + toybox::kTileCut.inkHeight + 8, cellW, number, fui::TextAlign::Center, colour);
    }
    y += 2 * (62 + gap) + 4;
    static const char* const kNudges[4] = {"-10", "-1", "+1", "+10"};
    for (int n = 0; n < 4; ++n) {
      lab::button(screen, lab::rect(x + n * (cellW + gap), y, cellW, 48), kNudges[n], ActionNudge, n, false, false);
    }
    y += 48 + 14;
    lab::caption(screen, x, y, inner, "START FROM A SHAPE");
    y += toybox::kTileCut.inkHeight + 8;
    const int presetW = (inner - gap * 2) / 3;
    for (int p = 0; p < fm::kEnvelopePresets; ++p) {
      lab::button(screen, lab::rect(x + (p % 3) * (presetW + gap), y + (p / 3) * (42 + gap), presetW, 42),
                  fm::envelopeName(p), ActionPreset, p);
    }
    y += 2 * (42 + gap) + 10;
    lab::paragraph(screen, x, y, inner,
                   "RATE 99 IS INSTANT, LEVEL 99 IS FULL. ON A CARRIER THE ENVELOPE SHAPES THE VOLUME; ON A "
                   "MODULATOR IT SHAPES THE BRIGHTNESS.",
                   3);
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "FmLab");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void FmLabActivity::saveState() {
  const uint8_t data[15] = {kSaveVersion,
                            static_cast<uint8_t>(tab_),
                            static_cast<uint8_t>(algorithm_),
                            static_cast<uint8_t>(carrier_),
                            static_cast<uint8_t>(modulator_),
                            static_cast<uint8_t>(index_),
                            static_cast<uint8_t>(envelopeField_),
                            envelope_.rate[0],
                            envelope_.rate[1],
                            envelope_.rate[2],
                            envelope_.rate[3],
                            envelope_.level[0],
                            envelope_.level[1],
                            envelope_.level[2],
                            envelope_.level[3]};
  HalFile file;
  if (!Storage.openFileForWrite("FMLB", kSavePath, file)) return;
  file.write(data, sizeof(data));
  file.flush();
}

void FmLabActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("FMLB", kSavePath, file)) return;
  uint8_t data[15] = {};
  if (file.read(data, sizeof(data)) != sizeof(data) || data[0] != kSaveVersion) return;
  tab_ = clamp(data[1], 0, 2);
  algorithm_ = clamp(data[2], 1, fm::kAlgorithms);
  carrier_ = clamp(data[3], 0, kRatioCount - 1);
  modulator_ = clamp(data[4], 0, kRatioCount - 1);
  index_ = clamp(data[5], 0, kMaxIndex);
  envelopeField_ = clamp(data[6], 0, 7);
  for (int i = 0; i < 4; ++i) {
    envelope_.rate[i] = static_cast<uint8_t>(clamp(data[7 + i], 0, 99));
    envelope_.level[i] = static_cast<uint8_t>(clamp(data[11 + i], 0, 99));
  }
}
