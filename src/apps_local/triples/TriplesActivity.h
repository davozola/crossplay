#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxHowToState.h"
#include "../ui/ToyboxScreen.h"
#include "TriplesCore.h"
#include "TriplesScreens.h"

// TRIPLES: slide the board, make threes, double them. Played with swipes, which
// is why a swipe to the right that starts on the board is a move here and not
// the way out: Back is a swipe from the very edge of the glass.
class TriplesActivity final : public Activity {
 public:
  TriplesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Triples", renderer, mappedInput) {}
  ~TriplesActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void play(triples::Dir dir);
  void loadState();
  void saveState();

  triples::Game game_;
  uint32_t best_ = 0;
  int entered_ = -1;
  bool over_ = false;
  // Where the touch in progress began; -1 between touches.
  int pressX_ = -1;
  uint32_t newArmedMs_ = 0;

  toyboxhowto::State howTo_;
  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
