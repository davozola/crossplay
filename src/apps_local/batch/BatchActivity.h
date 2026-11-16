#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "BatchCore.h"

// THE BATCH: DeepLearning.AI's weekly newsletter, read on the device. The
// archive back to the first issue, each issue cut into its letter and its
// stories with a reading time on each, favourites, and everything opened once
// kept on the card for reading with the radio off.
class BatchActivity final : public Activity {
 public:
  BatchActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("Batch", renderer, mappedInput) {}
  ~BatchActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Archive, Issue, Read, Status };
  enum class Job : uint8_t { None, Sync, Fetch };
  // Joined Wi-Fi -> Ready -> the status frame is painted -> Running.
  enum class Step : uint8_t { Idle, Joining, Ready, Running };

  void loadIndex();
  void saveIndex();
  void loadShelf();
  void saveShelf();
  bool cached(int number) const;
  bool loadIssue(int number);
  void openIssue(int number);
  void openSection(int section, bool atEnd);
  void turn(int by);
  void start(Job job);
  void runSync();
  void runFetch();
  void showStatus(const char* headline, const char* detail, bool dismissable);
  void route(const freeink::ui::ActionEvent& event);
  int listed(int row) const;  // the index in issues_ of an archive row
  int listedCount() const;
  int rowsPerPage() const;
  void buildArchive(toybox::Screen& screen);
  void buildIssue(toybox::Screen& screen);
  void buildRead(toybox::Screen& screen);
  void buildStatus(toybox::Screen& screen);
  void layoutSection();
  int linesPerPage() const;
  int pageCount() const;

  View view_ = View::Archive;
  View statusBack_ = View::Archive;
  Step step_ = Step::Idle;
  Job job_ = Job::None;
  bool wifiActivated_ = false;

  std::vector<batch::Issue> issues_;
  batch::Shelf shelf_;
  int archivePage_ = 0;
  bool favouritesOnly_ = false;

  int number_ = 0;  // the issue open
  std::vector<batch::Section> sections_;
  int section_ = 0;
  std::vector<std::string> lines_;  // the open section, wrapped; an empty one is a paragraph gap
  int page_ = 0;

  const char* statusHeadline_ = "";
  char statusDetail_[120] = "";
  bool statusDismissable_ = false;

  int pressX_ = -1;
  uint8_t fastPaints_ = 0;
  bool cleanNext_ = true;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
