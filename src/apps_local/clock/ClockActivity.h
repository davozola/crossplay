#pragma once

#include <atomic>
#include <memory>

#include "../../activities/Activity.h"
#include "ClockFaces.h"

// RELOJ: a clock to leave standing, upright or on its side, in seven faces.
// Swipe across for the next face, up or down to turn it, tap to swap ink and
// paper. It redraws once a minute, and between minutes, once nobody has
// touched it for a while, the processor naps (see power/LightNap.h).
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

  // Reads which way up the device is and turns the face to match; true when
  // that changed anything.
  bool followGravity(uint32_t nowMs);

  // How the face is turned: by the device's own sense of down, or held.
  enum class Turn : uint8_t { Auto = 0, Tall = 1, Wide = 2 };
  Turn turn_ = Turn::Auto;
  bool flipped_ = false;  // lying on its other long edge
  uint8_t leaning_ = 0;   // the pose the sensor last read, and for how many reads
  uint8_t leaningFor_ = 0;
  uint32_t gravityAtMs_ = 0;
  const char* note_ = nullptr;  // said on the strip instead of the how-to

  int face_ = 0;
  bool wide_ = false;
  bool dark_ = false;
  int shownMinute_ = -1;      // the minute of the day on the panel, -1 for none
  bool cleanNext_ = true;     // the next paint clears ghosts with a fuller refresh
  uint32_t hintUntilMs_ = 0;  // the how-to strip shows until then
  int pressX_ = -1;
  // True from the moment a paint is asked for until the panel has taken it:
  // a nap must not begin while the render task is talking to the display.
  std::atomic<bool> paintPending_{false};
  void ask();                 // requestUpdate, with the flag
  uint32_t lastInputMs_ = 0;  // the last touch or key, for when napping may begin
};
