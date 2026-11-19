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
constexpr const char* kChosenPath = "/.crosspoint/news/sections.cfg";
constexpr const char* kCategoriesPath = "/.crosspoint/news/categories.tsv";
// The story files the current edition owns, so the next one can clear them.
constexpr const char* kManifestPath = "/.crosspoint/news/edition-files.txt";
// Which sections the edition on the card was fetched for, one slug to a line.
constexpr const char* kEditionSectionsPath = "/.crosspoint/news/edition-sections.txt";
// What the last fetch cost: read it off the card to check a device.
constexpr const char* kReportPath = "/.crosspoint/news/last-refresh.txt";
constexpr size_t kFileCap = 64u * 1024u;

std::string sectionPath(const std::string& slug) { return std::string(kDir) + "/sec-" + news::articleFile(slug); }
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

std::string slugsOf(const std::vector<news::Category>& sections) {
  std::string out;
  for (const news::Category& c : sections) out += c.slug + "\n";
  return out;
}

void sampleHeap(uint32_t& lowest) {
  const uint32_t free = ESP.getFreeHeap();
  if (free < lowest) lowest = free;
}

// Takes one category's stories as they are read: keeps the ones the edition
// has not told yet, up to a section's worth, and writes each to the card.
class Taker final : public news::StoriesReader::Sink {
 public:
  Taker(news::Deduper& once, std::vector<news::Item>& items, std::vector<std::string>& files)
      : once_(once), items_(items), files_(files) {}

  bool story(news::Story& story) override {
    if (static_cast<int>(items_.size()) >= news::kStoriesPerSection) return false;
    if (!once_.accept(story)) return true;
    const std::string file = news::articleFile(story.id);
    if (!writeFile(std::string(kStoryDir) + "/" + file, news::buildDocument(story))) {
      LOG_ERR("NEWS", "could not keep a story on the card");
      return false;
    }
    news::Item item;
    item.id = story.id;
    item.title = story.title;
    item.summary = story.summary.substr(0, story.summary.find('\n'));
    if (item.summary.size() > news::kLeadCap) {
      size_t cut = news::kLeadCap;
      while (cut > 0 && (static_cast<uint8_t>(item.summary[cut]) & 0xC0) == 0x80) --cut;
      const size_t word = item.summary.rfind(' ', cut);
      if (word != std::string::npos && word > cut / 2) cut = word;
      item.summary.resize(cut);
      item.summary += "...";
    }
    items_.push_back(std::move(item));
    files_.push_back(file);
    return static_cast<int>(items_.size()) < news::kStoriesPerSection;
  }

 private:
  news::Deduper& once_;
  std::vector<news::Item>& items_;
  std::vector<std::string>& files_;
};

}  // namespace

std::unique_ptr<Activity> NewsActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<NewsActivity>(renderer, mappedInput);
}

void NewsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  // No radio here: the edition opens from the card, and fetching one is the
  // only thing that asks for the network.
  loadChosen();
  loadFavourites();
  std::string size;
  if (readFile(kTextSizePath, size) && size[0] >= '0' && size[0] <= '2') {
    textSize_ = static_cast<uint8_t>(size[0] - '0');
  }
  loadSection(0);
  phase_ = Phase::Front;
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

void NewsActivity::loadChosen() {
  chosen_.clear();
  std::string text;
  if (readFile(kChosenPath, text)) news::unpackCategories(text, chosen_);
  if (chosen_.size() > static_cast<size_t>(news::kMaxSections)) chosen_.resize(static_cast<size_t>(news::kMaxSections));
  if (chosen_.empty()) {
    for (int i = 0; i < news::kDefaultSectionCount; ++i) chosen_.push_back(news::kDefaultSections[i]);
  }
}

void NewsActivity::saveChosen() {
  Storage.ensureDirectoryExists(kDir);
  if (!writeFile(kChosenPath, news::packCategories(chosen_))) LOG_ERR("NEWS", "could not keep the sections");
}

bool NewsActivity::isChosen(const std::string& slug) const {
  for (const news::Category& c : chosen_) {
    if (c.slug == slug) return true;
  }
  return false;
}

