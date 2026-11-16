#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"

// MUSIC THEORY LAB: scales, chords and progressions spelt out in any key on a
// drawn keyboard, and Euclidean rhythms on a wheel.
class TheoryActivity final : public Activity {
 public:
  TheoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Theory", renderer, mappedInput) {}
  ~TheoryActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadState();
  void saveState();

  int tab_ = 0;
  int root_ = 0;
  int scale_ = 0;
  int chord_ = 0;
  int progression_ = 0;
  int steps_ = 8;
  int pulses_ = 3;
  int rotation_ = 0;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
