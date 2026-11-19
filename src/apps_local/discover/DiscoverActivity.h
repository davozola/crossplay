#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxScreen.h"
#include "DiscoverCore.h"

// DISCOVER: what is climbing on GitHub, by the kind of thing it is or by the
// reader's own words, leaving out what a page here has shown already; the
// repositories whose new versions are watched for; and the ones kept. A page
// for each repository, its README to read, and a code a phone opens it with.
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
  enum class View : uint8_t { List, Picker, Detail, Readme, Release, Qr, Status };
  enum class Tab : uint8_t { Discover = 0, Releases, Kept };
  enum class Job : uint8_t { None, Search, Readme, Check };
  enum class Checked : uint8_t { Answered, Failed, Limited };
  enum class Step : uint8_t { Idle, Joining, Ready, Running };
  enum class Choice : uint8_t { Topic, Language, Stars, Age };

  void loadState();
  void saveFilter();
  void saveList();
  void saveKept();
  void saveTerms();
  void saveSeen();
  void saveWatched();
  void saveTab();
  void start(Job job);
  void runSearch();
  void runReadme();
  bool checkStep();
  Checked checkOne(discover::Watched& watched);
  void finishCheck();
  void askWords(bool forWatch);
  void typed(const char* text, bool forWatch);
  void searchFor(const char* words);
  void watch(const char* fullName, bool checkNow);
  void openRelease(int index);
  void openNotes();
  void showQr(const char* title, const char* url, View back);
  void setReading(std::string& raw);
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void route(const freeink::ui::ActionEvent& event);
  const discover::Repo* shown(int index) const;
  int shownCount() const;
  int rows() const;
  int linesPerPage() const;
  int readmePages() const;
  void buildList(toybox::Screen& screen);
  void buildPicker(toybox::Screen& screen);
  void buildDetail(toybox::Screen& screen);
  void buildReadme(toybox::Screen& screen);
  freeink::ui::Rect buildRelease(toybox::Screen& screen);
  freeink::ui::Rect buildQr(toybox::Screen& screen);
  void buildFoundRows(toybox::Screen& screen, int y, int rowH);
  void buildWatchedRows(toybox::Screen& screen, int y, int rowH);
  void buildTopicPicker(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);

  View view_ = View::List;
  View statusBack_ = View::List;
  View readingBack_ = View::Detail;
  View qrBack_ = View::Detail;
  Tab tab_ = Tab::Discover;
  Step step_ = Step::Idle;
  Job job_ = Job::None;
  bool wifiActivated_ = false;
  // Held across a check of several repositories, so the network is joined once.
  std::optional<autorefresh::Radio> radio_;

  discover::Filter filter_;
  Choice choice_ = Choice::Topic;
  std::unique_ptr<discover::Repo[]> found_;
  int foundCount_ = 0;
  int32_t total_ = 0;
  char foundOn_[12] = "";  // the day of the list on the card
  std::unique_ptr<discover::Repo[]> kept_;
  int keptCount_ = 0;
  std::unique_ptr<discover::Seen> seen_;
  bool seenDirty_ = false;
  discover::Terms terms_;
  std::unique_ptr<discover::Watched[]> watched_;
  int watchedCount_ = 0;
  int openWatched_ = -1;
  char sinceTag_[40] = "";  // what the open release replaced, when it was news
  int checkAt_ = 0;
  int checkFailed_ = 0;
  bool checkNewOnly_ = false;
  bool checkLimited_ = false;
  int page_ = 0;
  discover::Repo open_;
  char qrTitle_[72] = "";
  char qrUrl_[160] = "";

  std::vector<std::string> lines_;  // the README or the release notes, wrapped
  int readmePage_ = 0;
  const char* readingTitle_ = "README";
  char readingName_[120] = "";

  const char* statusHeadline_ = "";
  char statusDetail_[140] = "";
  bool statusDismissable_ = false;

  int pressX_ = -1;
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
