#pragma once

// HANDWRITING on the device: a notebook of pages you write on with a finger.
//
// Ink appears as fast as the panel will take it. A refresh is a waveform a few
// tenths of a second long that nothing here can shorten, so the app's job is to
// add no wait of its own: every point is recorded as it is sampled, only the
// new piece of a stroke is drawn into the page's bitmap, and the next repaint
// is asked for the moment the last one comes back.
//
// Pages live on the card in /handwriting: page-NN.ink holds the strokes (what
// the app reads back) and page-NN.bmp the picture, which opens on a computer
// and can be copied to /wallpapers to become a sleep screen. They are saved a
// few seconds after the last stroke, on leaving, and at once with SAVE.

#include <atomic>
#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "InkCore.h"

class HandwritingActivity final : public Activity {
 public:
  HandwritingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Handwriting", renderer, mappedInput) {}
  ~HandwritingActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return drawing || dirty; }

 private:
  enum class View : uint8_t { Page, Notes };

  void repaint();
  void loadIndex();
  void saveIndex();
  bool readPage(int number);
  void loadPage(int number);
  void savePage();
  void goToPage(int number);
  void openNotes();
  void routePage(const freeink::ui::ActionEvent& event, uint32_t now);
  void routeNotes(const freeink::ui::ActionEvent& event);
  void renderPage(toybox::Screen& screen);
  void renderNotes(toybox::Screen& screen);

  // ~48KB of strokes and ~40KB of raster, both allocated on entry (PSRAM on
  // these boards) and freed on exit. `thumb` is one notes-grid thumbnail.
  std::unique_ptr<ink::Page> page;
  std::unique_ptr<uint8_t[]> raster;
  std::unique_ptr<uint8_t[]> thumb;

  View view = View::Page;
  int current = 1;
  int pageCount = 1;
  int notesFirst = 1;  // the page in the grid's first tile
  bool ruled = true;
  ink::Tool tool = ink::Tool::Pen;
  ink::Tool nib = ink::Tool::Pen;  // what the nib button goes back to
  bool nibOpen = false;
  bool drawing = false;
  bool dirty = false;
  bool full = false;
  bool saveWanted = false;
  // How many of the page's points the raster already holds; -1 when it has to
  // be drawn again from nothing (a page loaded, a stroke undone).
  int rastered = -1;
  // True from asking for a repaint until the panel has shown it.
  std::atomic<bool> painting{false};
  int paintedCount = 0;

  // Fast ink, experimental: while a stroke is being drawn the panel is driven
  // with a short waveform of the app's own that only inks new pixels, instead
  // of the factory one. 0 is off; 1 to 3 are shorter and shorter drives. The
  // stroke is redrawn with the factory waveform when the finger lifts.
  int inkSpeed = 0;
  bool inkWaveformOn = false;
  uint32_t lastInkMs = 0;  // how long the last stroke refresh took, for tuning
  void loadInkSpeed();
  void saveInkSpeed();
  void endInkWaveform();
  // The framebuffer holds the page as last painted, so a stroke in progress
  // can add its new ink to it instead of rebuilding the screen.
  bool pageOnPanel = false;
  bool renderStroke();
  uint32_t lastStrokeEndMs = 0;
  uint32_t savedShownMs = 0;
  // CLEAR asks twice; this is when it asked.
  uint32_t clearArmedMs = 0;

  toybox::Interactions interactions;
  bool interactionsReady = false;
};
