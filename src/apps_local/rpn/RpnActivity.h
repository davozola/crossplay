#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "RpnCore.h"

// RPN 12C: a financial calculator in reverse Polish, after the classic
// business one. Forty keys in a grid that is hit-tested by arithmetic, since
// they would not fit the interaction table.
class RpnActivity final : public Activity {
 public:
  RpnActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Rpn", renderer, mappedInput) {}
  ~RpnActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadState();
  void saveState();

  rpn::Calculator calc_;
  toybox::Interactions interactions_;
};