void NewsActivity::loadSection(const int view) {
  view_ = view >= static_cast<int>(chosen_.size()) ? 0 : view;
  items_.clear();
  if (showingFavourites()) {
    items_ = favourites_;
  } else if (!chosen_.empty()) {
    std::string text;
    uint32_t edition = 0;
    if (readFile(sectionPath(chosen_[static_cast<size_t>(view_)].slug), text)) {
      news::unpackSection(text, items_, edition);
      if (edition != 0) edition_ = edition;
    }
  }
  news::editionLabel(edition_, editionText_, sizeof(editionText_));
  pageStarts_.clear();
  frontPage_ = 0;
}

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
  if (reading_.id.empty() || title_.empty()) return;
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
    if (!writeFile(path, title_ + "\n\n" + body_)) {
      LOG_ERR("NEWS", "could not keep the favourite on the card");
      return;
    }
    favourites_.insert(favourites_.begin(), reading_);
  }
  saveFavourites();
  requestUpdate();
}

void NewsActivity::ensureConnected(const Pending what) {
  if (WiFi.status() == WL_CONNECTED) {
    RenderLock lock(*this);
    phase_ = Phase::Busy;
    pending_ = what;
    requestUpdate();
    return;
  }
  afterConnect_ = what;
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
                           {
                             RenderLock lock(*this);
                             phase_ = Phase::Busy;
                             pending_ = wanted;
                           }
                           requestUpdate();
                         });
}

bool NewsActivity::fetchCategories(news::CategoriesReader& into, size_t& bytes, uint32_t& lowestHeap) {
  news::JsonStream stream(into);
  HttpDownloader::fetchUrl(news::kApiCategories, [&stream, &bytes, &lowestHeap](const uint8_t* data, const size_t len) {
    bytes += len;
    sampleHeap(lowestHeap);
    return stream.feed(reinterpret_cast<const char*>(data), len);
  });
  if (into.categories.empty() || news::parseDate(into.createdAt) == 0) return false;
  // What Kagi offers changes; the chooser always shows the latest list seen.
  Storage.ensureDirectoryExists(kDir);
  writeFile(kCategoriesPath, news::packCategories(into.categories));
  categories_ = into.categories;
  return true;
}

