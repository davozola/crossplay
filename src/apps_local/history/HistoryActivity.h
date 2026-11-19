#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "HistoryCore.h"

// EFEMERIDES: what happened on this day. One event each of science, history
// and culture to lead the page, the whole day's list behind them, and every
// day once fetched kept on the card.
class HistoryActivity final : public Activity {
 public:
  HistoryActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("History", renderer, mappedInput) {}
  ~HistoryActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Day, List, Event, Status };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  bool loadDay();
  void startFetch();
  void runFetch();
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void route(const freeink::ui::ActionEvent& event);
  int listed(int row) const;  // the index in events_ of a list row
  int listedCount() const;
  void buildDay(toybox::Screen& screen);
  void buildList(toybox::Screen& screen);
  void buildEvent(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);

  View view_ = View::Day;
  View statusBack_ = View::Day;
  Step step_ = Step::Idle;
  bool wifiActivated_ = false;

  int month_ = 1;
  int day_ = 1;
  int thisYear_ = 0;  // 0 when the clock is not set
  std::vector<history::Event> events_;
  int filter_ = -1;  // a Kind, or -1 for all of them
  int page_ = 0;
  int open_ = 0;

  const char* statusHeadline_ = "";
  char statusDetail_[140] = "";
  bool statusDismissable_ = false;

  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;
  // A fetch nobody asked for, behind what is already on the page: 1 the mark
  // is being painted, 2 the fetch runs. Its failures are kept to itself.
  uint8_t quietStage_ = 0;
  void quietFetch();

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
