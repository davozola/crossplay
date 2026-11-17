#pragma once

// REVERSI: trap the other colour between two of yours, against the device or a
// second player on the same panel.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "ReversiCore.h"

class ReversiActivity final : public Activity {
 public:
  ReversiActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Reversi", renderer, mappedInput) {}
  ~ReversiActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Setup, Play };
  static constexpr int kOptions = 4;

  void loadOptions();
  void saveOptions();
  void newGame();
  bool deviceToMove() const;
  void deviceMoves();
  void buildSetup(toybox::Screen& screen);
  void buildPlay(toybox::Screen& screen);
  int pageBottom() const;
  void boardRect(int& x, int& y, int& cell) const;

  reversi::Board board_;
  reversi::Rng rng_;
  uint8_t options_[kOptions] = {};
  View view_ = View::Setup;
  const char* note_ = "";  // what just happened, under the board
  uint8_t paints_ = 0;
  bool clean_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
