#include "NewsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>

#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ShelfScreen.h"
#include "../ui/Toybox.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxTheme.h"
#include "DevMode.h"

namespace {

constexpr const char* kDir = "/.crosspoint/news";
// The stories of the edition on the card, one file each.
constexpr const char* kStoryDir = "/.crosspoint/news/a";
constexpr const char* kFavouriteDir = "/.crosspoint/news/f";
constexpr const char* kFavouriteIndex = "/.crosspoint/news/favourites.tsv";
constexpr const char* kTextSizePath = "/.crosspoint/news/textsize.cfg";
// The story files the current edition owns, so the next one can clear them.
constexpr const char* kManifestPath = "/.crosspoint/news/edition-files.txt";
// What the last refresh cost: read it off the card to check a device.
constexpr const char* kReportPath = "/.crosspoint/news/last-refresh.txt";
constexpr size_t kFileCap = 64u * 1024u;
// Fewer stories than this in any section and the edition is not kept.
constexpr int kMinStories = 3;

void sectionPath(const int section, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/edition-%d.tsv", kDir, section);
}

std::string storyPath(const std::string& id) { return std::string(kStoryDir) + "/" + news::articleFile(id); }
std::string favouritePath(const std::string& id) { return std::string(kFavouriteDir) + "/" + news::articleFile(id); }

bool writeFile(const std::string& path, const std::string& text) {
  HalFile file;
  if (!Storage.openFileForWrite("NEWS", path, file)) return false;
  return file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size()) == text.size();
}

bool readFile(const std::string& path, std::string& text) {
  return Storage.exists(path.c_str()) && Storage.readFileToString("NEWS", path, kFileCap, text) && !text.empty();
}

// What a refresh measures, for the report and the notice that follows it.
struct Meter {
  size_t bytes = 0;
  uint32_t lowestHeap = 0xFFFFFFFFu;
  void sample(const size_t len) {
    bytes += len;
    const uint32_t free = ESP.getFreeHeap();
    if (free < lowestHeap) lowestHeap = free;
  }
};

// Takes one category's stories as they are read: keeps the ones the edition
// has not told yet, up to the category's share, and writes each to the card.
class Taker final : public news::StoriesReader::Sink {
 public:
  Taker(news::Deduper& once, std::vector<news::Item>& items, std::vector<std::string>& files, const int take)
      : once_(once), items_(items), files_(files), take_(take) {}

  bool story(news::Story& story) override {
    if (static_cast<int>(items_.size()) >= news::kStoriesPerSection || taken_ >= take_) return false;
    if (!once_.accept(story)) return true;
    const std::string file = news::articleFile(story.id);
    if (!writeFile(std::string(kStoryDir) + "/" + file, news::buildDocument(story))) {
      LOG_ERR("NEWS", "could not keep a story on the card");
      failed = true;
      return false;
    }
    news::Item item;
    item.id = story.id;
    item.title = story.title;
    item.kicker = news::kickerOf(story);
    item.summary = story.summary.substr(0, story.summary.find('\n'));
    if (item.summary.size() > news::kLeadCap) {
      size_t cut = news::kLeadCap;
      while (cut > 0 && (static_cast<uint8_t>(item.summary[cut]) & 0xC0) == 0x80) --cut;
      item.summary.resize(cut);
      item.summary += "...";
    }
    items_.push_back(std::move(item));
    files_.push_back(file);
    ++taken_;
    return taken_ < take_ && static_cast<int>(items_.size()) < news::kStoriesPerSection;
  }

  bool failed = false;

 private:
  news::Deduper& once_;
  std::vector<news::Item>& items_;
  std::vector<std::string>& files_;
  int take_;
  int taken_ = 0;
};

}  // namespace

std::unique_ptr<Activity> NewsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<NewsActivity>(renderer, mappedInput);
}

void NewsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  // No radio here: the edition opens from the card, and REFRESH is the one
  // thing that asks for the network.
  loadFavourites();
  std::string size;
  if (readFile(kTextSizePath, size) && size[0] >= '0' && size[0] <= '2') {
    textSize_ = static_cast<uint8_t>(size[0] - '0');
  }
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
  if (showingFavourites()) {
    items_ = favourites_;
  } else {
    char path[64];
    sectionPath(section, path, sizeof(path));
    std::string text;
    if (readFile(path, text)) news::unpackSection(text, items_, edition_);
  }
  topIndex_ = 0;
  rowsBuilt_ = false;
}

