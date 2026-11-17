#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "TodayStore.h"

// HOY: one page to leave standing. The time, today's weather, the three
// markets, the sea and a line of what happened on this day, all from what the
// other apps keep on the card, in five faces to swipe between. A tap on any
// part opens the app it comes from.
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
  // Minutes between fetches made with nobody asking, 0 for never; kept with the style.
  int autoMinutes_ = 0;
  uint32_t lastFetchMs_ = 0;
  void savePrefs();
  int shownMinute_ = -1;
  const char* statusHeadline_ = nullptr;  // non-null while a status page is up
  char report_[192] = "";
  bool statusDismissable_ = false;

  // What happened on a day like this, from what EFEMERIDES keeps on the card.
  void loadEvent();
  char eventText_[3][220] = {};
  int eventYear_[3] = {};
  int eventCount_ = 0;
  char playing_[160] = "";

  // Which of the five faces is up; a swipe changes it and it is kept.
  void setStyle(int style);
  int style_ = 0;
  int pressX_ = -1;
  bool cleanNext_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
