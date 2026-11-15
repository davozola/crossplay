#include "PomodoroActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>

#include "../../HapticFeedback.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "fontIds.h"

// The Buzzer library is in the device builds' lib_deps; the browser build does
// not have it, and a board without a buzzer links stubs whose begin() is false.
#if __has_include(<Buzzer.h>)
#include <Buzzer.h>
#define POMODORO_HAS_BUZZER 1
#else
#define POMODORO_HAS_BUZZER 0
#endif

namespace {

namespace pd = pomodoro;

constexpr char kSavePath[] = "/.crosspoint/pomodoro.sav";
constexpr uint8_t kSaveVersion = 1;

struct SaveState {
  uint8_t focusMinutes;
  uint8_t breakMinutes;
  uint8_t phase;
  uint8_t doneInCycle;
  uint32_t totalDone;
} __attribute__((packed));

// Seven segments, a..g, as on every kitchen timer. Bit 0 is a.
//   aaa
//  f   b
//   ggg
//  e   c
//   ddd
constexpr uint8_t kSegments[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

}  // namespace

std::unique_ptr<Activity> PomodoroActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<PomodoroActivity>(renderer, mappedInput);
}

void PomodoroActivity::onEnter() {
  Activity::onEnter();
  loadState();
  shownMinutes = -1;
  justFinished = false;
  flashOnNextPaint = false;
  requestUpdate();
}

void PomodoroActivity::onExit() {
  saveState();
  Activity::onExit();
}

PomodoroActivity::Layout PomodoroActivity::layout() const {
  const auto& m = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  const int side = m.contentSidePadding > 0 ? m.contentSidePadding * 2 : 24;
  const int innerW = sw - 2 * side;
  const int gap = 16;

  Layout l;
  int y = m.topPadding + m.headerHeight + m.verticalSpacing + 8;

  const int phaseH = renderer.getLineHeight(NOTOSANS_18_FONT_ID) + 8;
  l.phase = Rect{side, y, innerW, phaseH};
  y += phaseH + gap;

  // The digits take what the controls leave, capped so they do not swallow a
  // landscape screen: roughly a quarter of the height in portrait.
  const int bottom = sh - m.buttonHintsHeight - m.verticalSpacing;
  const int buttonH = 72;
  const int settingH = 56;
  const int controlsH = settingH + gap + buttonH + gap + buttonH;
  const int captionH = renderer.getLineHeight(UI_12_FONT_ID) + 6;
  const int barH = 18;
  const int dotsH = 28;
  int digitsH = bottom - controlsH - gap - y - (gap + captionH + gap + barH + gap + dotsH + gap);
  if (digitsH > sh / 4) digitsH = sh / 4;
  if (digitsH < 60) digitsH = 60;
  l.digits = Rect{side, y, innerW, digitsH};
  y += digitsH + gap;

  l.caption = Rect{side, y, innerW, captionH};
  y += captionH + gap;
  l.bar = Rect{side, y, innerW, barH};
  y += barH + gap;
  l.dots = Rect{side, y, innerW, dotsH};

  // Controls hang from the bottom so they stay under the thumb whatever the
  // digits did.
  int cy = bottom - buttonH;
  const int half = (innerW - gap) / 2;
  l.skip = Rect{side, cy, half, buttonH};
  l.reset = Rect{side + half + gap, cy, innerW - half - gap, buttonH};
  cy -= gap + buttonH;
  l.primary = Rect{side, cy, innerW, buttonH};
  cy -= gap + settingH;
  l.focusSetting = Rect{side, cy, half, settingH};
  l.breakSetting = Rect{side + half + gap, cy, innerW - half - gap, settingH};
  return l;
}

namespace {
bool inside(const Rect& r, const int x, const int y) {
  return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
}
}  // namespace

void PomodoroActivity::togglePrimary() {
  justFinished = false;
  timer.toggle(static_cast<uint32_t>(millis()));
  shownMinutes = -1;
  requestUpdate();
}

void PomodoroActivity::handleTap(const int x, const int y) {
  const Layout l = layout();
  const bool ready = timer.state() == pd::State::Ready;
  if (inside(l.primary, x, y)) {
    haptic_feedback::touchAction();
    togglePrimary();
  } else if (inside(l.skip, x, y)) {
    haptic_feedback::touchAction();
    justFinished = false;
    timer.skip();
    saveState();
    requestUpdate();
  } else if (inside(l.reset, x, y)) {
    haptic_feedback::touchAction();
    justFinished = false;
    timer.reset();
    saveState();
    requestUpdate();
  } else if (ready && inside(l.focusSetting, x, y)) {
    // Lengths change only while stopped: shortening a running session under
    // itself would either end it on the spot or quietly move the goalposts.
    haptic_feedback::touchAction();
    timer.cycleFocus();
    saveState();
    requestUpdate();
  } else if (ready && inside(l.breakSetting, x, y)) {
    haptic_feedback::touchAction();
    timer.cycleBreak();
    saveState();
    requestUpdate();
  }
}

