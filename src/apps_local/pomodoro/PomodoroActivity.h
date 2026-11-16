#pragma once

// POMODORO: a focus timer for a screen that is good at holding still.
//
// The panel is redrawn once a minute, never once a second. A ticking seconds
// field on e-ink is a quarter-second flicker sixty times a minute for
// information nobody at a desk needs; the minute count and the ring say how far
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
#include "../ui/ToyboxScreen.h"
#include "PomodoroCore.h"
#include "PomodoroScreens.h"

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

 private:
  enum class View : uint8_t { Timer, PickFocus, PickBreak };

  void route(const freeink::ui::ActionEvent& event);
  void togglePrimary();
  void skip();
  void ring();
  void loadState();
  void saveState();

  pomodoro::Timer timer;
  View view = View::Timer;
  int shownMinutes = -1;
  bool justFinished = false;
  bool flashOnNextPaint = false;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