void NewsActivity::fetchEdition() {
  const uint32_t started = millis();
  size_t bytes = 0;
  uint32_t lowestHeap = 0xFFFFFFFFu;

  // 1. What today's batch calls each category.
  news::CategoriesReader categories;
  if (!fetchCategories(categories, bytes, lowestHeap)) {
    showNotice("Could not reach Kagi News",
               "The list of today's sections did not arrive. The edition on the card is untouched.");
    return;
  }
  const uint32_t created = news::parseDate(categories.createdAt);
  std::string fetchedFor;
  readFile(kEditionSectionsPath, fetchedFor);
  if (edition_ != 0 && created <= edition_ && fetchedFor == slugsOf(chosen_)) {
    showNotice("Already the latest", std::string("Kagi has published nothing newer than the edition of ") +
                                         editionText_ + " that is on the card. It makes one a day, around midday UTC.");
    return;
  }

  // 2. Each section's stories, told once across the whole edition.
  Storage.ensureDirectoryExists(kDir);
  Storage.ensureDirectoryExists(kStoryDir);
  news::Deduper once;
  std::vector<std::vector<news::Item>> fresh(chosen_.size());
  std::vector<std::string> files;
  files.reserve(chosen_.size() * static_cast<size_t>(news::kStoriesPerSection));
  std::string failed;  // in the batch, but nothing could be read
  std::string absent;  // not in today's batch at all
  int total = 0;
  for (size_t s = 0; s < chosen_.size(); ++s) {
    std::string uuid;
    for (const news::Category& c : categories.categories) {
      if (c.slug == chosen_[s].slug) uuid = c.uuid;
    }
    if (uuid.empty()) {
      absent += (absent.empty() ? "" : ", ") + chosen_[s].name;
      continue;
    }
    fresh[s].reserve(static_cast<size_t>(news::kStoriesPerSection));
    Taker taker(once, fresh[s], files);
    news::StoriesReader reader(taker);
    news::JsonStream stream(reader);
    HttpDownloader::fetchUrl(news::storiesUrl(uuid),
                             [&stream, &bytes, &lowestHeap](const uint8_t* data, const size_t len) {
                               bytes += len;
                               sampleHeap(lowestHeap);
                               return stream.feed(reinterpret_cast<const char*>(data), len);
                             });
    LOG_INF("NEWS", "%s: %d kept of %d read, heap %lu", chosen_[s].slug.c_str(), static_cast<int>(fresh[s].size()),
            reader.seen(), static_cast<unsigned long>(ESP.getFreeHeap()));
    if (reader.seen() == 0) failed += (failed.empty() ? "" : ", ") + chosen_[s].name;
    total += static_cast<int>(fresh[s].size());
  }

  const uint32_t seconds = (millis() - started) / 1000;
  char numbers[160];
  std::snprintf(numbers, sizeof(numbers), "%d stories. %lu KB in %lu s. Lowest free memory %lu KB.", total,
                static_cast<unsigned long>(bytes / 1024), static_cast<unsigned long>(seconds),
                static_cast<unsigned long>(lowestHeap == 0xFFFFFFFFu ? 0 : lowestHeap / 1024));
  writeFile(kReportPath, std::string(categories.createdAt) + "\n" + numbers + "\nfailed: " + failed +
                             "\nnot in the batch: " + absent + "\n");
  LOG_INF("NEWS", "%s", numbers);

  // 3. Kept only whole: a section that did not transfer leaves the old edition be.
  if (!failed.empty() || total == 0) {
    for (const std::string& file : files) Storage.remove((std::string(kStoryDir) + "/" + file).c_str());
    showNotice("Edition not kept",
               std::string("Not all of it arrived, so the one on the card stays. Nothing came for: ") +
                   (failed.empty() ? "any section" : failed) + ". " + numbers);
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
  for (size_t s = 0; s < chosen_.size(); ++s) {
    writeFile(sectionPath(chosen_[s].slug), news::packSection(fresh[s], created));
  }
  writeFile(kEditionSectionsPath, slugsOf(chosen_));
  chosenChanged_ = false;

  {
    RenderLock lock(*this);
    edition_ = created;
    loadSection(showingFavourites() ? 0 : view_);
  }
  showNotice("Today's edition is on the card", std::string("Kagi News, ") + editionText_ + ". " + numbers +
                                                   (absent.empty() ? "" : " Not in today's batch: " + absent + "."));
}

void NewsActivity::openItem(const int index) {
  if (index < 0 || index >= static_cast<int>(items_.size())) return;
  const news::Item& item = items_[static_cast<size_t>(index)];
  // A favourite's own copy first: it outlives the edition it came from.
  std::string text;
  if (!readFile(favouritePath(item.id), text) && !readFile(storyPath(item.id), text)) {
    showNotice("Not on the card", "This story's text is missing. Fetching the edition again brings it back.");
    requestUpdate();
    return;
  }
  {
    RenderLock lock(*this);
    // The headline is the first paragraph; the reader sets it apart.
    const size_t gap = text.find("\n\n");
    title_ = gap == std::string::npos ? item.title : text.substr(0, gap);
    body_ = gap == std::string::npos ? text : text.substr(gap + 2);
    reading_ = item;
    readerSection_ = showingFavourites() ? "FAVOURITES" : chosen_[static_cast<size_t>(view_)].name;
    for (char& c : readerSection_) {
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 32);
    }
    page_ = 0;
    restoreAt_ = -1;
    phase_ = Phase::Reading;
  }
  requestUpdate();
}

void NewsActivity::openChooser() {
  if (categories_.empty()) {
    std::string text;
    if (readFile(kCategoriesPath, text)) news::unpackCategories(text, categories_);
  }
  if (categories_.empty()) {
    // Never fetched: the list of what Kagi offers has to come from Kagi.
    ensureConnected(Pending::Categories);
    return;
  }
  chooserPage_ = 0;
  phase_ = Phase::Chooser;
  requestUpdate();
}

void NewsActivity::toggleCategory(const int row) {
  const size_t at = static_cast<size_t>(chooserPage_ * newsui::kChooserRows + row);
  if (at >= categories_.size()) return;
  const news::Category& category = categories_[at];
  bool removed = false;
  for (size_t i = 0; i < chosen_.size(); ++i) {
    if (chosen_[i].slug != category.slug) continue;
    // The last section cannot go: an edition of nothing is not one.
    if (chosen_.size() > 1) chosen_.erase(chosen_.begin() + static_cast<long>(i));
    removed = true;
    break;
  }
  if (!removed) {
    if (chosen_.size() >= static_cast<size_t>(news::kMaxSections)) return;
    news::Category added = category;
    added.uuid.clear();
    chosen_.push_back(std::move(added));
  }
  chosenChanged_ = true;
  saveChosen();
  requestUpdate();
}

void NewsActivity::showNotice(const char* headline, const std::string& message) {
  RenderLock lock(*this);
  noticeHeadline_ = headline;
  noticeMessage_ = message;
  noticeReturn_ = Phase::Front;
  phase_ = Phase::Notice;
}

