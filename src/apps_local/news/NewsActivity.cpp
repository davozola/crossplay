#include "NewsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <Utf8.h>
#include <WiFi.h>

#include <cstdio>
#include <ctime>

#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ShelfScreen.h"
#include "../hackernews/HackerNewsCore.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "DevMode.h"

namespace {

// The text extractor HACKER NEWS reads articles through. Every article opened
// here tells that third party what is being read; the headlines do not go
// near it.
constexpr const char* kExtractorPrefix = "https://r.jina.ai/";
constexpr const char* kCacheDir = "/.crosspoint/news";
// Every article that opened in full is kept here, so it opens again with no
// network. Nothing prunes it: an article is a few tens of kilobytes.
constexpr const char* kArticleDir = "/.crosspoint/news/a";
constexpr const char* kFavouriteDir = "/.crosspoint/news/f";
constexpr const char* kFavouriteIndex = "/.crosspoint/news/favourites.tsv";
constexpr size_t kMaxArticleBytes = 128u * 1024u;
constexpr size_t kCacheCap = 96u * 1024u;

void cachePath(const int section, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/s%d.tsv", kCacheDir, section);
}

std::string articlePath(const std::string& url) { return std::string(kArticleDir) + "/" + news::articleFile(url); }

std::string favouritePath(const std::string& url) { return std::string(kFavouriteDir) + "/" + news::articleFile(url); }

uint32_t nowSeconds() {
  const time_t now = time(nullptr);
  return now > 0 ? static_cast<uint32_t>(now) : 0;
}

}  // namespace

std::unique_ptr<Activity> NewsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<NewsActivity>(renderer, mappedInput);
}

void NewsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  // No radio here: the last headlines open from the card, and the first tap
  // that needs the network is what asks for it.
  loadFavourites();
  loadSection(0);
  phase_ = Phase::List;
  requestUpdate();
}

void NewsActivity::onExit() {
  Activity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

void NewsActivity::loadSection(const int section) {
  section_ = section;
  items_.clear();
  fetched_ = 0;
  fetchFailed_ = false;
  if (showingFavourites()) {
    items_ = favourites_;
  } else {
    char path[48];
    cachePath(section, path, sizeof(path));
    std::string text;
    if (Storage.exists(path) && Storage.readFileToString("NEWS", path, kCacheCap, text)) {
      news::unpackSection(text, items_, fetched_);
    }
  }
  topIndex_ = 0;
  rowsBuilt_ = false;
}

void NewsActivity::saveSection() {
  Storage.ensureDirectoryExists(kCacheDir);
  char path[48];
  cachePath(section_, path, sizeof(path));
  const std::string packed = news::packSection(items_, fetched_);
  HalFile file;
  if (!Storage.openFileForWrite("NEWS", path, file)) {
    LOG_ERR("NEWS", "could not keep section %d on the card", section_);
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(packed.data()), packed.size());
}

const char* NewsActivity::sourceOf(const news::Item& item) const {
  if (showingFavourites()) return item.summary.c_str();
  const news::Section& section = news::kSections[section_];
  return item.feed < section.feedCount ? section.feeds[item.feed].name : "";
}

void NewsActivity::loadFavourites() {
  favourites_.clear();
  std::string text;
  uint32_t unused = 0;
  if (Storage.exists(kFavouriteIndex) && Storage.readFileToString("NEWS", kFavouriteIndex, kCacheCap, text)) {
    news::unpackSection(text, favourites_, unused);
  }
}

void NewsActivity::saveFavourites() {
  Storage.ensureDirectoryExists(kCacheDir);
  const std::string packed = news::packSection(favourites_, 0);
  HalFile file;
  if (!Storage.openFileForWrite("NEWS", kFavouriteIndex, file)) {
    LOG_ERR("NEWS", "could not keep the favourites on the card");
    return;
  }
  file.write(reinterpret_cast<const uint8_t*>(packed.data()), packed.size());
}

bool NewsActivity::isFavourite(const std::string& url) const {
  for (const news::Item& f : favourites_) {
    if (f.url == url) return true;
  }
  return false;
}

void NewsActivity::toggleFavourite() {
  if (reading_.url.empty() || document_.empty()) return;
  const std::string path = favouritePath(reading_.url);
  if (isFavourite(reading_.url)) {
    for (size_t i = 0; i < favourites_.size(); ++i) {
      if (favourites_[i].url == reading_.url) {
        favourites_.erase(favourites_.begin() + static_cast<long>(i));
        break;
      }
    }
    Storage.remove(path.c_str());
  } else {
    if (static_cast<int>(favourites_.size()) >= news::kPerSection) {
      LOG_ERR("NEWS", "favourites are full");
      return;
    }
    Storage.ensureDirectoryExists(kFavouriteDir);
    HalFile file;
    if (!Storage.openFileForWrite("NEWS", path, file)) {
      LOG_ERR("NEWS", "could not keep the favourite on the card");
      return;
    }
    file.write(reinterpret_cast<const uint8_t*>(document_.data()), document_.size());
    news::Item kept = reading_;
    kept.summary = readingSource_;
    kept.feed = 0;
    favourites_.insert(favourites_.begin(), std::move(kept));
  }
  saveFavourites();
  if (showingFavourites()) items_ = favourites_;
  rowsBuilt_ = false;
  requestUpdate();
}

bool NewsActivity::hasLead() const { return !items_.empty() && topIndex_ == 0; }

void NewsActivity::ensureConnected(const Pending what, const char* busyMessage) {
  if (WiFi.status() == WL_CONNECTED) {
    request(what, busyMessage);
    return;
  }
  afterConnect_ = what;
  afterConnectMessage_ = busyMessage;
  // The Back press that may be on record belongs to the screen about to stop
  // being on top; carried across the picker it would close the app on return.
  backPressSeen_ = false;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           const Pending wanted = afterConnect_;
                           afterConnect_ = Pending::None;
                           if (result.isCancelled) {
                             requestUpdate();
                             return;
                           }
                           request(wanted, afterConnectMessage_);
                         });
}

