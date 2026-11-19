#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/SoloGame.h"
#include "../ui/ToyboxHowToState.h"
#include "../ui/ToyboxScreen.h"
#include "MancalaCore.h"

// MANCALA: Kalah against the machine or a friend across the device. The board
// lies on its side as it does on a table: your six pits along the foot, your
// store at the right-hand end. A swipe up or down stands it upright instead.
class MancalaActivity final : public Activity {
 public:
  MancalaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Mancala", renderer, mappedInput) {}
  ~MancalaActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Play };

  void load();
  void save();
  void newGame();
  void move(int slot);
  void route(const freeink::ui::ActionEvent& event);
  void buildMenu(toybox::Screen& screen);
  void buildBoard(toybox::Screen& screen);
  void buildBoardWide(toybox::Screen& screen);
  void buildVerdict(toybox::Screen& screen, const char* status, int top, int bottom);
  const char* statusText() const;
  void buildMenuWide(toybox::Screen& screen);
  void loadView();
  void saveView();
  bool machineToMove() const;
  void takeBack();
  void showHint();
  void recordIfOver();

  View view_ = View::Menu;
  mancala::Config config_;
  mancala::Board board_;
  bool inProgress_ = false;  // a game that can be continued
  int8_t played_ = -1;       // the pit the last move emptied
  int8_t last_ = -1;         // where its last seed fell
  uint8_t captured_ = 0;
  bool movedAgain_ = false;
  uint32_t rng_ = 1;
  uint32_t machineAtMs_ = 0;  // when the machine moves; 0 while it is not waiting to
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;
  bool wide_ = true;  // on its side, two rows and a store at each end; false: upright

  // The positions it was the player's turn in, for taking a move back (this
  // visit only); the pit a hint points at; and the record against each level.
  sologame::History<mancala::Board, 32> history_;
  int8_t hint_ = -1;
  bool recorded_ = false;
  sologame::LevelStats stats_;

  toyboxhowto::State howTo_;
  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