bool NewsActivity::hasLead() const { return !items_.empty() && topIndex_ == 0; }

void NewsActivity::loadFavourites() {
  favourites_.clear();
  std::string text;
  uint32_t unused = 0;
  if (readFile(kFavouriteIndex, text)) news::unpackSection(text, favourites_, unused);
}

void NewsActivity::saveFavourites() {
  Storage.ensureDirectoryExists(kDir);
  if (!writeFile(kFavouriteIndex, news::packSection(favourites_, 0))) {
    LOG_ERR("NEWS", "could not keep the favourites on the card");
  }
}

bool NewsActivity::isFavourite(const std::string& id) const {
  for (const news::Item& f : favourites_) {
    if (f.id == id) return true;
  }
  return false;
}

void NewsActivity::toggleFavourite() {
  if (reading_.id.empty() || document_.empty()) return;
  const std::string path = favouritePath(reading_.id);
  if (isFavourite(reading_.id)) {
    for (size_t i = 0; i < favourites_.size(); ++i) {
      if (favourites_[i].id == reading_.id) {
        favourites_.erase(favourites_.begin() + static_cast<long>(i));
        break;
      }
    }
    Storage.remove(path.c_str());
  } else {
    if (favourites_.size() >= 40) {
      LOG_ERR("NEWS", "favourites are full");
      return;
    }
    Storage.ensureDirectoryExists(kDir);
    Storage.ensureDirectoryExists(kFavouriteDir);
    if (!writeFile(path, document_)) {
      LOG_ERR("NEWS", "could not keep the favourite on the card");
      return;
    }
    favourites_.insert(favourites_.begin(), reading_);
  }
  saveFavourites();
  if (showingFavourites()) items_ = favourites_;
  rowsBuilt_ = false;
  requestUpdate();
}

void NewsActivity::ensureConnected() {
  if (WiFi.status() == WL_CONNECTED) {
    RenderLock lock(*this);
    phase_ = Phase::Busy;
    fetchPending_ = true;
    requestUpdate();
    return;
  }
  // The Back press that may be on record belongs to the screen about to stop
  // being on top; carried across the picker it would close the app on return.
  backPressSeen_ = false;
  WiFi.mode(WIFI_STA);
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             requestUpdate();
                             return;
                           }
                           {
                             RenderLock lock(*this);
                             phase_ = Phase::Busy;
                             fetchPending_ = true;
                           }
                           requestUpdate();
                         });
}

