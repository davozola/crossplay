#pragma once

// POMODORO: a focus timer for a screen that is good at holding still.
//
// The panel is redrawn once a minute, never once a second. A ticking seconds
// field on e-ink is a quarter-second flicker sixty times a minute for
// information nobody at a desk needs; the minute count and the bar say how far
// along you are, and the alarm says when you are done.
//
// The alarm is the Sticky's buzzer where there is one (the X4 Pro has none and
// links a silent stub), the vibration motor where there is one, and on every
// device a full refresh -- the black blink this panel makes anyway, used as
// punctuation the same way FOREHEAD uses it for its buzzer.
//
// The logic lives in PomodoroCore so host-tests/pomodoro can run it.

#include <memory>

#include "../../activities/Activity.h"
#include "../../components/themes/BaseTheme.h"
#include "PomodoroCore.h"

class PomodoroActivity final : public Activity {
 public:
  PomodoroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Pomodoro", renderer, mappedInput) {}
  ~PomodoroActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  // Only while the clock runs. A paused or finished timer lets the device
  // sleep like any other screen; nothing is lost but the seconds.
  bool preventAutoSleep() override { return timer.state() == pomodoro::State::Running; }

  // Shared by render() and the tap handler so the two cannot disagree about
  // where a button is.
  struct Layout {
    Rect phase;
    Rect digits;
    Rect caption;
    Rect bar;
    Rect dots;
    Rect focusSetting;
    Rect breakSetting;
    Rect primary;
    Rect skip;
    Rect reset;
  };

 private:
  Layout layout() const;
  void handleTap(int x, int y);
  void togglePrimary();
  void ring();
  void loadState();
  void saveState();
  void drawButton(const Rect& r, const char* label, bool filled) const;
  void drawDigits(const Rect& r, int value) const;

  pomodoro::Timer timer;
  int shownMinutes = -1;
  bool justFinished = false;
  bool flashOnNextPaint = false;
};