void NewsActivity::request(const Pending what, const char* busyMessage) {
  if (what == Pending::None) return;
  {
    RenderLock lock(*this);
    phase_ = Phase::Busy;
    busyMessage_ = busyMessage;
    pending_ = what;
  }
  requestUpdate();
}

bool NewsActivity::fetchSection() {
  const news::Section& section = news::kSections[section_];
  std::vector<news::Item> fresh;
  fresh.reserve(static_cast<size_t>(section.feedCount * news::kPerFeed));
  failedFeeds_ = 0;
  for (int f = 0; f < section.feedCount; ++f) {
    news::FeedScanner scanner(fresh, static_cast<uint8_t>(f));
    const bool ok = HttpDownloader::fetchUrl(section.feeds[f].url, [&scanner](const uint8_t* data, const size_t len) {
      return scanner.feed(reinterpret_cast<const char*>(data), len);
    });
    // A transfer the scanner cut short because it had enough is a success.
    if (!ok && !scanner.full() && scanner.kept() == 0) {
      ++failedFeeds_;
      LOG_ERR("NEWS", "feed failed: %s", section.feeds[f].name);
    } else {
      LOG_INF("NEWS", "%s: %d items", section.feeds[f].name, scanner.kept());
    }
  }
  phase_ = Phase::List;
  if (fresh.empty()) {
    // The headlines already on the card stay; a failed refresh takes nothing.
    fetchFailed_ = true;
    return true;
  }
  news::mergeSection(fresh);
  items_ = std::move(fresh);
  fetched_ = nowSeconds();
  fetchFailed_ = false;
  topIndex_ = 0;
  rowsBuilt_ = false;
  saveSection();
  return true;
}

