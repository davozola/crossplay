#pragma once

// DOTS AND BOXES: draw the fourth side of a square to own it, against the device or
// a second player on the same panel.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "BoxesCore.h"

class BoxesActivity final : public Activity {
 public:
  BoxesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Boxes", renderer, mappedInput) {}
  ~BoxesActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Setup, Play };
  static constexpr int kOptions = 3;

  void loadOptions();
  void saveOptions();
  void newGame();
  bool deviceToMove() const;
  void deviceMoves();
  void buildSetup(toybox::Screen& screen);
  void buildPlay(toybox::Screen& screen);
  int pageBottom() const;
  void boardRect(int& x, int& y, int& cell) const;
  int lineAt(int tapX, int tapY) const;

  boxes::Board board_;
  boxes::Rng rng_;
  uint8_t options_[kOptions] = {};
  View view_ = View::Setup;
  const char* note_ = "";  // what just happened, under the board
  uint8_t paints_ = 0;
  bool clean_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
