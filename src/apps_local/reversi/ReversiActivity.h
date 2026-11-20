#pragma once

// REVERSI: trap the other colour between two of yours, against the device or a
// second player on the same panel.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/SoloGame.h"
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
  // A game kept on the card so it can be left and come back to.
  void saveGame();
  void continueGame();
  void takeBack();
  void showHint();
  void recordIfOver();
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

  // The positions it was the player's turn in, for taking a move back; whether
  // the card holds a game to continue; the square a hint points at; and the
  // record against each level of the device.
  sologame::History<reversi::Board, 32> history_;
  bool hasSaved_ = false;
  bool recorded_ = false;
  int hint_ = -1;
  sologame::LevelStats stats_;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
