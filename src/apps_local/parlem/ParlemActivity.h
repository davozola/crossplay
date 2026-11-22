#pragma once

// PARLEM: a useful Catalan phrase a day for somebody starting out, on its
// side and set large, with its Spanish. A tap turns to the next one, a swipe
// sideways goes either way, and a swipe up or down changes how the page is
// set (parlem::Style), which is remembered. Two small buttons along the
// bottom: SECCIONES keeps the phrases to one topic, TIP explains the one on
// the page.

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
  enum class View : uint8_t { Page, Tip, Menu };

  void loadState();
  void saveState();
  void turn(int by);
  void restyle(int by);
  void show(View view);
  void drawEditorial();
  void drawCartell();
  void drawDiccionari();
  void drawButtons(bool ink);
  void drawTip();
  void drawMenu();
  // The section under a tap on the menu, or -1.
  int sectionAt(int x, int y) const;

  // The cuts one page is set in, and its text broken into lines.
  struct Set;
  bool set(int width, int room, Set& out) const;

  parlem::State state_;
  View view_ = View::Page;
  bool cleanNext_ = true;
  int pressX_ = -1;
};
