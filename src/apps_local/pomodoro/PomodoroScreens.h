#pragma once

// POMODORO on screen: the timer and the two length pickers, as freestanding
// builders over toybox::Screen.

#include "../ui/ToyboxScreen.h"
#include "PomodoroCore.h"

namespace pomodoroui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionPrimary = 1,
  ActionSkip,
  ActionReset,
  ActionPickFocus,
  ActionPickBreak,
  ActionChoose,  // value: minutes
};

// Lends the builder the huge cut for the one number in the ring; see
// weatherui::FaceSwap for why a builder has to ask.
struct FaceSwap {
  void* ctx = nullptr;
  void (*bindTitle)(void* ctx, bool huge) = nullptr;
};

struct TimerModel {
  pomodoro::Phase phase = pomodoro::Phase::Focus;
  pomodoro::State state = pomodoro::State::Ready;
  int minutes = 25;  // left, or the phase's length while ready
  int progressPermille = 0;
  int doneInCycle = 0;
  uint32_t totalDone = 0;
  int focusMinutes = 25;
  int breakMinutes = 5;
  bool justFinished = false;
  FaceSwap faces;
};

struct PickerModel {
  bool focus = true;
  int current = 25;
};

void buildTimer(toybox::Screen& screen, const TimerModel& model);
void buildPicker(toybox::Screen& screen, const PickerModel& model);

}  // namespace pomodoroui
