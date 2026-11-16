#pragma once

#include <memory>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "MarketsCore.h"
#include "MarketsScreens.h"

// MARKETS: Bitcoin, the S&P 500 and the Nasdaq on one screen that can stand on
// a desk. Bitcoin comes from Coinbase's daily candles and the indexes from CNBC, one small answer per
// instrument, and are kept on the card so the last ones read with no network.
// With auto-refresh on, the device stays awake and fetches again by itself.
class MarketsActivity final : public Activity {
 public:
  MarketsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Markets", renderer, mappedInput) {}
  ~MarketsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return autoMinutes_ > 0; }

 private:
  enum class View : uint8_t { Dashboard, Detail, AutoPicker, Status };
  // Joined Wi-Fi -> Ready -> the status frame is painted -> Running.
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void loadState();
  void saveState();
  bool loadCached(markets::Asset asset);
  int fetchAll();
  void startRefresh();
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void route(const freeink::ui::ActionEvent& event);

  View view_ = View::Dashboard;
  View statusBack_ = View::Dashboard;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;

  // ~1 KB each: with the activity on the heap, not on a stack.
  markets::Quote quotes_[markets::kAssets];
  markets::Asset open_ = markets::Asset::Bitcoin;
  marketsui::Span span_ = marketsui::Span::Month;
  int autoMinutes_ = 0;
  uint32_t lastFetchMs_ = 0;

  const char* statusHeadline_ = "";
  const char* statusDetail_ = "";
  bool statusDismissable_ = false;
  // What each source answered on the last round, for the page shown when none did.
  char report_[96] = "";

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
