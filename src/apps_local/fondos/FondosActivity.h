#pragma once

#include <HalStorage.h>

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "FondosCore.h"

// FONDOS: one picture a page, a button to keep it as the sleep screen, and a
// switch to have the sleep screen change by itself. Nothing else.
class FondosActivity final : public Activity {
 public:
  FondosActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Fondos", renderer, mappedInput) {}
  ~FondosActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  bool openPack();
  bool readPack(size_t at, uint8_t* out, size_t bytes);
  void readText(int index);
  bool writePicture(int index, const char* path);
  void loadState();
  void saveState();
  void useThisOne();
  void toggleEvery();
  void clearRotation();
  void useCustomSleep();
  void say(const char* message);

  fondos::Header header_;
  // The pack in the firmware, or null when it is read from the card.
  const uint8_t* embedded_ = nullptr;
  int index_ = 0;
  fondos::Mode mode_ = fondos::Mode::Off;
  int chosen_ = 0;
  char title_[fondos::kTitleBytes * 2 + 1] = "";
  char artist_[fondos::kArtistBytes * 2 + 1] = "";
  // The page's picture is on the card as this index, ready to draw; -1 for none.
  int shownOnCard_ = -1;
  const char* note_ = "";
  bool busy_ = false;
  uint8_t paints_ = 0;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
