#pragma once

// DITA: a Catalan saying a day, on its side, with what it means in Spanish
// and what it says word for word. A tap turns to the next one, a swipe
// sideways goes either way, and a swipe up or down changes how the page is
// set (dita::Style), which is remembered.

#include <memory>

#include "../../activities/Activity.h"
#include "DitaCore.h"

class DitaActivity final : public Activity {
 public:
  DitaActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Dita", renderer, mappedInput) {}
  ~DitaActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void loadState();
  void saveState();
  void turn(int by);
  void restyle(int by);
  void drawEditorial();
  void drawCartell();
  void drawDiccionari();

  dita::State state_;
  bool cleanNext_ = true;
  int pressX_ = -1;
};
