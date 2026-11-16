#pragma once

// Mastermind on the device, played alone against a hidden code. The thin
// layer: input, storage, the shelf. Rules and scoring are MastermindCore.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "MastermindCore.h"
#include "MastermindScreens.h"

class MastermindActivity final : public Activity {
 public:
  MastermindActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Mastermind", renderer, mappedInput) {}
  ~MastermindActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Screen : uint8_t { Menu, HowTo, Board };

  void goTo(Screen next);
  void newGame(mastermind::Mode mode);
  void finishIfOver();
  void loadState();
  void saveState();

  Screen screen = Screen::Menu;
  mastermind::Game game;
  bool inProgress = false;
  bool recorded = false;
  mastermindui::Stats stats[mastermind::kModeCount];
  int howToPage = 0;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
