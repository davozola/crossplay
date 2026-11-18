#pragma once

// NEWS: five sections of headlines, read straight off each outlet's feed, and
// the article behind any of them.
//
// Nothing here is ours on the far side: the device asks each outlet for its
// own feed, and a section is kept on the card so the last headlines open with
// no network. The one third party is the text extractor an article goes
// through, the same one HACKER NEWS uses and for the same reason.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxScreen.h"
#include "../ui/ToyboxWrappedText.h"
#include "NewsCore.h"
#include "NewsScreens.h"

class NewsActivity final : public Activity {
 public:
  NewsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput) : Activity("News", renderer, mappedInput) {}
  ~NewsActivity() override = default;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;
  void onExit() override;
  void render(RenderLock&&) override;

 private:
  enum class Phase : uint8_t { Busy, List, Reading, Notice };
  enum class Pending : uint8_t { None, Section, Article };

  void ensureConnected(Pending what, const char* busyMessage);
  void request(Pending what, const char* busyMessage);
  bool fetchSection();
  bool fetchArticle();
  // An article already on the card, opened with no network. False if it is not there.
  bool openKept(int index);
  void openItem(int index);
  // Favourites: a list of their own, with each article's text kept beside it
  // on the card for as long as it stays one.
  bool showingFavourites() const { return section_ == news::kSectionCount; }
  bool isFavourite(const std::string& url) const;
  void toggleFavourite();
  void loadFavourites();
  void saveFavourites();
  const char* sourceOf(const news::Item& item) const;
  void loadSection(int section);
  void saveSection();
  void buildRows();
  void pageList(int delta);
  void turnPage(int delta);
  void showNotice(const char* headline, const char* message);
  bool hasLead() const;

  int section_ = 0;
  std::vector<news::Item> items_;
  uint32_t fetched_ = 0;
  int failedFeeds_ = 0;
  bool fetchFailed_ = false;

  // The strings the list draws, owned here because a ListItem holds pointers.
  std::vector<std::string> labels_;
  std::vector<std::string> values_;
  std::vector<freeink::ui::ListItem> rows_;
  bool rowsBuilt_ = false;
  std::string leadKicker_;
  std::string leadTitle_;
  std::string leadSummary_;
  int topIndex_ = 0;
  int visibleRows_ = 0;
  int selected_ = 0;

  // In a favourite, `summary` holds the outlet's name: its section is not kept.
  std::vector<news::Item> favourites_;
  news::Item reading_;
  std::string readingSource_;

  std::string document_;
  std::string readerTitle_;
  toybox::WrappedText wrap_;
  uint32_t topLine_ = 0;
  uint32_t lineCount_ = 0;
  uint16_t visibleLines_ = 0;
  static constexpr int kPageLabelCap = 2 * toybox::kULongChars + toybox::literalChars("/") + 1;
  char pageLabel_[kPageLabelCap] = "";

  std::string noticeHeadline_;
  std::string noticeMessage_;

  bool backPressSeen_ = false;
  Phase phase_ = Phase::List;
  Pending pending_ = Pending::None;
  const char* busyMessage_ = "";
  Pending afterConnect_ = Pending::None;
  const char* afterConnectMessage_ = "";

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
