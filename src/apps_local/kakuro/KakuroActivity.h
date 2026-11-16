#pragma once

// Kakuro on the device. The thin layer: input, storage, the shelf. The bank,
// the rules and the checks are KakuroCore, host-tested.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "KakuroCore.h"

class KakuroActivity final : public Activity {
 public:
  KakuroActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Kakuro", renderer, mappedInput) {}
  ~KakuroActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { Menu, HowTo, Board };

  // What is kept per board size: which puzzle is open, its progress, and which
  // puzzles are done. ~400 bytes each, so the three live on the heap with the
  // activity rather than on any stack.
  struct Slot {
    uint16_t index = 0;
    bool started = false;
    kakuro::Progress progress;
    uint64_t solved = 0;  // bit i: puzzle i solved
  };

  void goTo(Screen next);
  void open(kakuro::Size size);
  void openNext();
  void pressKey(int key);
  void loadState();
  void saveState();
  int solvedCount(const Slot& slot) const;

  Screen screen = Screen::Menu;
  kakuro::Size current = kakuro::Size::Small;
  bool anyStarted = false;
  Slot slots[kakuro::kSizeCount];
  kakuro::Layout layout;
  int selected = -1;
  bool notes = false;
  int howToPage = 0;
  int unsavedEdits = 0;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
