#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "TodayStore.h"

// HOY: one page to leave standing. The time on a black band, today's weather
// and the day ahead as a curve, the three markets, the sea and a line of what
// happened on this day, all from what the other apps keep on the card.
class TodayActivity final : public Activity {
 public:
  TodayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Today", renderer, mappedInput) {}
  ~TodayActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  // Joined Wi-Fi -> Ready -> the status frame is painted -> Running.
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void startRefresh();
  void buildPage(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);

  // ~20 KB of forecasts and quotes: on the heap, allocated once on entry.
  std::unique_ptr<todaystore::Snapshot> data_;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;
  int shownMinute_ = -1;
  const char* statusHeadline_ = nullptr;  // non-null while a status page is up
  char report_[192] = "";
  bool statusDismissable_ = false;

  // What happened on a day like this, from what EFEMERIDES keeps on the card.
  void loadEvent();
  char eventText_[220] = "";
  int eventYear_ = 0;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