void NewsActivity::fetchEdition() {
  const uint32_t started = millis();
  Meter meter;

  // 1. What today's batch calls each category.
  news::CategoriesReader categories;
  {
    news::JsonStream stream(categories);
    HttpDownloader::fetchUrl(news::kApiCategories, [&stream, &meter](const uint8_t* data, const size_t len) {
      meter.sample(len);
      return stream.feed(reinterpret_cast<const char*>(data), len);
    });
  }
  const uint32_t created = news::parseDate(categories.createdAt);
  if (categories.categories.empty() || created == 0) {
    showNotice("COULD NOT REACH KAGI NEWS",
               "The list of today's categories did not arrive. The edition on the card is untouched.");
    return;
  }
  uint32_t onCard = 0;
  {
    char path[64];
    sectionPath(0, path, sizeof(path));
    std::string text;
    std::vector<news::Item> unused;
    if (readFile(path, text)) news::unpackSection(text, unused, onCard);
  }
  if (onCard != 0 && created <= onCard) {
    char label[16];
    news::editionLabel(onCard, label, sizeof(label));
    showNotice("ALREADY THE LATEST", std::string("Kagi has published nothing newer than the edition of ") + label +
                                         " that is on the card. It makes one a day, around midday UTC.");
    return;
  }

  // 2. Each section's stories, told once across the whole edition.
  Storage.ensureDirectoryExists(kDir);
  Storage.ensureDirectoryExists(kStoryDir);
  news::Deduper once;
  std::vector<news::Item> fresh[news::kSectionCount];
  std::vector<std::string> files;
  files.reserve(static_cast<size_t>(news::kSectionCount * news::kStoriesPerSection));
  std::string trouble;
  for (int s = 0; s < news::kSectionCount; ++s) {
    const news::Section& section = news::kSections[s];
    fresh[s].reserve(static_cast<size_t>(news::kStoriesPerSection));
    for (int c = 0; c < news::kMaxCategoriesPerSection && section.categories[c] != nullptr; ++c) {
      const char* slug = section.categories[c];
      std::string uuid;
      for (const news::CategoryRef& ref : categories.categories) {
        if (ref.slug == slug) uuid = ref.uuid;
      }
      const size_t before = fresh[s].size();
      if (!uuid.empty()) {
        Taker taker(once, fresh[s], files, section.take[c]);
        news::StoriesReader reader(taker);
        news::JsonStream stream(reader);
        HttpDownloader::fetchUrl(news::storiesUrl(uuid), [&stream, &meter](const uint8_t* data, const size_t len) {
          meter.sample(len);
          return stream.feed(reinterpret_cast<const char*>(data), len);
        });
      }
      LOG_INF("NEWS", "%s: %d stories, heap %lu", slug, static_cast<int>(fresh[s].size() - before),
              static_cast<unsigned long>(ESP.getFreeHeap()));
      if (fresh[s].size() == before) {
        if (!trouble.empty()) trouble += ", ";
        trouble += slug;
      }
    }
  }

  // 3. Kept only whole: a section that came up short leaves the old edition be.
  int total = 0;
  bool whole = true;
  for (int s = 0; s < news::kSectionCount; ++s) {
    total += static_cast<int>(fresh[s].size());
    whole = whole && static_cast<int>(fresh[s].size()) >= kMinStories;
  }
  const uint32_t seconds = (millis() - started) / 1000;
  char numbers[160];
  std::snprintf(numbers, sizeof(numbers), "%d stories. %lu KB in %lu s. Lowest free memory %lu KB.", total,
                static_cast<unsigned long>(meter.bytes / 1024), static_cast<unsigned long>(seconds),
                static_cast<unsigned long>(meter.lowestHeap == 0xFFFFFFFFu ? 0 : meter.lowestHeap / 1024));
  writeFile(kReportPath, std::string(categories.createdAt) + "\n" + numbers + "\nnothing from: " + trouble + "\n");
  LOG_INF("NEWS", "%s", numbers);

  if (!whole) {
    for (const std::string& file : files) Storage.remove((std::string(kStoryDir) + "/" + file).c_str());
    showNotice("EDITION NOT KEPT",
               std::string("Too little of it arrived, so the one on the card stays. Nothing came from: ") +
                   (trouble.empty() ? "(every category answered, but with too few stories)" : trouble) + ". " +
                   numbers);
    return;
  }

  // The previous edition's stories go, except the ones this edition also has.
  std::string previous;
  if (readFile(kManifestPath, previous)) {
    size_t at = 0;
    while (at < previous.size()) {
      size_t end = previous.find('\n', at);
      if (end == std::string::npos) end = previous.size();
      const std::string name = previous.substr(at, end - at);
      at = end + 1;
      bool still = name.empty();
      for (const std::string& file : files) still = still || file == name;
      if (!still) Storage.remove((std::string(kStoryDir) + "/" + name).c_str());
    }
  }
  std::string manifest;
  for (const std::string& file : files) manifest += file + "\n";
  writeFile(kManifestPath, manifest);
  for (int s = 0; s < news::kSectionCount; ++s) {
    char path[64];
    sectionPath(s, path, sizeof(path));
    writeFile(path, news::packSection(fresh[s], created));
  }

  char label[16];
  news::editionLabel(created, label, sizeof(label));
  {
    RenderLock lock(*this);
    loadSection(showingFavourites() ? 0 : section_);
  }
  showNotice("TODAY'S EDITION IS ON THE CARD", std::string("Kagi News, ") + label + ". " + numbers +
                                                   (trouble.empty() ? "" : " Nothing came from: " + trouble + "."));
}

