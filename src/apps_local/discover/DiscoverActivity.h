#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "DiscoverCore.h"

// DISCOVER: what is new and climbing on GitHub, by the kind of thing it is,
// the language it is in, how many stars it has and how old it is. A page for
// each repository, its README to read, and a list of the ones kept.
class DiscoverActivity final : public Activity {
 public:
  DiscoverActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Discover", renderer, mappedInput) {}
  ~DiscoverActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { List, Picker, Detail, Readme, Status };
  enum class Job : uint8_t { None, Search, Readme };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };
  enum class Choice : uint8_t { Topic, Language, Stars, Age };

  void loadState();
  void saveFilter();
  void saveList();
  void saveKept();
  void start(Job job);
  void runSearch();
  void runReadme();
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void route(const freeink::ui::ActionEvent& event);
  const discover::Repo* shown(int index) const;
  int shownCount() const;
  int linesPerPage() const;
  int readmePages() const;
  void buildList(toybox::Screen& screen);
  void buildPicker(toybox::Screen& screen);
  void buildDetail(toybox::Screen& screen);
  void buildReadme(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);

  View view_ = View::List;
  View statusBack_ = View::List;
  Step step_ = Step::Idle;
  Job job_ = Job::None;
  bool wifiActivated_ = false;

  discover::Filter filter_;
  Choice choice_ = Choice::Topic;
  std::unique_ptr<discover::Repo[]> found_;
  int foundCount_ = 0;
  int32_t total_ = 0;
  char foundOn_[12] = "";  // the day of the list on the card
  std::unique_ptr<discover::Repo[]> kept_;
  int keptCount_ = 0;
  bool showingKept_ = false;
  int page_ = 0;
  discover::Repo open_;

  std::vector<std::string> lines_;  // the README, wrapped
  int readmePage_ = 0;

  const char* statusHeadline_ = "";
  char statusDetail_[140] = "";
  bool statusDismissable_ = false;

  int pressX_ = -1;
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;
  // A fetch nobody asked for, behind what is already on the page: 1 the mark
  // is being painted, 2 the fetch runs. Its failures are kept to itself.
  uint8_t quietStage_ = 0;
  void quietFetch();

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
