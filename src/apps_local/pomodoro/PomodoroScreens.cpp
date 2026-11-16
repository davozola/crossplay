#include "PomodoroScreens.h"

#include <cmath>
#include <cstdio>

#include "../ui/ToyboxFormat.h"

namespace pomodoroui {

namespace {

namespace pd = pomodoro;

constexpr int kBottomReserve = 40;  // the button hints
constexpr int kRadius = toybox::kPillRadius / 2;

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);

fui::Rect rect(const int x, const int y, const int w, const int h) {
  return fui::makeRect(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w),
                       static_cast<int16_t>(h));
}

void box(toybox::Screen& screen, const int x, const int y, const int w, const int h, const fui::Paint& paint,
         const int radius = 0) {
  if (w <= 0 || h <= 0) return;
  screen.target().fill(rect(x, y, w, h), paint, static_cast<uint8_t>(radius));
}

// Text whose ink (cap height) is centred in `r` vertically.
void label(toybox::Screen& screen, const fui::Rect& r, const char* text, const fui::FontId font,
           const toybox::CutMetrics& cut, const fui::TextAlign align = fui::TextAlign::Center,
           const fui::Color color = fui::Color::Black) {
  fui::TextStyle s;
  s.font = font;
  s.align = align;
  s.color = color;
  s.maxLines = 1;
  screen.target().text(toybox::inkCentred(r, cut), text, s);
}

void caption(toybox::Screen& screen, const int x, const int y, const int w, const char* text,
             const fui::TextAlign align = fui::TextAlign::Center, const fui::Color color = fui::Color::Black) {
  label(screen, rect(x, y, w, toybox::kTileCut.inkHeight), text, toybox::kTileFont, toybox::kTileCut, align, color);
}

void disc(toybox::Screen& screen, const int cx, const int cy, const int r, const fui::Color c) {
  if (r > 0) toybox::disc(screen, static_cast<int16_t>(cx), static_cast<int16_t>(cy), static_cast<int16_t>(r), c);
}

// The part of the annulus between `rIn` and `rOut` swept clockwise from twelve
// o'clock through `permille` of the turn. Walked a row at a time and painted in
// runs, so a full ring is a few hundred fills rather than one per pixel.
void arc(toybox::Screen& screen, const int cx, const int cy, const int rOut, const int rIn, const int permille) {
  if (permille <= 0) return;
  const float limit = static_cast<float>(permille > 1000 ? 1000 : permille) * 6.2831853f / 1000.0f;
  const int out2 = rOut * rOut;
  const int in2 = rIn * rIn;
  for (int dy = -rOut; dy <= rOut; ++dy) {
    int run = 0;
    int runStart = 0;
    for (int dx = -rOut; dx <= rOut + 1; ++dx) {
      bool on = false;
      const int d2 = dx * dx + dy * dy;
      if (dx <= rOut && d2 <= out2 && d2 >= in2) {
        float a = std::atan2(static_cast<float>(dx), static_cast<float>(-dy));
        if (a < 0) a += 6.2831853f;
        on = a <= limit;
      }
      if (on) {
        if (run == 0) runStart = dx;
        ++run;
      } else if (run > 0) {
        box(screen, cx + runStart, cy + dy, run, 1, kInk);
        run = 0;
      }
    }
  }
}

// A small downward chevron: "this opens a list".
void chevron(toybox::Screen& screen, const int cx, const int cy, const fui::Color colour) {
  const fui::Paint p = fui::Paint::solid(colour);
  for (int i = 0; i < 6; ++i) {
    box(screen, cx - 7 + i, cy - 3 + i, 3, 1, p);
    box(screen, cx + 5 - i, cy - 3 + i, 3, 1, p);
  }
}

void chrome(toybox::Screen& screen, const char* title, const char* right) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = right;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kSmallFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  header.sidePadding = screen.theme().headerSidePadding;
  toybox::headerBand(screen, header);
}

// A length setting: caption over value, a chevron when it can be opened.
void settingCell(toybox::Screen& screen, const fui::Rect& r, const char* name, const int minutes,
                 const fui::ActionId action, const bool open) {
  if (open) {
    fui::ButtonProps b;
    b.label = "";
    b.action = action;
    b.styles = toybox::rowStyles();
    b.radius = kRadius;
    screen.button(b, r);
    chevron(screen, r.x + r.width - 24, r.y + r.height / 2, fui::Color::Black);
  }
  const int textX = r.x + 16;
  const int textW = r.width - 32 - (open ? 24 : 0);
  const int capY = r.y + 11;
  caption(screen, textX, capY, textW, name, fui::TextAlign::Left);
  char value[toybox::kIntChars + 5];
  std::snprintf(value, sizeof(value), "%d MIN", minutes);
  label(screen, rect(textX, capY + toybox::kTileCut.inkHeight + 7, textW, toybox::kUiCut.inkHeight), value,
        toybox::kUiFont, toybox::kUiCut, fui::TextAlign::Left);
}

}  // namespace

