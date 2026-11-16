#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "TodayStore.h"

// HOY: one page to leave standing. The time, today's weather and the next
// three days, the three markets and the sea, all from what the other apps
// keep on the card, with one button that fetches the lot again.
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

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
