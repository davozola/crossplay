#pragma once

#include <HalStorage.h>

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "TacticsCore.h"

// TACTICS: chess puzzles from a pack on the card, picked to sit at your own
// rating, which moves with how you do. A puzzle a day, hints, and the numbers.
class TacticsActivity final : public Activity {
 public:
  TacticsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Tactics", renderer, mappedInput) {}
  ~TacticsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Home, Solve, Stats };

  void loadStats();
  void saveStats();
  bool openPack();
  bool readRecord(uint32_t index, uint8_t* record);
  bool pickNear(int rating, tactics::Puzzle& out);
  void startTraining();
  void startDaily();
  void settle(bool solved);
  void tapBoard(int square);
  void after(tactics::Outcome outcome);
  void route(const freeink::ui::ActionEvent& event);
  void buildHome(toybox::Screen& screen);
  void buildSolve(toybox::Screen& screen);
  void buildStats(toybox::Screen& screen);
  void drawBoard(int x, int y, int cell) const;
  int squareAt(int tapX, int tapY) const;
  int32_t today() const;

  View view_ = View::Home;
  tactics::Stats stats_;
  tactics::Focus focus_ = tactics::Focus::Everything;
  tactics::Header pack_;
  bool packOpen_ = false;
  HalFile file_;

  tactics::Run run_;
  bool daily_ = false;
  bool settled_ = false;   // this puzzle has been counted
  int16_t delta_ = 0;      // what it did to the rating
  int8_t selected_ = -1;   // the square of the piece picked up
  const char* said_ = "";  // the line under the board
  int boardX_ = 0;
  int boardY_ = 0;
  int cell_ = 0;

  uint32_t rng_ = 1;
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