void NewsActivity::pageFront(const int delta) {
  const int pages = static_cast<int>(pageStarts_.size());
  if (pages <= 1) return;
  frontPage_ = (frontPage_ + delta + pages) % pages;
  requestUpdate();
}

void NewsActivity::stepSection(const int delta) {
  const int count = static_cast<int>(chosen_.size());
  if (count == 0) return;
  // Sideways through the sections, the favourites after the last of them.
  const int at = showingFavourites() ? count : view_;
  const int next = (at + delta + count + 1) % (count + 1);
  loadSection(next == count ? -1 : next);
  requestUpdate();
}

void NewsActivity::turnPage(const int delta) {
  if (phase_ != Phase::Reading) return;
  const uint32_t pages = paging_.pages();
  if (delta > 0 && page_ + 1 < pages) {
    ++page_;
  } else if (delta < 0 && page_ > 0) {
    --page_;
  } else {
    return;
  }
  requestUpdate();
}

void NewsActivity::loop() {
  namespace fui = freeink::ui;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) backPressSeen_ = true;

  if (pending_ != Pending::None) {
    const Pending what = pending_;
    pending_ = Pending::None;
    if (what == Pending::Edition) {
      fetchEdition();
    } else {
      news::CategoriesReader categories;
      size_t bytes = 0;
      uint32_t lowest = 0xFFFFFFFFu;
      if (fetchCategories(categories, bytes, lowest)) {
        chooserPage_ = 0;
        phase_ = Phase::Chooser;
      } else {
        showNotice("Could not reach Kagi News", "The list of sections to choose from did not arrive.");
      }
    }
    requestUpdate();
    return;
  }

  if (backPressSeen_ && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    backPressSeen_ = false;
    if (phase_ == Phase::Chooser) {
      phase_ = Phase::Menu;
      requestUpdate();
    } else if (phase_ != Phase::Front) {
      phase_ = Phase::Front;
      requestUpdate();
    } else {
      shelf::leave(renderer, mappedInput);
    }
    return;
  }

  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  const bool up = swipe == MappedInputManager::SwipeDir::Up;
  const bool down = swipe == MappedInputManager::SwipeDir::Down;
  // A swipe from the left edge is Back and never reaches here.
  const bool left = swipe == MappedInputManager::SwipeDir::Left;
  const bool right = swipe == MappedInputManager::SwipeDir::Right;
  const bool forward = mappedInput.wasReleased(MappedInputManager::Button::PageForward) ||
                       mappedInput.wasReleased(MappedInputManager::Button::Down);
  const bool backward = mappedInput.wasReleased(MappedInputManager::Button::PageBack) ||
                        mappedInput.wasReleased(MappedInputManager::Button::Up);
  if (phase_ == Phase::Reading) {
    if (forward || up || left) {
      turnPage(1);
      return;
    }
    if (backward || down || right) {
      turnPage(-1);
      return;
    }
  } else if (phase_ == Phase::Front) {
    if (forward || up) {
      pageFront(1);
      return;
    }
    if (backward || down) {
      pageFront(-1);
      return;
    }
    if (left || right) {
      stepSection(left ? 1 : -1);
      return;
    }
  } else if (phase_ == Phase::Chooser && (forward || up || backward || down)) {
    const int pages = (static_cast<int>(categories_.size()) + newsui::kChooserRows - 1) / newsui::kChooserRows;
    if (pages > 0) chooserPage_ = (chooserPage_ + ((forward || up) ? 1 : -1) + pages) % pages;
    requestUpdate();
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
  const int action = event.action;

  // A tap on the story itself turns the page: the right half forward, the left
  // half back.
  if (phase_ == Phase::Reading && action == fui::NO_ACTION) {
    turnPage(tapX >= renderer.getScreenWidth() / 2 ? 1 : -1);
    return;
  }

  if (action >= newsui::ActionOpen0 && action < newsui::ActionOpen0 + newsui::kMaxRows) {
    openItem(action - newsui::ActionOpen0);
    return;
  }
  if (action >= newsui::ActionSection0 && action < newsui::ActionSection0 + news::kMaxSections) {
    loadSection(action - newsui::ActionSection0);
    phase_ = Phase::Front;
    requestUpdate();
    return;
  }
  if (action >= newsui::ActionToggle0 && action < newsui::ActionToggle0 + newsui::kChooserRows) {
    toggleCategory(action - newsui::ActionToggle0);
    return;
  }
  const int chooserPages = (static_cast<int>(categories_.size()) + newsui::kChooserRows - 1) / newsui::kChooserRows;
  switch (action) {
    case newsui::ActionMenu:
      phase_ = Phase::Menu;
      requestUpdate();
      break;
    case newsui::ActionFavourites:
      loadSection(-1);
      phase_ = Phase::Front;
      requestUpdate();
      break;
    case newsui::ActionChoose:
      openChooser();
      break;
    case newsui::ActionRefresh:
      ensureConnected(Pending::Edition);
      break;
    case newsui::ActionDone:
      loadSection(0);
      if (chosenChanged_) {
        showNotice("Sections changed",
                   "The edition on the card was fetched for the sections you had before. Fetch "
                   "today's edition from the menu to fill the new ones.");
      } else {
        phase_ = Phase::Front;
      }
      requestUpdate();
      break;
    case newsui::ActionNext:
      if (phase_ == Phase::Chooser && chooserPages > 0) {
        chooserPage_ = (chooserPage_ + 1) % chooserPages;
        requestUpdate();
      } else if (phase_ == Phase::Front) {
        pageFront(1);
      }
      break;
    case newsui::ActionPrev:
      if (phase_ == Phase::Chooser && chooserPages > 0) {
        chooserPage_ = (chooserPage_ + chooserPages - 1) % chooserPages;
        requestUpdate();
      }
      break;
    case newsui::ActionTextSize: {
      textSize_ = static_cast<uint8_t>((textSize_ + 1) % 3);
      restoreAt_ =
          paging_.lines > 0 ? static_cast<int>(static_cast<uint64_t>(paging_.topOf(page_)) * 1000 / paging_.lines) : 0;
      Storage.ensureDirectoryExists(kDir);
      writeFile(kTextSizePath, std::string(1, static_cast<char>('0' + textSize_)));
      requestUpdate();
      break;
    }
    case newsui::ActionFavourite:
      toggleFavourite();
      break;
    case newsui::ActionBack:
      if (phase_ == Phase::Reading && showingFavourites()) loadSection(-1);
      phase_ = Phase::Front;
      requestUpdate();
      break;
    default:
      break;
  }
}

void NewsActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;
  renderer.clearScreen();
  // Two sets of faces, because a target draws from three: the page (small
  // serif, the reading serif at the reader's size, the display serif) and the
  // heads (small reading serif, bold serif, large bold serif).
  const int readingFace = phase_ != Phase::Reading ? toybox::kReadingMidFontId
                          : textSize_ == 0         ? toybox::kReadingSmallFontId
                          : textSize_ == 2         ? toybox::kReadingFontId
                                                   : toybox::kReadingMidFontId;
  fui::GfxRendererTarget page =
      toybox::makeTarget(renderer, toybox::Faces{toybox::kSerifSmallFontId, readingFace, toybox::kSerifTitleFontId});
  fui::GfxRendererTarget heads = toybox::makeTarget(
      renderer,
      toybox::Faces{toybox::kReadingSmallFontId, toybox::kReadingBoldSmallFontId, toybox::kReadingBoldFontId});
  const fui::DeviceContext device = page.deviceContext();
  const fui::ThemeTokens& tokens = toybox::themeTokens();
  const newsui::Ink ink{page, heads, tokens};
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(page, device, noInput, interactions_);
  toybox::Screen screen(frame);
  const char* what = "News";

  switch (phase_) {
    case Phase::Busy: {
      newsui::NoticeModel model;
      model.headline = "Fetching from Kagi News";
      model.message = "An edition takes a minute or two. The one on the card stays until the new one has arrived.";
      newsui::buildNotice(screen, ink, model);
      what = "News busy";
      break;
    }
    case Phase::Front: {
      std::vector<newsui::Headline> rows(items_.size());
      for (size_t i = 0; i < items_.size(); ++i) {
        rows[i].title = items_[i].title.c_str();
        rows[i].summary =
            i == 0 && !showingFavourites() && !items_[i].summary.empty() ? items_[i].summary.c_str() : nullptr;
      }
      const int count = static_cast<int>(rows.size());
      if (pageStarts_.empty()) {
        // Measured once per section: where each page of whole headlines starts.
        int top = 0;
        while (top < count && pageStarts_.size() < 16) {
          pageStarts_.push_back(top);
          const int fits = newsui::frontFit(ink, device, rows.data(), count, top);
          top += fits > 0 ? fits : 1;
        }
        if (pageStarts_.empty()) pageStarts_.push_back(0);
        if (frontPage_ >= static_cast<int>(pageStarts_.size())) frontPage_ = 0;
      }
      newsui::FrontModel model;
      const bool favourites = showingFavourites();
      model.section = favourites ? "Favourites" : chosen_[static_cast<size_t>(view_)].name.c_str();
      model.edition = favourites ? "" : editionText_;
      model.items = rows.empty() ? nullptr : rows.data();
      model.count = count;
      model.top = pageStarts_[static_cast<size_t>(frontPage_)];
      model.page = frontPage_ + 1;
      model.pages = static_cast<int>(pageStarts_.size());
      if (rows.empty() && favourites) {
        model.emptyHeadline = "No favourites yet";
        model.emptyMessage = "Open a story and tap Save. It stays here, and its text stays on the card.";
      } else if (rows.empty()) {
        model.emptyHeadline = edition_ == 0 ? "No edition on the card" : "Nothing in this section today";
        model.emptyMessage = edition_ == 0
                                 ? "One fetch over Wi-Fi brings the day's stories from Kagi News. After that they read "
                                   "with no network."
                                 : "Either Kagi had nothing for it, or the section was chosen after the edition was "
                                   "fetched.";
        model.emptyCanFetch = true;
      }
      newsui::buildFront(screen, ink, model);
      what = "News front";
      break;
    }
    case Phase::Menu: {
      const char* names[news::kMaxSections] = {};
      int n = 0;
      for (const news::Category& c : chosen_) {
        if (n < news::kMaxSections) names[n++] = c.name.c_str();
      }
      newsui::MenuModel model;
      model.edition = editionText_;
      model.sections = names;
      model.sectionCount = n;
      model.current = view_;
      model.favourites = static_cast<int>(favourites_.size());
      newsui::buildMenu(screen, ink, model);
      what = "News menu";
      break;
    }
    case Phase::Chooser: {
      const int total = static_cast<int>(categories_.size());
      const int pages = (total + newsui::kChooserRows - 1) / newsui::kChooserRows;
      if (chooserPage_ >= pages) chooserPage_ = 0;
      const char* names[newsui::kChooserRows] = {};
      bool marks[newsui::kChooserRows] = {};
      int n = 0;
      for (int i = chooserPage_ * newsui::kChooserRows; i < total && n < newsui::kChooserRows; ++i, ++n) {
        names[n] = categories_[static_cast<size_t>(i)].name.c_str();
        marks[n] = isChosen(categories_[static_cast<size_t>(i)].slug);
      }
      newsui::ChooserModel model;
      model.names = names;
      model.chosen = marks;
      model.count = n;
      model.chosenTotal = static_cast<int>(chosen_.size());
      model.maxChosen = news::kMaxSections;
      model.page = chooserPage_ + 1;
      model.pages = pages < 1 ? 1 : pages;
      newsui::buildChooser(screen, ink, model);
      what = "News chooser";
      break;
    }
    case Phase::Reading: {
      const newsui::StoryMetrics m = newsui::storyMetrics(ink, device, title_.c_str());
      fui::TextStyle bodyStyle;
      bodyStyle.font = fui::FONT_SLOT_BODY;
      bodyStyle.maxLines = 255;
      paging_.lines = wrap_.lineCount(page, m.body.width, body_.c_str(), bodyStyle);
      paging_.first = m.firstLines;
      paging_.rest = m.restLines;
      if (restoreAt_ >= 0) {
        page_ = paging_.pageOf(static_cast<uint32_t>(static_cast<uint64_t>(paging_.lines) * restoreAt_ / 1000));
        restoreAt_ = -1;
      }
      if (page_ >= paging_.pages()) page_ = paging_.pages() - 1;
      std::snprintf(pageLabel_, sizeof(pageLabel_), "%lu / %lu", static_cast<unsigned long>(page_ + 1),
                    static_cast<unsigned long>(paging_.pages()));
      newsui::StoryModel model;
      model.section = readerSection_.c_str();
      model.title = title_.c_str();
      model.body = body_.c_str();
      model.wrap = &wrap_;
      model.topLine = paging_.topOf(page_);
      model.firstPage = page_ == 0;
      model.pageLabel = pageLabel_;
      model.favourite = isFavourite(reading_.id);
      newsui::buildStory(screen, ink, model);
      what = "News story";
      break;
    }
    case Phase::Notice: {
      newsui::NoticeModel model;
      model.headline = noticeHeadline_.c_str();
      model.message = noticeMessage_.c_str();
      model.backButton = true;
      newsui::buildNotice(screen, ink, model);
      what = "News notice";
      break;
    }
  }

  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, what);
  renderer.displayBuffer();
}
