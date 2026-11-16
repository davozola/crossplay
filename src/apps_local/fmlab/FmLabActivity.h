#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "FmCore.h"

// FM SYNTH LAB: a crib sheet for a six-operator FM synth. The 32 algorithms as
// diagrams, what a carrier and a modulator's ratio and index do to the
// spectrum, and the shape of a four-rate, four-level envelope.
class FmLabActivity final : public Activity {
 public:
  FmLabActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("FmLab", renderer, mappedInput) {}
  ~FmLabActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadState();
  void saveState();

  int tab_ = 0;
  int algorithm_ = 1;
  int carrier_ = 1;  // index into the ratio table
  int modulator_ = 1;
  int index_ = 4;  // halves: 4 is an index of 2.0
  fm::Envelope envelope_;
  int envelopeField_ = 0;  // 0..3 rates, 4..7 levels

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