void PomodoroActivity::ring() {
  flashOnNextPaint = true;
  justFinished = true;
  haptic_feedback::touchAction(true);
#if POMODORO_HAS_BUZZER
  Buzzer buzzer;
  if (buzzer.begin()) {
    // Three short rising beeps. tone() blocks, which is fine here: the whole
    // thing is under a second and there is nothing else to do at the bell.
    const uint32_t notes[] = {1760, 2093, 2637};
    for (int round = 0; round < 2; ++round) {
      for (const uint32_t f : notes) {
        buzzer.tone(f, 120);
        delay(60);
      }
      delay(250);
    }
    buzzer.end();
  }
#endif
}

void PomodoroActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    saveState();
    shelf::leave(renderer, mappedInput);
    return;
  }

  const uint32_t now = static_cast<uint32_t>(millis());
  if (timer.tick(now)) {
    ring();
    saveState();
    shownMinutes = -1;
    requestUpdate();
    return;
  }
  if (timer.state() == pd::State::Running && timer.remainingMinutes(now) != shownMinutes) {
    requestUpdate();
  }

  // Two keys and touch is the floor (docs/buttons.md). Next/Down and the
  // Sticky's Confirm are the big button; Previous/Up skips, but only while
  // stopped, so a key brushed in a bag cannot throw away twenty minutes.
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) ||
      mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    togglePrimary();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up) && timer.state() != pd::State::Running) {
    justFinished = false;
    timer.skip();
    saveState();
    requestUpdate();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) handleTap(tapX, tapY);
}

void PomodoroActivity::drawButton(const Rect& r, const char* label, const bool filled) const {
  constexpr int radius = 12;
  if (filled) {
    renderer.fillRoundedRect(r.x, r.y, r.width, r.height, radius, Color::Black);
  } else {
    renderer.drawRoundedRect(r.x, r.y, r.width, r.height, 3, radius, true);
  }
  const int font = UI_12_FONT_ID;
  const int tw = renderer.getTextWidth(font, label, EpdFontFamily::BOLD);
  const int th = renderer.getLineHeight(font);
  renderer.drawText(font, r.x + (r.width - tw) / 2, r.y + (r.height - th) / 2, label, !filled, EpdFontFamily::BOLD);
}

void PomodoroActivity::drawDigits(const Rect& r, const int value) const {
  const int digits = value >= 10 ? 2 : 1;
  const int h = r.height;
  const int w = h * 55 / 100;
  const int t = h / 9 > 4 ? h / 9 : 4;
  const int spacing = t * 2;
  const int total = digits * w + (digits - 1) * spacing;
  int x = r.x + (r.width - total) / 2;
  const int y = r.y;
  const int half = h / 2;

  for (int i = digits - 1; i >= 0; --i) {
    int d = value;
    for (int k = 0; k < i; ++k) d /= 10;
    d %= 10;
    const uint8_t s = kSegments[d];
    // Horizontal bars inset by t so the corners read as a digit, not a frame.
    if (s & 0x01) renderer.fillRect(x + t, y, w - 2 * t, t);                            // a
    if (s & 0x02) renderer.fillRect(x + w - t, y + t, t, half - t - t / 2);             // b
    if (s & 0x04) renderer.fillRect(x + w - t, y + half + t / 2, t, half - t - t / 2);  // c
    if (s & 0x08) renderer.fillRect(x + t, y + h - t, w - 2 * t, t);                    // d
    if (s & 0x10) renderer.fillRect(x, y + half + t / 2, t, half - t - t / 2);          // e
    if (s & 0x20) renderer.fillRect(x, y + t, t, half - t - t / 2);                     // f
    if (s & 0x40) renderer.fillRect(x + t, y + half - t / 2, w - 2 * t, t);             // g
    x += w + spacing;
  }
}

void PomodoroActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int sw = renderer.getScreenWidth();
  const uint32_t now = static_cast<uint32_t>(millis());
  const Layout l = layout();
  const pd::State state = timer.state();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, sw, metrics.headerHeight}, "Pomodoro");

  // Phase, as a black band during a break so the screen reads at a glance from
  // across the desk: white is work, black is rest.
  const bool resting = timer.phase() != pd::Phase::Focus;
  if (resting) renderer.fillRoundedRect(l.phase.x, l.phase.y, l.phase.width, l.phase.height, 10, Color::Black);
  {
    const char* label = pd::phaseLabel(timer.phase());
    const int tw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, label, EpdFontFamily::BOLD);
    const int th = renderer.getLineHeight(NOTOSANS_18_FONT_ID);
    renderer.drawText(NOTOSANS_18_FONT_ID, l.phase.x + (l.phase.width - tw) / 2, l.phase.y + (l.phase.height - th) / 2,
                      label, !resting, EpdFontFamily::BOLD);
  }

  const int minutes = timer.remainingMinutes(now);
  shownMinutes = minutes;
  drawDigits(l.digits, minutes);

  char caption[48];
  if (justFinished) {
    std::snprintf(caption, sizeof(caption), "Done. %s next.", timer.phase() == pd::Phase::Focus ? "Focus" : "Break");
  } else if (state == pd::State::Paused) {
    std::snprintf(caption, sizeof(caption), "PAUSED  -  %d MIN LEFT", minutes);
  } else if (state == pd::State::Ready) {
    std::snprintf(caption, sizeof(caption), "MINUTES  -  TAP START");
  } else {
    std::snprintf(caption, sizeof(caption), "MINUTES LEFT");
  }
  UITheme::drawCenteredText(renderer, l.caption, UI_12_FONT_ID, l.caption.y, caption);

  // Progress bar.
  renderer.drawRect(l.bar.x, l.bar.y, l.bar.width, l.bar.height, 2, true);
  const int fill = (l.bar.width - 8) * timer.progressPermille(now) / 1000;
  if (fill > 0) renderer.fillRect(l.bar.x + 4, l.bar.y + 4, fill, l.bar.height - 8);

  // Where you are in the cycle: one dot per focus session, filled once done.
  {
    const int d = l.dots.height - 6;
    const int gap = d;
    const int total = pd::kSessionsPerCycle * d + (pd::kSessionsPerCycle - 1) * gap;
    int x = l.dots.x + (l.dots.width - total) / 2;
    for (int i = 0; i < pd::kSessionsPerCycle; ++i) {
      if (i < timer.doneInCycle()) {
        renderer.fillRoundedRect(x, l.dots.y, d, d, d / 2, Color::Black);
      } else {
        renderer.drawRoundedRect(x, l.dots.y, d, d, 2, d / 2, true);
      }
      x += d + gap;
    }
    char total_[24];
    std::snprintf(total_, sizeof(total_), "%lu done", static_cast<unsigned long>(timer.totalDone()));
    renderer.drawText(SMALL_FONT_ID, l.dots.x, l.dots.y + (d - renderer.getLineHeight(SMALL_FONT_ID)) / 2, total_);
  }

  // Settings, only meaningful while stopped; drawn as plain labels otherwise.
  char focusLabel[24];
  char breakLabel[24];
  std::snprintf(focusLabel, sizeof(focusLabel), "FOCUS %d MIN", timer.focusMinutes);
  std::snprintf(breakLabel, sizeof(breakLabel), "BREAK %d MIN", timer.breakMinutes);
  if (state == pd::State::Ready) {
    drawButton(l.focusSetting, focusLabel, false);
    drawButton(l.breakSetting, breakLabel, false);
  } else {
    UITheme::drawCenteredText(renderer, l.focusSetting, UI_10_FONT_ID,
                              l.focusSetting.y + (l.focusSetting.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                              focusLabel);
    UITheme::drawCenteredText(renderer, l.breakSetting, UI_10_FONT_ID,
                              l.breakSetting.y + (l.breakSetting.height - renderer.getLineHeight(UI_10_FONT_ID)) / 2,
                              breakLabel);
  }

  const char* primary = state == pd::State::Running ? "PAUSE" : (state == pd::State::Paused ? "RESUME" : "START");
  drawButton(l.primary, primary, true);
  drawButton(l.skip, "SKIP", false);
  drawButton(l.reset, "RESET", false);

  const auto labels = mappedInput.mapLabels("Back", primary, state == pd::State::Running ? "" : "Skip", primary);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer(flashOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  flashOnNextPaint = false;
}

void PomodoroActivity::saveState() {
  SaveState s{};
  s.focusMinutes = static_cast<uint8_t>(timer.focusMinutes);
  s.breakMinutes = static_cast<uint8_t>(timer.breakMinutes);
  s.phase = static_cast<uint8_t>(timer.phase());
  s.doneInCycle = static_cast<uint8_t>(timer.doneInCycle());
  s.totalDone = timer.totalDone();
  HalFile file;
  if (!Storage.openFileForWrite("POMO", kSavePath, file)) return;
  const uint8_t version = kSaveVersion;
  file.write(&version, 1);
  file.write(reinterpret_cast<const uint8_t*>(&s), sizeof(s));
  file.flush();
}

void PomodoroActivity::loadState() {
  timer = pd::Timer{};
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("POMO", kSavePath, file)) return;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kSaveVersion) return;
  SaveState s{};
  if (file.read(reinterpret_cast<uint8_t*>(&s), sizeof(s)) != sizeof(s)) return;
  // Clamped against the choice tables as they are now, so an edited or old
  // file cannot set a length the settings rows would never offer.
  for (const int c : pd::kFocusChoices) {
    if (c == s.focusMinutes) timer.focusMinutes = c;
  }
  for (const int c : pd::kBreakChoices) {
    if (c == s.breakMinutes) timer.breakMinutes = c;
  }
  timer.restore(static_cast<pd::Phase>(s.phase <= 2 ? s.phase : 0), s.doneInCycle);
  timer.setTotalDone(s.totalDone);
}