bool NewsActivity::fetchArticle() {
  if (selected_ < 0 || selected_ >= static_cast<int>(items_.size())) return false;
  const news::Item& item = items_[static_cast<size_t>(selected_)];
  const char* source = sourceOf(item);

  std::string response;
  std::string url = kExtractorPrefix;
  url += item.url;
  bool overLimit = false;
  const bool ok = HttpDownloader::fetchUrl(url, [&response, &overLimit](const uint8_t* data, const size_t len) {
    if (response.size() + len > kMaxArticleBytes) {
      overLimit = true;
      return false;
    }
    response.append(reinterpret_cast<const char*>(data), len);
    return true;
  });

  char age[8];
  news::ageLabel(item.when, nowSeconds(), age, sizeof(age));
  document_ = item.title;
  document_ += "\n\n";
  document_ += source;
  if (age[0] != '\0') {
    document_ += "  ";
    document_ += age;
  }
  document_ += "\n\n";

  const hn::Extracted extracted = (ok || overLimit) ? hn::splitExtractorResponse(response) : hn::Extracted{};
  if (hn::readsAsProse(extracted.body)) {
    for (const std::string& paragraph : hn::paragraphsFromMarkdown(extracted.body)) {
      document_ += paragraph;
      document_ += "\n\n";
    }
    Storage.ensureDirectoryExists(kArticleDir);
    HalFile kept;
    if (Storage.openFileForWrite("NEWS", articlePath(item.url), kept)) {
      kept.write(reinterpret_cast<const uint8_t*>(document_.data()), document_.size());
      rowsBuilt_ = false;
    } else {
      LOG_ERR("NEWS", "could not keep the article on the card");
    }
  } else if (!item.summary.empty()) {
    // A paywall, a page that needs a browser, or no network: the outlet's own
    // summary is still worth reading, and saying which it is.
    document_ += item.summary;
    document_ += "\n\n";
    document_ += ok ? "The full text did not come through: the page is behind a paywall or needs a browser. This is "
                      "the outlet's own summary."
                    : "The article could not be fetched. This is the outlet's own summary.";
    document_ += "\n";
  } else {
    showNotice(ok ? "NOT READABLE HERE" : "COULD NOT FETCH IT",
               ok ? "That page came back with no article in it, and its feed carries no summary."
                  : "The article did not arrive. Check the network and try again.");
    return true;
  }

  RenderLock lock(*this);
  readerTitle_ = source;
  readingSource_ = source;
  reading_ = item;
  topLine_ = 0;
  lineCount_ = 0;
  visibleLines_ = 0;
  phase_ = Phase::Reading;
  return true;
}

bool NewsActivity::openKept(const int index) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return false;
  const news::Item& item = items_[static_cast<size_t>(index)];
  // A favourite's own copy first: it outlives anything that clears the others.
  std::string path = favouritePath(item.url);
  if (!Storage.exists(path.c_str())) path = articlePath(item.url);
  std::string text;
  if (!Storage.exists(path.c_str()) || !Storage.readFileToString("NEWS", path, kMaxArticleBytes * 2, text) ||
      text.empty()) {
    return false;
  }
  RenderLock lock(*this);
  document_ = std::move(text);
  readingSource_ = sourceOf(item);
  reading_ = item;
  readerTitle_ = readingSource_;
  topLine_ = 0;
  lineCount_ = 0;
  visibleLines_ = 0;
  phase_ = Phase::Reading;
  return true;
}

void NewsActivity::openItem(const int index) {
  selected_ = index;
  if (openKept(index)) {
    requestUpdate();
    return;
  }
  ensureConnected(Pending::Article, "OPENING");
}

void NewsActivity::showNotice(const char* headline, const char* message) {
  RenderLock lock(*this);
  noticeHeadline_ = headline;
  noticeMessage_ = message;
  phase_ = Phase::Notice;
}

void NewsActivity::pageList(const int delta) {
  const int count = static_cast<int>(rows_.size());
  if (count <= 0 || visibleRows_ <= 0) return;
  // The first page of the lead arrangement holds fewer rows than the rest, so
  // pages are walked rather than computed.
  if (delta > 0) {
    if (topIndex_ + visibleRows_ < count) {
      topIndex_ += visibleRows_;
    } else {
      topIndex_ = 0;
    }
  } else {
    topIndex_ = topIndex_ > 0 ? (topIndex_ > visibleRows_ ? topIndex_ - visibleRows_ : 0) : 0;
  }
  requestUpdate();
}

void NewsActivity::turnPage(const int delta) {
  if (phase_ != Phase::Reading || visibleLines_ == 0) return;
  const uint32_t span = visibleLines_;
  const uint32_t maxTop = lineCount_ > span ? lineCount_ - span : 0;
  if (delta > 0) {
    topLine_ = topLine_ + span > maxTop ? maxTop : topLine_ + span;
  } else {
    topLine_ = topLine_ > span ? topLine_ - span : 0;
  }
  requestUpdate();
}

