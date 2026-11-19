#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/SoloGame.h"
#include "../ui/ToyboxHowToState.h"
#include "../ui/ToyboxScreen.h"
#include "HiveCore.h"
#include "HiveEngine.h"
#include "HiveScreens.h"

// HIVE: against the device at three levels, or two people passing it across.
// Tap a piece in the hand or on the table, then one of the cells it lights.
class HiveActivity final : public Activity {
 public:
  HiveActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Hive", renderer, mappedInput) {}
  ~HiveActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Play };
  toyboxhowto::State howTo_;
  static constexpr int kTwoPlayers = 3;

  void startGame(int mode);
  void select(int piece);
  void play(const hive::Move& move);
  // Passes for a side with no move, and says whose turn it is.
  void settle();
  void takeBack();
  bool deviceToMove() const { return mode_ != kTwoPlayers && game_.turn == 1 && game_.winner() < 0; }
  void boardTap(int x, int y);
  void loadState();
  void saveState();

  View view_ = View::Menu;
  int mode_ = 1;
  bool inProgress_ = false;
  hive::Game game_;
  hive::Rng rng_;
  std::unique_ptr<hive::SearchBuffers> buffers_;

  int selected_ = -1;
  hive::Move targets_[96];
  int targetCount_ = 0;
  int lastPiece_ = -1;
  char status_[64] = "";
  // Set when the device is to move: the next pass paints, the one after thinks.
  bool thinking_ = false;
  // Positions before each move made by hand, for taking one back (this visit
  // only), and the record against each level.
  sologame::History<hive::Game, 16> history_;
  sologame::LevelStats levelStats_;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
