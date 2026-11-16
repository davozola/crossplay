#pragma once

#include <atomic>
#include <memory>

#include "../../activities/Activity.h"
#include "PaintCore.h"

// PINTA: finger painting for a small child. A sheet, two brushes, a rubber,
// pictures to stamp and a button for a clean sheet. Five big buttons and no
// words to read; the sheet is kept on the card between visits.
class PaintActivity final : public Activity {
 public:
  PaintActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Paint", renderer, mappedInput) {}
  ~PaintActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class Tool : uint8_t { Thin, Fat, Stamp, Rubber };

  void repaint();
  void loadSheet();
  void saveSheet();
  int sheetHeight() const;
  int toolbarTop() const;
  void drawButton(int index, bool selected) const;

  std::unique_ptr<uint8_t[]> bits_;
  std::unique_ptr<paint::Sheet> sheet_;
  Tool tool_ = Tool::Fat;
  int picture_ = 0;
  bool drawing_ = false;
  bool moved_ = false;  // the sheet has changed since the last paint
  bool dirty_ = false;  // and since the last save
  int lastX_ = 0;
  int lastY_ = 0;
  // Set when a repaint is asked for, cleared when the panel has taken it: new
  // paint is shown the moment the last frame has come back, with no timer.
  std::atomic<bool> painting_{false};
};
