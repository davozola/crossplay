#pragma once

// NEWS: the day's edition of Kagi News, in the sections the reader chose, read
// from the card.
//
// One thing touches the network: fetching the edition (and, the first time,
// the list of sections to choose from). It replaces the edition on the card
// only if it arrived. Everything else, reading included, works with the radio
// off.
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
  enum class Phase : uint8_t { Busy, Front, Menu, Chooser, Reading, Notice };
  enum class Pending : uint8_t { None, Edition, Categories };

  void ensureConnected(Pending what);
  void fetchEdition();
  bool fetchCategories(news::CategoriesReader& into, size_t& bytes, uint32_t& lowestHeap);
  void loadChosen();
  void saveChosen();
  void loadSection(int view);
  void pageFront(int delta);
  void turnPage(int delta);
  void stepSection(int delta);
  void showNotice(const char* headline, const std::string& message);
  void openItem(int index);
  void openChooser();
  void toggleCategory(int row);
  bool isChosen(const std::string& slug) const;

  // Favourites: a list of their own, with each story's text kept beside it on
  // the card for as long as it stays one.
  bool showingFavourites() const { return view_ < 0; }
  bool isFavourite(const std::string& id) const;
  void toggleFavourite();
  void loadFavourites();
  void saveFavourites();

  std::vector<news::Category> chosen_;      // the reader's sections, in order
  std::vector<news::Category> categories_;  // everything Kagi offers, for the chooser
  bool chosenChanged_ = false;
  int chooserPage_ = 0;

  int view_ = 0;  // index into chosen_, or -1 for the favourites
  std::vector<news::Item> items_;
  uint32_t edition_ = 0;  // when Kagi made it, seconds since 1970
  char editionText_[16] = "";

  // Where each page of the front starts; headlines are set whole, so a page
  // holds as many as fit and that is only known once they are measured.
  std::vector<int> pageStarts_;
  int frontPage_ = 0;

  std::vector<news::Item> favourites_;
  news::Item reading_;

  std::string title_;
  std::string body_;
  std::string readerSection_;
  toybox::WrappedText wrap_;
  news::Paging paging_;
  uint32_t page_ = 0;
  // Which of the three reading cuts a story is set in: 0 small, 1 the step
  // under the reading cut, 2 the reading cut. Kept on the card.
  uint8_t textSize_ = 1;
  // After the size changes, the line to land on once the story is re-wrapped,
  // in thousandths of the way through; -1 when there is nothing to restore.
  int restoreAt_ = -1;
  char pageLabel_[24] = "";

  std::string noticeHeadline_;
  std::string noticeMessage_;

  bool backPressSeen_ = false;
  Phase phase_ = Phase::Front;
  Phase noticeReturn_ = Phase::Front;
  Pending pending_ = Pending::None;
  Pending afterConnect_ = Pending::None;

  toybox::Interactions interactions_;
  bool interactionsReady_ = false;
};
