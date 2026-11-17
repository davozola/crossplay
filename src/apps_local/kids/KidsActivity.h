#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "KidsCore.h"
#include "KidsMore.h"
#include "KidsScreens.h"
#include "MoleCore.h"

// ILYA: ten games for a small child. Big pictures, one tap at a time, and
// nothing that can be lost: a wrong tap greys the tile and asks again.
class KidsActivity final : public Activity {
 public:
  // `only` is the menu's number for one game (KidsCore's six, then KidsMore's
  // four) when the shelf opened that game by itself; kMenu for the page of all.
  KidsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const int8_t only = kMenu)
      : Activity("Ilya", renderer, mappedInput), only_(only) {}
  ~KidsActivity() override = default;

  static constexpr int8_t kMenu = -1;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createCards(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createFind(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createMatch(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createLetters(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createCount(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createMole(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createMemory(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createSums(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createMaze(GfxRenderer& renderer, MappedInputManager& mappedInput);
  static std::unique_ptr<Activity> createDots(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Card, Round, MoleSetup, MolePlay, Mini };

  void open(kids::Game game);
  // Opens game `number` of the menu; false when it could not be made.
  bool openNumber(int number);
  void deal();
  void route(const freeink::ui::ActionEvent& event);
  void cheer();
  void did(kidsmoreui::Did what);

  // MEMORIA, SUMES, LABERINT or PUNTS, while one is open.
  std::unique_ptr<kidsmoreui::Mini> mini_;

  // EL TALP.
  void loopMole();
  void loadMoleConfig();
  void saveMoleConfig();
  void bopSound();
  void partySound();
  mole::Config moleConfig_;
  mole::Game mole_;
  uint32_t partyUntilMs_ = 0;  // non-zero while the celebration is up
  bool cleanNext_ = false;     // the next paint wipes the panel's ghosts
  uint8_t fastPaints_ = 0;

  int8_t only_ = kMenu;
  View view_ = View::Menu;
  kids::Game game_ = kids::Game::Cards;
  kids::Rng rng_;
  kids::Round round_;
  int card_ = 0;
  int stars_ = 0;
  int lastTarget_ = -1;
  int lastCount_ = 0;
  // When a solved round moves on by itself; 0 while none is waiting.
  uint32_t advanceAtMs_ = 0;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