void buildTimer(toybox::Screen& screen, const TimerModel& model) {
  char done[toybox::kIntChars + 6];
  std::snprintf(done, sizeof(done), "%lu DONE", static_cast<unsigned long>(model.totalDone));
  chrome(screen, "POMODORO", done);

  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const bool resting = model.phase != pd::Phase::Focus;
  const bool ready = model.state == pd::State::Ready;

  // Controls hang from the bottom; the ring takes what is left above them.
  const int bottom = device.height - kBottomReserve - 6;
  const int smallH = 50;
  const int primaryH = 64;
  const int cellH = 64;
  const int gap = 12;
  const int smallY = bottom - smallH;
  const int primaryY = smallY - gap - primaryH;
  const int cellY = primaryY - gap - cellH;

  int y = toybox::kChromeHeight + 26;
  label(screen, rect(toybox::kMargin, y, inner, toybox::kDisplayCut.inkHeight), pd::phaseLabel(model.phase),
        toybox::kDisplayFont, toybox::kDisplayCut);
  y += toybox::kDisplayCut.inkHeight + 16;

  // Dots and the status line sit between the ring and the controls.
  const int dotsH = 14;
  const int statusY = cellY - 18 - toybox::kTileCut.inkHeight;
  const int dotsY = statusY - 14 - dotsH;
  const int ringSpace = dotsY - 16 - y;
  int r = ringSpace / 2;
  if (r > (inner - 40) / 2) r = (inner - 40) / 2;
  const int cx = width / 2;
  const int cy = y + ringSpace / 2;
  const int band = 16;

  // White is work, black is rest: a break fills the dial.
  disc(screen, cx, cy, r - band / 2 + 2, fui::Color::Black);
  disc(screen, cx, cy, r - band / 2 - 1, fui::Color::White);
  arc(screen, cx, cy, r, r - band, model.progressPermille);
  if (resting) disc(screen, cx, cy, r - band - 10, fui::Color::Black);
  const fui::Color ink = resting ? fui::Color::White : fui::Color::Black;

  char minutes[toybox::kIntChars + 1];
  std::snprintf(minutes, sizeof(minutes), "%d", model.minutes);
  const bool huge = model.faces.bindTitle != nullptr;
  const toybox::CutMetrics& cut = huge ? toybox::kHugeCut : toybox::kDisplayCut;
  const int numTop = cy - cut.inkHeight / 2 - 16;
  if (huge) model.faces.bindTitle(model.faces.ctx, true);
  label(screen, rect(cx - r + band + 8, numTop, (r - band - 8) * 2, cut.inkHeight), minutes, toybox::kDisplayFont, cut,
        fui::TextAlign::Center, ink);
  if (huge) model.faces.bindTitle(model.faces.ctx, false);
  const char* unit = "MINUTES LEFT";
  if (ready) unit = model.minutes == 1 ? "MINUTE" : "MINUTES";
  if (model.state == pd::State::Paused) unit = "PAUSED";
  caption(screen, cx - r + band, numTop + cut.inkHeight + 18, (r - band) * 2, unit, fui::TextAlign::Center, ink);

  // One dot per focus session of the cycle, filled once done.
  {
    const int step = dotsH + 14;
    int x = cx - (pd::kSessionsPerCycle * dotsH + (pd::kSessionsPerCycle - 1) * 14) / 2 + dotsH / 2;
    for (int i = 0; i < pd::kSessionsPerCycle; ++i) {
      disc(screen, x, dotsY + dotsH / 2, dotsH / 2, fui::Color::Black);
      if (i >= model.doneInCycle) disc(screen, x, dotsY + dotsH / 2, dotsH / 2 - 2, fui::Color::White);
      x += step;
    }
  }
  char status[48];
  if (model.justFinished) {
    std::snprintf(status, sizeof(status), "TIME. %s IS NEXT.", resting ? "A BREAK" : "FOCUS");
  } else if (resting) {
    std::snprintf(status, sizeof(status), "%d OF %d SESSIONS DONE", model.doneInCycle, pd::kSessionsPerCycle);
  } else {
    const int session = model.doneInCycle + 1 > pd::kSessionsPerCycle ? pd::kSessionsPerCycle : model.doneInCycle + 1;
    std::snprintf(status, sizeof(status), "SESSION %d OF %d", session, pd::kSessionsPerCycle);
  }
  caption(screen, toybox::kMargin, statusY, inner, status);

  // Lengths open their picker only while stopped: shortening a running session
  // under itself would end it on the spot or quietly move the goalposts.
  const int half = (inner - gap) / 2;
  settingCell(screen, rect(toybox::kMargin, cellY, half, cellH), "FOCUS", model.focusMinutes, ActionPickFocus, ready);
  settingCell(screen, rect(toybox::kMargin + half + gap, cellY, half, cellH), "BREAK", model.breakMinutes,
              ActionPickBreak, ready);

  fui::ButtonProps primary;
  primary.label = model.state == pd::State::Running ? "PAUSE" : (model.state == pd::State::Paused ? "RESUME" : "START");
  primary.action = ActionPrimary;
  primary.styles = toybox::rowStyles();
  primary.state = fui::StateSelected;
  primary.radius = kRadius;
  screen.button(primary, rect(toybox::kMargin, primaryY, inner, primaryH));

  fui::ButtonProps skip;
  skip.label = "SKIP";
  skip.action = ActionSkip;
  skip.styles = toybox::rowStyles();
  skip.text = toybox::buttonText(screen.theme());
  skip.radius = kRadius;
  screen.button(skip, rect(toybox::kMargin, smallY, half, smallH));
  fui::ButtonProps reset = skip;
  reset.label = "RESET";
  reset.action = ActionReset;
  screen.button(reset, rect(toybox::kMargin + half + gap, smallY, half, smallH));
}

