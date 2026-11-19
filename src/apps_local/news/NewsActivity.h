#pragma once

// NEWS: the day's edition of Kagi News in four sections, read from the card.
//
// One thing touches the network: REFRESH, which fetches the whole edition and
// replaces the one on the card only if all of it arrived. Everything else,
// reading included, works with the radio off.
//
// The stories are Kagi's (news.kagi.com, CC BY-NC 4.0): syntheses of what many
// outlets reported, each ending with its sources and that attribution.

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

  void ensureConnected();
  void fetchEdition();
  void loadSection(int section);
  void buildRows();
  void pageList(int delta);
  void turnPage(int delta);
  void showNotice(const char* headline, const std::string& message);
  bool hasLead() const;
  void openItem(int index);

  // Favourites: a list of their own, with each story's text kept beside it on
  // the card for as long as it stays one.
  bool showingFavourites() const { return section_ == news::kSectionCount; }
  bool isFavourite(const std::string& id) const;
  void toggleFavourite();
  void loadFavourites();
  void saveFavourites();

  int section_ = 0;
  std::vector<news::Item> items_;
  uint32_t edition_ = 0;  // when Kagi made it, seconds since 1970

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

  std::vector<news::Item> favourites_;
  news::Item reading_;

  std::string document_;
  std::string readerTitle_;
  toybox::WrappedText wrap_;
  uint32_t topLine_ = 0;
  uint32_t lineCount_ = 0;
  uint16_t visibleLines_ = 0;
  // Which of the three reading cuts a story is set in: 0 small, 1 the step
  // under the reading cut, 2 the reading cut. Kept on the card.
  uint8_t textSize_ = 1;
  // After the size changes, where in the story to land once it is re-wrapped,
  // in thousandths; -1 when there is nothing to restore.
  int restoreAt_ = -1;
  static constexpr int kPageLabelCap = 2 * toybox::kULongChars + toybox::literalChars("/") + 1;
  char pageLabel_[kPageLabelCap] = "";

  std::string noticeHeadline_;
  std::string noticeMessage_;

  bool backPressSeen_ = false;
  Phase phase_ = Phase::List;
  bool fetchPending_ = false;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
