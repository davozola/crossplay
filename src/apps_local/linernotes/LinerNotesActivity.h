#pragma once

// LINER NOTES: what to know about the records in your own library, read from
// a pack on the card. The pack is made off the device and is the owner's; the
// app ships with none and says how to add one.

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "LinerNotesCore.h"

class LinerNotesActivity final : public Activity {
 public:
  LinerNotesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("LinerNotes", renderer, mappedInput) {}
  ~LinerNotesActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { NoPack, Home, List, Letters, Album, Related };
  enum class Shelf : uint8_t { Essential, Hidden, All, Marked, Recent, Letter };

  bool spanish() const;
  void saveKept();
  void openList(Shelf shelf, int value = 0);
  void fillList();
  void openAlbum(int album);
  bool loadRecord(int album);
  void back();
  void act(int action, int value);
  void play();
  void startAlbum();
  int pageBottom() const;
  int listRows() const;
  int albumLines() const;
  void drawFit(int font, int x, int y, int width, const char* text, bool black = true) const;

  void buildNoPack(toybox::Screen& screen);
  void buildHome(toybox::Screen& screen);
  void buildList(toybox::Screen& screen);
  void buildLetters(toybox::Screen& screen);
  void buildAlbum(toybox::Screen& screen);
  void buildRelated(toybox::Screen& screen);

  linernotes::Index index_;
  linernotes::Kept kept_;
  linernotes::Document doc_;
  std::vector<uint16_t> list_;
  std::vector<uint8_t> trail_;  // views to go back through
  std::vector<uint16_t> albumTrail_;

  View view_ = View::NoPack;
  Shelf shelf_ = Shelf::All;
  int shelfValue_ = 0;  // a category (0 for all) or a letter
  int listTop_ = 0;
  int album_ = -1;
  int lineTop_ = 0;
  int today_ = 0;
  const char* note_ = "";
  uint8_t playStep_ = 0;  // 1 painted "asking", 2 ask now
  uint8_t paints_ = 0;
  bool clean_ = true;
  bool radioUsed_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
