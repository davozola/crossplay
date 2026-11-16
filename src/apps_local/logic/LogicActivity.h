#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "LogicCore.h"
#include "LogicScreens.h"

// Slitherlink, Shikaku, Akari and Hashi: four shelf entries, one activity. Each
// keeps which boards are solved and the board in progress on the card.
class LogicActivity final : public Activity {
 public:
  LogicActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, logic::Game game)
      : Activity(logic::gameName(game), renderer, mappedInput), game_(game) {}
  ~LogicActivity() override = default;

  static std::unique_ptr<Activity> createSlither(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createShikaku(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createAkari(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createHashi(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Picker, Board };

  void openBoard(int number, bool resume);
  void loadBoard();
  bool boardTap(int x, int y);
  bool isSolved() const;
  void savePath(char* out, size_t cap) const;
  void loadState();
  void saveState();
  void route(const freeink::ui::ActionEvent& event);

  const logic::Game game_;
  View view_ = View::Picker;
  int size_ = 0;
  int number_ = 0;
  // The board whose marks are on the card: where RESUME picks up.
  int savedSize_ = -1;
  int savedNumber_ = -1;
  uint32_t solved_[logic::kSizes] = {};
  bool solvedNow_ = false;
  bool touched_ = false;
  uint32_t resetArmedMs_ = 0;

  // All four are small; only the one for game_ is used.
  logic::Slither slither_;
  logic::Shikaku shikaku_;
  logic::Akari akari_;
  logic::Hashi hashi_;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
