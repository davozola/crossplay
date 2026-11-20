#pragma once

// WIKIPEDIA: Spanish Wikipedia over Wi-Fi, read like a book.
//
// The app used to read a pack of English articles copied onto the card. It now
// asks es.wikipedia.org: a search, a random article, one article's text. Every
// article opened is kept on the card (the last kCachedArticles of them), so it
// opens again with no network, and the saved titles answer a search by
// themselves when there is no Wi-Fi.
//
// The activity is the thin layer: it owns the query, the article the book
// engine is laying out, the history and the radio. What the network says is
// read in WikiOnlineCore (freestanding, host-tested); the drawing is
// WikipediaScreens.cpp (freestanding).

#include <Epub/Page.h>
#include <Epub/Section.h>
#include <FreeInkUIGfxRenderer.h>
#include <PaintClock.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../activities/Activity.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxScreen.h"
#include "WikiOnlineCore.h"
#include "../wikipedia/WikipediaCore.h"
#include "WikipediaScreens.h"

class WikiesActivity final : public Activity {
 public:
  WikiesActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Wikipedia", renderer, mappedInput) {}
  ~WikiesActivity() override;

  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  // Keep laying out the open article between renders.
  bool skipLoopDelay() override;

 private:
  enum class View : uint8_t { Search, Article, Contents, List, Notice };
  // What the list view is showing: the ways to discover, a list Wikipedia
  // answered with, the articles on the card, or those same ones to delete.
  enum class ListKind : uint8_t { Discover, Hits, Saved, Delete };
  enum class Feed : uint8_t { TopDay, TopMonth, Featured, Good };
  // Work that needs the network, run from loop() once its status is painted.
  enum class Job : uint8_t { None, Search, Random, Open, Feed };
  enum class Fail : uint8_t { None, Network, Missing, Card };
  struct Visit {
    uint32_t id;
    int page;
    std::string title;
  };
  // The article on the panel: what the card's copy says about it.
  struct Doc {
    std::string title;
    std::vector<std::string> headings;
    size_t bytes = 0;
  };
  static constexpr int kHistoryDepth = 8;
  // Below this many bytes of XHTML, headings flow with the text; above it each
  // top-level heading starts a fresh page.
  static constexpr size_t kFreshPageBytes = 24 * 1024;
  // The radio stays up this long after the last fetch, so the next search or
  // article in a row does not pay for a join; then it goes down.
  static constexpr uint32_t kRadioIdleMs = 45000;

  void go(View next);
  void routeAction(int action, int value);
  void showNotice(const char* headline, const char* body, const char* actionLabel, freeink::ui::ActionId action);
  void noticeAbout(const char* headline, const char* fmt, const std::string& title);

  // network
  void startJob(Job job);
  void runJob();
  bool online();
  bool fetchSmall(const std::string& url, std::string& body, size_t cap);
  Fail download(uint32_t& id, const std::string& titleHint);
  void runFeed();

  // lists
  void showList(ListKind kind, const std::string& title);
  void showDiscover();
  void showSaved(bool deleting);
  void closeList();
  void listTapped(int row);
  void forget(uint32_t id);
  void forgetAll();
  uint32_t savedIdFor(const std::string& title) const;
  void runSearch();
  void runRandom();

  // the card
  std::string dirFor(uint32_t id) const;
  bool isCached(uint32_t id) const;
  bool loadMeta();
  void loadIndex();
  void saveIndex() const;
  void touchIndex(uint32_t id, const std::string& title);
  void loadState();
  void saveState();
  void refreshFooter();

  // search
  void refreshLocal();
  void handleKey(int value);
  void open(uint32_t id, int page, const std::string& title);
  freeink::ui::Rect keyboardRect() const;
  void drawKeyboard();

  // article
  bool openLocator(uint32_t id, int page, const std::string& anchor, const std::string& title);
  bool ensureBuilt();
  void closeArticle();
  void turnPage(int delta);
  void pushHistory();
  void popHistory();
  void refreshHeadingPages();
  int headingForPage(int page) const;
  void renderArticle(toybox::Screen& screen);
  void ensureSection(const freeink::ui::Rect& body);
  void layOutUnderCue();
  void paintOpeningCue(const std::string& title, const char* status);

  View view_ = View::Search;
  wikipedia::State state_;
  std::string footer_;

  // network
  Job job_ = Job::None;
  bool jobArmed_ = false;  // the network is there (or can be joined): paint the status
  bool jobReady_ = false;  // the status is on the panel: run it
  uint32_t openId_ = 0;
  int openPage_ = 0;
  std::string openTitle_;
  std::optional<autorefresh::Radio> radio_;
  uint32_t radioUsedMs_ = 0;
  bool netUsed_ = false;

  // lists
  ListKind listKind_ = ListKind::Discover;
  Feed feed_ = Feed::TopDay;
  std::string listTitle_;
  std::vector<std::string> listRows_;
  std::vector<wikies::Hit> listHits_;
  int listFirst_ = 0;
  int listShown_ = 0;
  bool fromList_ = false;  // the open article was picked from the list view

  // the card: every article kept, most recently opened first
  std::vector<wikies::Hit> saved_;

  // search
  std::string query_;
  std::vector<wikies::Hit> results_;
  bool searched_ = false;  // results_ is Wikipedia's answer for query_, not only the saved titles
  bool offline_ = false;   // that answer could not be had
  bool shifted_ = false;
  bool symbols_ = false;
  bool keyboardShown_ = false;
  freeink::ui::InteractionBuffer<56> kbInteractions_;
  paintclock::RevealGate kbGate_;

  // article
  uint32_t locator_ = 0;
  Doc article_;
  std::string cacheDir_;
  std::unique_ptr<Section> section_;
  int targetPage_ = 0;
  std::string pendingAnchor_;
  std::vector<int> headingPages_;  // page of each heading, -1 while unknown
  bool headingPagesFinal_ = false;
  std::vector<Visit> history_;
  bool buildFailed_ = false;
  int contentsFirst_ = 0;
  int contentsShown_ = 0;
  int recentRows_[wikiesui::kMaxRecent] = {};

  // notice
  std::string noticeHead_;
  std::string noticeBody_;
  const char* noticeAction_ = nullptr;
  freeink::ui::ActionId noticeActionId_ = 0;

  toybox::Interactions interactions_;
  bool cuePainted_ = false;
  freeink::ui::Rect cueBody_{};
  bool interactionsReady_ = false;
};
