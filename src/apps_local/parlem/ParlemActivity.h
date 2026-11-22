#pragma once

// PARLEM: a useful Catalan phrase a day for somebody starting out, on its
// side and set large, with its Spanish and a tip. A tap turns to the next
// one, a swipe sideways goes either way, and a swipe up or down changes how
// the page is set (dita::Style), which is remembered.

#include <memory>

#include "../../activities/Activity.h"
#include "ParlemCore.h"

class ParlemActivity final : public Activity {
 public:
  ParlemActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Parlem", renderer, mappedInput) {}
  ~ParlemActivity() override = default;

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

  // The cuts one page is set in, and its text broken into lines.
  struct Set;
  bool set(int width, int room, Set& out) const;
};
