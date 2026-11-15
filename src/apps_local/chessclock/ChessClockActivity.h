#pragma once

// CHESS CLOCK: two clocks with increment, on a panel that lies flat between
// the players. The thin layer: input, refresh policy, alarm, storage. The
// clock itself is ClockCore and is host-tested.
//
// A press is taken on the touch DOWN edge, not the release: on a clock the
// moment you hit it is the moment your time stops, and the release is up to a
// tenth of a second later.
//
// The panel repaints only when what it shows changes: once a minute per side
// with plenty of time, once a second in the last minute (or always, if the
// player asked for seconds). A pause gets a full refresh, which clears the
// ghosting a long run of partial ones leaves behind without blinking in the
// middle of play.

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "ClockCore.h"

class ChessClockActivity final : public Activity {
 public:
  ChessClockActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ChessClock", renderer, mappedInput) {}
  ~ChessClockActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return clock.state() == chessclock::State::Running; }

 private:
  enum class Screen : uint8_t { Menu, Face };

  void goTo(Screen next);
  void newGame();
  void pauseIfRunning();
  void ring();
  bool shownChanged();
  void loadState();
  void saveState();

  Screen screen = Screen::Menu;
  chessclock::Clock clock;
  bool alwaysSeconds = false;
  chessclock::Shown lastShown[2];
  bool fullOnNextPaint = false;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
