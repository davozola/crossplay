#pragma once

// INNER CLOCK on the device. Both taps are read on the touch-DOWN edge and
// stamped with millis() in the loop, before anything is drawn: the panel takes
// a third of a second to show "COUNTING", and none of that may leak into the
// measurement. Taps are sampled every loop pass, so a reading is good to about
// a hundredth of a second, which is what the two decimals claim.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "InnerCore.h"

class InnerClockActivity final : public Activity {
 public:
  InnerClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("InnerClock", renderer, mappedInput) {}
  ~InnerClockActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return round.state() == innerclock::State::Running; }

 private:
  void loadState();
  void saveState();

  innerclock::Round round;
  innerclock::Record record;
  // When the last press was taken, so the bounce of one tap cannot also be the
  // next one.
  uint32_t lastPressMs = 0;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