void NewsActivity::loop() {
  namespace fui = freeink::ui;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) backPressSeen_ = true;

  if (pending_ != Pending::None) {
    const Pending what = pending_;
    pending_ = Pending::None;
    const bool ok = what == Pending::Section ? fetchSection() : fetchArticle();
    if (!ok && phase_ == Phase::Busy) {
      showNotice("COULD NOT FETCH IT", "Nothing arrived. Check the network and try again.");
    }
    requestUpdate();
    return;
  }

  if (backPressSeen_ && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    backPressSeen_ = false;
    if (phase_ == Phase::Reading || phase_ == Phase::Notice) {
      phase_ = Phase::List;
      requestUpdate();
    } else {
      shelf::leave(renderer, mappedInput);
    }
    return;
  }

  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  const bool swipeNext = swipe == MappedInputManager::SwipeDir::Up;
  const bool swipePrev = swipe == MappedInputManager::SwipeDir::Down;
  if (phase_ == Phase::Reading) {
    // Sideways too, the way a page is turned. A swipe from the left edge is
    // Back and never reaches here.
    const bool sideNext = swipe == MappedInputManager::SwipeDir::Left;
    const bool sidePrev = swipe == MappedInputManager::SwipeDir::Right;
    if (mappedInput.wasReleased(MappedInputManager::Button::PageForward) || swipeNext || sideNext) {
      turnPage(1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageBack) || swipePrev || sidePrev) {
      turnPage(-1);
      return;
    }
  }
  const bool next = mappedInput.wasReleased(MappedInputManager::Button::Down) || swipeNext;
  const bool prev = mappedInput.wasReleased(MappedInputManager::Button::Up) || swipePrev;
  if (phase_ == Phase::List && (next || prev)) {
    pageList(next ? 1 : -1);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);

  // A tap on the article itself turns the page: the right half forward, the
  // left half back.
  if (phase_ == Phase::Reading && event.action == fui::NO_ACTION) {
    turnPage(tapX >= renderer.getScreenWidth() / 2 ? 1 : -1);
    return;
  }

  if (event.action >= newsui::ActionTab0 && event.action <= newsui::ActionTab0 + news::kSectionCount) {
    loadSection(event.action - newsui::ActionTab0);
    phase_ = Phase::List;
    requestUpdate();
    return;
  }
  switch (event.action) {
    case newsui::ActionOpenLead:
      openItem(0);
      break;
    case newsui::ActionOpen:
      openItem(event.value);
      break;
    case newsui::ActionFavourite:
      toggleFavourite();
      break;
    case newsui::ActionRefresh:
      if (showingFavourites()) break;
      ensureConnected(Pending::Section, "FETCHING THE HEADLINES");
      break;
    case newsui::ActionPagePrev:
      turnPage(-1);
      break;
    case newsui::ActionPageNext:
      turnPage(1);
      break;
    case newsui::ActionBack:
      if (showingFavourites()) items_ = favourites_;
      phase_ = Phase::List;
      requestUpdate();
      break;
    default:
      break;
  }
}

void NewsActivity::buildRows() {
  const uint32_t now = nowSeconds();
  labels_.clear();
  values_.clear();
  rows_.clear();
  labels_.reserve(items_.size());
  values_.reserve(items_.size());
  rows_.reserve(items_.size());
  leadKicker_.clear();
  leadTitle_.clear();
  leadSummary_.clear();
  for (size_t i = 0; i < items_.size(); ++i) {
    const news::Item& item = items_[i];
    char age[8];
    news::ageLabel(item.when, now, age, sizeof(age));
    std::string value = sourceOf(item);
    if (age[0] != '\0') {
      value += "  ";
      value += age;
    }
    if (!showingFavourites()) {
      if (isFavourite(item.url)) {
        value += "  FAV";
      } else if (Storage.exists(articlePath(item.url).c_str())) {
        value += "  ON CARD";
      }
    }
    if (i == 0) {
      leadKicker_ = value;
      leadTitle_ = item.title;
      leadSummary_ = showingFavourites() ? std::string() : item.summary;
      continue;
    }
    labels_.push_back(item.title);
    values_.push_back(value);
  }
  for (size_t i = 0; i < labels_.size(); ++i) {
    freeink::ui::ListItem row;
    row.label = labels_[i].c_str();
    row.subtitle = values_[i].c_str();
    row.actionValue = static_cast<int16_t>(i + 1);
    rows_.push_back(row);
  }
  rowsBuilt_ = true;
}

void NewsActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  // The reading cut one step down: a few more words to the line.
  const toybox::Faces faces =
      phase_ == Phase::Reading
          ? toybox::Faces{toybox::kReadingSmallFontId, toybox::kReadingMidFontId, toybox::kReadingBoldFontId}
          : toybox::Faces{toybox::kTileFontId, toybox::kReadingMidFontId, toybox::kDisplayFontId};
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, faces);
  const fui::DeviceContext device = target.deviceContext();
  const fui::ThemeTokens& tokens = toybox::themeTokens();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  const char* what = "News";

  switch (phase_) {
    case Phase::Busy: {
      newsui::NoticeModel model;
      model.headline = busyMessage_;
      newsui::buildNotice(screen, model);
      what = "News busy";
      break;
    }
    case Phase::List: {
      if (!rowsBuilt_) {
        buildRows();
        const fui::TextStyle titleStyle = newsui::listTitleStyle(tokens);
        const int16_t width = newsui::listTitleWidth(device, tokens);
        for (std::string& label : labels_) label = toybox::fitLines(target, label.c_str(), width, 2, titleStyle);
        for (size_t i = 0; i < rows_.size(); ++i) rows_[i].label = labels_[i].c_str();
      }
      const bool lead = hasLead();
      const int16_t rowHeight = newsui::listRowHeight(target, tokens);
      visibleRows_ = fui::listVisibleRows(newsui::listBand(device, lead, target, tokens), rowHeight, tokens.listRowGap);
      if (topIndex_ >= static_cast<int>(rows_.size())) topIndex_ = 0;

      newsui::ListModel model;
      const bool favourites = showingFavourites();
      model.title = favourites ? "FAVOURITES" : news::kSections[section_].title;
      model.canRefresh = !favourites;
      const char* tabs[newsui::kMaxTabs] = {};
      for (int s = 0; s < news::kSectionCount && s < newsui::kMaxTabs - 1; ++s) tabs[s] = news::kSections[s].name;
      tabs[news::kSectionCount] = "FAVS";
      model.tabs = tabs;
      model.tabCount = news::kSectionCount + 1;
      model.tab = section_;
      model.items = rows_.empty() ? nullptr : rows_.data();
      model.count = static_cast<int>(rows_.size());
      model.topIndex = topIndex_;
      if (lead) {
        model.leadKicker = leadKicker_.c_str();
        model.leadTitle = leadTitle_.c_str();
        model.leadSummary = leadSummary_.empty() ? nullptr : leadSummary_.c_str();
      }
      if (items_.empty() && favourites) {
        model.emptyHeadline = "NO FAVOURITES YET";
        model.emptyMessage = "Open an article and tap FAV. It stays here, and its text stays on the card.";
      } else if (items_.empty()) {
        model.emptyHeadline = fetchFailed_ ? "NOTHING ARRIVED" : "NO HEADLINES YET";
        model.emptyMessage = fetchFailed_ ? "None of this section's feeds answered. Check the network and try again."
                                          : "This section has not been fetched on this card. It needs Wi-Fi once; "
                                            "after that the last headlines open without it.";
      }
      newsui::buildList(screen, model);
      what = "News list";
      break;
    }
    case Phase::Reading: {
      const fui::Rect body = newsui::readerBody(device);
      visibleLines_ = fui::textAreaVisibleLines(body, target.lineHeight(tokens.bodyText.font));
      lineCount_ = wrap_.lineCount(target, body.width, document_.c_str(), tokens.bodyText);
      const uint32_t pages = visibleLines_ > 0 ? (lineCount_ + visibleLines_ - 1) / visibleLines_ : 1;
      const uint32_t page = visibleLines_ > 0 ? topLine_ / visibleLines_ + 1 : 1;
      std::snprintf(pageLabel_, sizeof(pageLabel_), "%lu/%lu", static_cast<unsigned long>(page),
                    static_cast<unsigned long>(pages < 1 ? 1 : pages));
      newsui::ReaderModel model;
      model.title = readerTitle_.c_str();
      model.text = document_.c_str();
      model.wrap = &wrap_;
      model.topLine = topLine_;
      model.pageLabel = pageLabel_;
      model.canPagePrev = topLine_ > 0;
      model.canPageNext = lineCount_ > topLine_ + visibleLines_;
      model.favourite = isFavourite(reading_.url);
      newsui::buildReader(screen, model);
      what = "News reader";
      break;
    }
    case Phase::Notice: {
      newsui::NoticeModel model;
      model.headline = noticeHeadline_.c_str();
      model.message = noticeMessage_.c_str();
      model.backButton = true;
      newsui::buildNotice(screen, model);
      what = "News notice";
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, what);
  const auto labels = mappedInput.mapLabels("Back", "", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
