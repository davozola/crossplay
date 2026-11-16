#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "CasaCore.h"

// CASA: the room the device is standing in. Temperature and humidity from the
// board's own sensor, and the last twenty-four hours of each as a line.
class CasaActivity final : public Activity {
 public:
  CasaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Casa", renderer, mappedInput) {}
  ~CasaActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void measure();
  void buildPage(toybox::Screen& screen);
  // One day of one quantity. `humidity` picks which.
  void plot(toybox::Screen& screen, int x, int y, int w, int h, bool humidity);

  // 2.3 KB of readings: on the heap, allocated once on entry.
  std::unique_ptr<casa::Log> log_;
  casa::Sample now_ = {};
  bool hasNow_ = false;
  bool sleepLogging_ = false;
  uint32_t lastMeasureMs_ = 0;
  int shownTenths_ = 0;
  int shownHum_ = 0;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
