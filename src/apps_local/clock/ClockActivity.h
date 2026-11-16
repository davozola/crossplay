#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "ClockFaces.h"

// RELOJ: a clock to leave standing, upright or on its side, in seven faces.
// Swipe across for the next face, up or down to turn it, tap to swap ink and
// paper. It redraws once a minute and keeps the device awake while it is open.
class ClockActivity final : public Activity {
 public:
  ClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Clock", renderer, mappedInput) {}
  ~ClockActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  void loadState();
  void saveState();
  void applyOrientation();
  void changeFace(int by);
  clockfaces::Moment now() const;

  int face_ = 0;
  bool wide_ = false;
  bool dark_ = false;
  int shownMinute_ = -1;      // the minute of the day on the panel, -1 for none
  bool cleanNext_ = true;     // the next paint clears ghosts with a fuller refresh
  uint32_t hintUntilMs_ = 0;  // the how-to strip shows until then
  int pressX_ = -1;
};