void buildPicker(toybox::Screen& screen, const PickerModel& model) {
  chrome(screen, model.focus ? "FOCUS" : "BREAK", nullptr);
  const fui::DeviceContext device = screen.device();
  const int width = device.width;
  const int inner = width - toybox::kMargin * 2;
  const int* choices = model.focus ? pd::kFocusChoices : pd::kBreakChoices;
  const int count = model.focus ? pd::kFocusChoiceCount : pd::kBreakChoiceCount;

  int y = toybox::kBodyTop - 12;
  caption(screen, toybox::kMargin, y, inner,
          model.focus ? "HOW LONG IS A FOCUS SESSION?" : "HOW LONG IS A SHORT BREAK?", fui::TextAlign::Left);
  y += toybox::kTileCut.inkHeight + 16;

  const int cols = model.focus ? 3 : 2;
  const int gap = 10;
  const int tileW = (inner - gap * (cols - 1)) / cols;
  const int tileH = model.focus ? 132 : 150;
  for (int i = 0; i < count; ++i) {
    const int col = i % cols;
    const int row = i / cols;
    const fui::Rect r = rect(toybox::kMargin + col * (tileW + gap), y + row * (tileH + gap), tileW, tileH);
    const bool on = choices[i] == model.current;
    fui::ButtonProps tile;
    tile.label = "";
    tile.action = ActionChoose;
    tile.value = static_cast<int16_t>(choices[i]);
    tile.styles = toybox::rowStyles();
    tile.state = on ? fui::StateSelected : fui::StateNormal;
    tile.radius = kRadius;
    screen.button(tile, r);
    const fui::Color ink = on ? fui::Color::White : fui::Color::Black;
    char n[toybox::kIntChars + 1];
    std::snprintf(n, sizeof(n), "%d", choices[i]);
    const int numTop = r.y + (tileH - toybox::kDisplayCut.inkHeight - 12 - toybox::kTileCut.inkHeight) / 2;
    label(screen, rect(r.x, numTop, tileW, toybox::kDisplayCut.inkHeight), n, toybox::kDisplayFont, toybox::kDisplayCut,
          fui::TextAlign::Center, ink);
    caption(screen, r.x, numTop + toybox::kDisplayCut.inkHeight + 12, tileW, "MIN", fui::TextAlign::Center, ink);
  }
  const int rows = (count + cols - 1) / cols;
  y += rows * (tileH + gap) + 10;
  if (!model.focus) {
    char note[64];
    std::snprintf(note, sizeof(note), "THE LONG BREAK AFTER FOUR SESSIONS IS %d MIN",
                  model.current * pd::kLongBreakFactor);
    caption(screen, toybox::kMargin, y, inner, note, fui::TextAlign::Left);
  }
}

}  // namespace pomodoroui
