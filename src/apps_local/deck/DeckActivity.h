#pragma once

// DECK: fifteen shortcuts for one program at a time, sent over Bluetooth as a
// keyboard, so the Sticky sits beside the computer as a small macro pad. The
// first visit asks whether the computer is a Mac or a Windows machine; the
// templates are written for both and the answer picks the chords.

#include <memory>

#include "../../activities/Activity.h"
#include "DeckCore.h"

class DeckActivity final : public Activity {
 public:
  DeckActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Deck", renderer, mappedInput) {}
  ~DeckActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // It sits on the desk between presses; sleeping would drop the computer.
  bool preventAutoSleep() override { return true; }

 private:
  enum class View : uint8_t { Ask, Keys };

  bool loadState();  // true when the computer has been named before
  void saveState();
  void turn(int by);
  void press(int button);
  void drawAsk();
  void drawKeys();
  int buttonAt(int x, int y) const;

  View view_ = View::Ask;
  deck::Os os_ = deck::Os::Mac;
  int template_ = 0;
  int lit_ = -1;             // the button drawn pressed, -1 for none
  uint32_t litUntilMs_ = 0;  // when it goes back to rest
  const char* note_ = nullptr;
  uint8_t linkShown_ = 0;
  bool cleanNext_ = true;
  bool radioUsed_ = false;
  int pressX_ = -1;
  int osLineY_ = 0;  // where the column names the computer, for a tap to change it
};
