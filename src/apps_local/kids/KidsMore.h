#pragma once

// ILYA's four bigger games on the panel: MEMORIA, SUMES, LABERINT and PUNTS.
// Each is a page of choices and then the game; KidsActivity owns one at a time
// and hands it fingers and time.

#include <GfxRenderer.h>

#include <memory>

#include "../ui/ToyboxScreen.h"
#include "KidsCore.h"
#include "KidsMoreCore.h"

namespace kidsmoreui {

namespace fui = freeink::ui;

enum class Which : uint8_t { Memory, Sums, Maze, Dots };
constexpr int kGames = 4;
const char* nameOf(Which which);
// The picture that stands for it on ILYA's menu.
int pictureOf(Which which);

// Above KidsScreens' and MoleScreens' actions, which share the frame.
enum : fui::ActionId {
  ActionOption = 40,  // + the option's index: step it to its next value
  ActionPlay = 60,
};

// What a finger or a moment did, for the activity to act on.
enum class Did : uint8_t {
  Nothing,
  Paint,  // something changed
  Clean,  // a different page: wipe the panel's ghosts
  Cheer,  // paint, then the happy noise
  Nudge,  // nothing to paint: a wrong touch, felt and not shown
};

class Mini {
 public:
  virtual ~Mini() = default;

  // Reads the saved choices and shows the page of them.
  void open(kids::Rng& rng, bool touch);
  bool playing() const { return playing_; }
  // Back from the game goes to the choices; false when already there.
  bool back();
  Did action(fui::ActionId action);
  Did tap(GfxRenderer& renderer, int x, int y, uint32_t now);
  Did tick(uint32_t now);
  void draw(toybox::Screen& screen, GfxRenderer& renderer);

 protected:
  virtual const char* title() const = 0;
  virtual const char* file() const = 0;
  virtual const kidsmore::Option* options() const = 0;
  virtual int optionCount() const = 0;
  virtual int face() const = 0;  // the picture on the page of choices
  virtual void start(uint32_t now) = 0;
  virtual Did onTap(GfxRenderer& renderer, int x, int y, uint32_t now) = 0;
  virtual Did onTick(uint32_t /*now*/) { return Did::Nothing; }
  virtual void drawPlay(GfxRenderer& renderer) = 0;

  int bottom(const GfxRenderer& renderer) const;

  uint8_t values_[kidsmore::kMaxOptions] = {};
  kids::Rng* rng_ = nullptr;
  bool touch_ = true;
  int stars_ = 0;

 private:
  void load();
  void save();
  bool playing_ = false;
};

std::unique_ptr<Mini> make(Which which);

}  // namespace kidsmoreui
