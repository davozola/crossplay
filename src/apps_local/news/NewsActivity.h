#pragma once

// NEWS: the day's edition of Kagi News, in the sections the reader chose, read
// from the card.
//
// What is on the card is shown first, always. Fetching the edition happens
// behind it: by itself when the app opens and the last fetch is two hours old
// or more, and whenever the reader asks. The radio joins the saved network
// with no screen of its own, and the only sign is a word at the foot of the
// page. A fetch runs a step at a time between passes of the loop, so the page
// in front keeps answering, and it replaces the edition only if it arrived.
//
// The stories are Kagi's (news.kagi.com, CC BY-NC 4.0): syntheses of what many
// outlets reported, each ending with its sources and that attribution.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/AutoRefresh.h"
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
  // A fetch of the edition. Quiet: behind the page, with no notice at the end.
  void startEdition(bool quiet);
  void stepEdition();
  void endEdition(const char* headline, const std::string& message, bool kept);
  bool autoFetchDue() const;
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

  // The edition being fetched, between steps.
  struct EditionJob {
    enum class Stage : uint8_t { Paint, Join, Categories, Sections, Commit };
    bool quiet = false;
    Stage stage = Stage::Paint;
    size_t section = 0;
    std::vector<news::Category> sections;  // the choice when the fetch began
    news::CategoriesReader categories;
    news::Deduper once;
    std::vector<std::vector<news::Item>> fresh;
    std::vector<std::string> files;
    std::string failed;  // in the batch, but nothing could be read
    std::string absent;  // not in today's batch at all
    size_t bytes = 0;
    uint32_t lowestHeap = 0xFFFFFFFFu;
    uint32_t started = 0;
    uint32_t created = 0;
    int total = 0;
  };
  std::unique_ptr<EditionJob> job_;
  std::optional<autorefresh::Radio> radio_;

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
