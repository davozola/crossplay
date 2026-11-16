#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "KidsCore.h"
#include "KidsScreens.h"
#include "MoleCore.h"

// ILYA: six games for a three-year-old. Big pictures, one tap at a time, and
// nothing that can be lost: a wrong tap greys the tile and asks again.
class KidsActivity final : public Activity {
 public:
  KidsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Ilya", renderer, mappedInput) {}
  ~KidsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Card, Round, MoleSetup, MolePlay };

  void open(kids::Game game);
  void deal();
  void route(const freeink::ui::ActionEvent& event);
  void cheer();

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
