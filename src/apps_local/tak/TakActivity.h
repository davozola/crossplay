#pragma once

// Tak on the device. The thin layer: renderer, input, shelf, storage. The
// rules are TakCore, the opponent TakEngine, the hand TakFlow, the screens
// TakScreens, and all four are host-tested.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/SoloGame.h"
#include "../ui/ToyboxScreen.h"
#include "TakCore.h"
#include "TakEngine.h"
#include "TakFlow.h"
#include "TakScreens.h"

class TakActivity final : public Activity {
 public:
  TakActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Tak", renderer, mappedInput) {}
  ~TakActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // The search is a couple of seconds at most; it must not be cut short by
  // the device dozing off between the human's move and the reply.
  bool preventAutoSleep() override { return thinking; }

 private:
  void goTo(tak::Screen next);
  void newGame(bool computer);
  void playMove(const tak::Move& move);
  void computerTurn();
  bool computerToMove() const;
  void finishIfOver();
  void takeBack();
  void loadState();
  void saveState();
  void handleBoardTap(int x, int y);

  tak::Screen screen = tak::Screen::Menu;
  tak::Game game{};
  tak::Hand hand{};
  tak::Stone tray = tak::Stone::Flat;
  bool vsComputer = true;
  bool inProgress = false;
  uint8_t human = tak::kWhite;
  int menuSize = 5;
  tak::Level level = tak::Level::Medium;
  uint64_t lastMove = 0;
  char lastPtn[12] = {};
  int howToPage = 0;
  int played = 0;
  int won = 0;
  bool resultRecorded = false;
  // Set when the computer is to move; the search runs on the NEXT pass, after
  // the human's move and "THINKING" have been painted.
  bool thinking = false;
  bool thinkingShown = false;

  // ~165KB: five move lists and the root noise. Allocated the first time the
  // computer moves and kept until the app closes, so a game never asks the
  // heap for it twice.
  // Positions before each move of the player's, for taking one back (this
  // visit only), and the record against each level.
  sologame::History<tak::Game, 12> history;
  sologame::LevelStats levelStats;

  std::unique_ptr<tak::SearchBuffers> buffers;

  takui::BoardModel boardModel;
  takui::ResultModel resultModel;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