void NewsActivity::openItem(const int index) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  const news::Item& item = items_[static_cast<size_t>(index)];
  // A favourite's own copy first: it outlives the edition it came from.
  std::string text;
  if (!readFile(favouritePath(item.id), text) && !readFile(storyPath(item.id), text)) {
    showNotice("NOT ON THE CARD", "This story's text is missing. REFRESH fetches the edition again.");
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    document_ = std::move(text);
    reading_ = item;
    readerTitle_ = showingFavourites() ? "FAVOURITES" : news::kSections[section_].title;
    topLine_ = 0;
    lineCount_ = 0;
    visibleLines_ = 0;
    phase_ = Phase::Reading;
  }
  requestUpdate();
}

void NewsActivity::showNotice(const char* headline, const std::string& message) {
  RenderLock lock(*this);
  noticeHeadline_ = headline;
  noticeMessage_ = message;
  phase_ = Phase::Notice;
}

void NewsActivity::pageList(const int delta) {
  const int count = static_cast<int>(rows_.size());
  if (count <= 0 || visibleRows_ <= 0) return;
  // The first page holds the lead and so fewer rows than the rest; pages are
  // walked rather than computed.
  if (delta > 0) {
    topIndex_ = topIndex_ + visibleRows_ < count ? topIndex_ + visibleRows_ : 0;
  } else {
    topIndex_ = topIndex_ > visibleRows_ ? topIndex_ - visibleRows_ : 0;
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

  if (fetchPending_) {
    fetchPending_ = false;
    fetchEdition();
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

  // A tap on the story itself turns the page: the right half forward, the left
  // half back.
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
    case newsui::ActionTextSize: {
      textSize_ = static_cast<uint8_t>((textSize_ + 1) % 3);
      restoreAt_ = lineCount_ > 0 ? static_cast<int>(static_cast<uint64_t>(topLine_) * 1000 / lineCount_) : 0;
      Storage.ensureDirectoryExists(kDir);
      writeFile(kTextSizePath, std::string(1, static_cast<char>('0' + textSize_)));
      requestUpdate();
      break;
    }
    case newsui::ActionFavourite:
      toggleFavourite();
      break;
    case newsui::ActionRefresh:
      if (!showingFavourites()) ensureConnected();
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
    std::string kicker = item.kicker;
    if (!showingFavourites() && isFavourite(item.id)) kicker += "  FAV";
    if (i == 0) {
      leadKicker_ = kicker;
      leadTitle_ = item.title;
      leadSummary_ = item.summary;
      continue;
    }
    labels_.push_back(item.title);
    values_.push_back(kicker);
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
  // The reading cut one step down, or the size the reader chose.
  const toybox::Faces faces =
      phase_ == Phase::Reading ? toybox::Faces{toybox::kReadingSmallFontId,
                                               textSize_ == 0   ? toybox::kReadingSmallFontId
                                               : textSize_ == 2 ? toybox::kReadingFontId
                                                                : toybox::kReadingMidFontId,
                                               toybox::kReadingBoldFontId}
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
      model.headline = "FETCHING TODAY'S EDITION";
      model.message =
          "Four sections from Kagi News. It takes a minute or two; the edition on the card stays until "
          "the new one has arrived whole.";
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
      for (int s = 0; s < news::kSectionCount && s < newsui::kMaxTabs - 1; ++s) tabs[s] = news::kSections[s].tab;
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
        model.emptyMessage = "Open a story and tap FAV. It stays here, and its text stays on the card.";
      } else if (items_.empty()) {
        model.emptyHeadline = "NO EDITION ON THE CARD";
        model.emptyMessage =
            "One fetch over Wi-Fi brings the day's stories from Kagi News; after that they read "
            "with no network.";
      }
      newsui::buildList(screen, model);
      what = "News list";
      break;
    }
    case Phase::Reading: {
      const fui::Rect body = newsui::readerBody(device);
      visibleLines_ = fui::textAreaVisibleLines(body, target.lineHeight(tokens.bodyText.font));
      lineCount_ = wrap_.lineCount(target, body.width, document_.c_str(), tokens.bodyText);
      if (restoreAt_ >= 0 && visibleLines_ > 0) {
        // The same place in the text, snapped to the top of a page.
        const uint32_t line = static_cast<uint32_t>(static_cast<uint64_t>(lineCount_) * restoreAt_ / 1000);
        topLine_ = line / visibleLines_ * visibleLines_;
        restoreAt_ = -1;
      }
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
      model.favourite = isFavourite(reading_.id);
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
