#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "GuideCore.h"

// FM-1 GUIAS: guides to the FM-1's community firmwares, in chapters, with the
// panel and the keys drawn where the text points at them. To read beside the
// synth: every page turn is one tap.
class FmGuideActivity final : public Activity {
 public:
  FmGuideActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("FmGuide", renderer, mappedInput) {}
  ~FmGuideActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Home, Chapters, Read };
  static constexpr int kMaxPages = 48;

  void load();
  void save();
  void openChapter(int chapter, int page);
  void turn(int direction);
  void paginate();
  int bodyTop() const;
  int bodyBottom() const;
  void buildHome(toybox::Screen& screen);
  void buildChapters(toybox::Screen& screen);
  void drawPage(toybox::Screen& screen);

  View view_ = View::Home;
  int guide_ = 0;
  int chapter_ = 0;
  int page_ = 0;
  // The first block of each page of the open chapter; one past the last page
  // holds the chapter's block count.
  uint8_t pageStart_[kMaxPages + 1] = {};
  int pageCount_ = 0;
  bool paged_ = false;

  fmguide::Place places_[fmguide::kGuides];
  int last_ = -1;  // the guide read most recently
  int pressX_ = -1;
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
