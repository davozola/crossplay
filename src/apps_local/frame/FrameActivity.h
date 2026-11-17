#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"

// MARCO: the photo frame. How often a sleeping reader changes the picture on
// its glass, and whether it has what it needs to do so: a set of two or more
// pictures chosen in WALLPAPERS. The changing itself is wakejobs' work.
class FrameActivity final : public Activity {
 public:
  FrameActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Frame", renderer, mappedInput) {}
  ~FrameActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void readCard();
  void buildPage(toybox::Screen& screen);

  int choice_ = 0;
  // What the card says, read on entry: the pictures in the set, whether one
  // picture is pinned over it, and how many the frame has changed so far.
  int inSet_ = 0;
  bool pinned_ = false;
  uint32_t changes_ = 0;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
