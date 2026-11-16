#pragma once

// HANDWRITING on the device: a notebook of pages you write on with a finger.
//
// Ink appears in bursts: the panel takes a third of a second per refresh, so
// while a finger is down the page is repainted about four times a second, and
// once more when it lifts. Every point is recorded as it is sampled; only the
// showing of it waits for the panel.
//
// Pages live on the card in /handwriting: page-NN.ink holds the strokes (what
// the app reads back) and page-NN.bmp the picture, which opens on a computer
// and can be copied to /wallpapers to become a sleep screen.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "InkCore.h"

class HandwritingActivity final : public Activity {
 public:
  HandwritingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Handwriting", renderer, mappedInput) {}
  ~HandwritingActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return drawing || dirty; }

 private:
  void loadIndex();
  void saveIndex();
  void loadPage(int number);
  void savePage();
  void goToPage(int number);

  // ~48KB of strokes and ~40KB of raster, both allocated on entry (PSRAM on
  // these boards) and freed on exit.
  std::unique_ptr<ink::Page> page;
  std::unique_ptr<uint8_t[]> raster;

  int current = 1;
  int pageCount = 1;
  bool ruled = true;
  ink::Tool tool = ink::Tool::Pen;
  bool drawing = false;
  bool dirty = false;
  bool full = false;
  uint32_t lastPaintMs = 0;
  uint32_t lastStrokeEndMs = 0;
  // CLEAR asks twice; this is when it asked.
  uint32_t clearArmedMs = 0;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
