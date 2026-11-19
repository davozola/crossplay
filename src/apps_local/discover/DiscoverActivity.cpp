#include "DiscoverActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../hackernews/HackerNewsCore.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ExtraIcons.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxSerif.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace dc = discover;

namespace {

enum : fui::ActionId {
  ActionOpen = 1,  // value: the row on this page
  ActionPagePrev,
  ActionPageNext,
  ActionKeptList,
  ActionRefresh,
  ActionChoose,  // value: Choice
  ActionPick,    // value: the option
  ActionKeep,
  ActionReadme,
  ActionDismiss,
};

constexpr char kDir[] = "/.crosspoint/discover";
constexpr char kFilterPath[] = "/.crosspoint/discover/filter";
constexpr char kListPath[] = "/.crosspoint/discover/list.txt";
constexpr char kKeptPath[] = "/.crosspoint/discover/kept.txt";
constexpr char kStagedPath[] = "/.crosspoint/discover/answer.tmp";
constexpr size_t kMaxAnswerBytes = 320u * 1024u;
constexpr size_t kMaxReadmeBytes = 160u * 1024u;
constexpr char kAgent[] = "crossplay-discover";

constexpr int kBottomReserve = 40;
constexpr int kBackSwipeEdge = 28;
constexpr uint8_t kCleanAfterPaints = 8;
constexpr int kRows = 5;
constexpr int kFootH = 48;
constexpr int kBodyLine = 29;
constexpr int kBodyFont = toybox::kReadingSmallFontId;
constexpr int kBoldFont = toybox::kReadingBoldSmallFontId;
constexpr int kHeadFont = toybox::kReadingBoldFontId;

int32_t today() {
  const time_t now = time(nullptr);
  return now > 1700000000 ? static_cast<int32_t>(now / 86400) : 0;
}

bool download(const char* url, const char* accept) {
  Storage.ensureDirectoryExists(kDir);
  const std::vector<HttpDownloader::Header> headers = {
      {"User-Agent", kAgent}, {"Accept", accept}, {"X-GitHub-Api-Version", "2022-11-28"}};
  const auto result = HttpDownloader::downloadToFile(url, kStagedPath, nullptr, nullptr, "", "", headers);
  if (result != HttpDownloader::OK) {
    LOG_ERR("DISC", "fetch failed, status %d: %s", HttpDownloader::lastStatus(), url);
    Storage.remove(kStagedPath);
    return false;
  }
  return true;
}

// What a chip says: the choice in a word.
const char* chipText(const dc::Filter& f, const int choice) {
  static constexpr const char* kLanguages[dc::kLanguages] = {"ANY LANG", "C",  "C++", "RUST",
                                                             "PYTHON",   "GO", "TS",  "ZIG"};
  static constexpr const char* kStars[dc::kStarChoices] = {"10+", "50+", "200+", "1000+"};
  static constexpr const char* kAges[dc::kAges] = {"WEEK", "MONTH", "YEAR", "ACTIVE"};
  switch (choice) {
    case 0:
      return dc::topicName(f.topic);
    case 1:
      return kLanguages[static_cast<int>(f.language) % dc::kLanguages];
    case 2:
      return kStars[static_cast<int>(f.stars) % dc::kStarChoices];
    default:
      return kAges[static_cast<int>(f.age) % dc::kAges];
  }
}

// Markup that is not prose: a README's centred logos and badges.
void stripTags(std::string& text) {
  std::string out;
  out.reserve(text.size());
  bool tag = false;
  for (const char c : text) {
    if (c == '<') {
      tag = true;
    } else if (c == '>' && tag) {
      tag = false;
    } else if (!tag) {
      out.push_back(c);
    }
  }
  text.swap(out);
}

}  // namespace

std::unique_ptr<Activity> DiscoverActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<DiscoverActivity>(renderer, mappedInput);
}

void DiscoverActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  found_ = makeUniqueNoThrow<dc::Repo[]>(dc::kPerPage);
  kept_ = makeUniqueNoThrow<dc::Repo[]>(dc::kMaxSaved);
  if (!found_ || !kept_) {
    LOG_ERR("DISC", "OOM: the lists");
    shelf::leave(renderer, mappedInput);
    return;
  }
  loadState();
  view_ = View::List;
  page_ = 0;
  cleanNext_ = true;
  requestUpdate();
}

void DiscoverActivity::onExit() {
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

void DiscoverActivity::loadState() {
  filter_ = dc::Filter{};
  foundCount_ = 0;
  keptCount_ = 0;
  foundOn_[0] = 0;
  if (Storage.exists(kFilterPath)) {
    HalFile file;
    uint8_t bytes[4];
    if (Storage.openFileForRead("DISC", kFilterPath, file) && file.read(bytes, 4) == 4)
      dc::unpackFilter(bytes, filter_);
  }
  std::string text;
  if (Storage.exists(kListPath) && Storage.readFileToString("DISC", kListPath, 48u * 1024u, text)) {
    // The first line is the day it was fetched.
    const size_t nl = text.find('\n');
    if (nl != std::string::npos && nl < sizeof(foundOn_)) {
      std::memcpy(foundOn_, text.data(), nl);
      foundOn_[nl] = 0;
      foundCount_ = dc::unpackSaved(std::string_view(text).substr(nl + 1), found_.get(), dc::kPerPage);
    }
  }
  text.clear();
  if (Storage.exists(kKeptPath) && Storage.readFileToString("DISC", kKeptPath, 48u * 1024u, text)) {
    keptCount_ = dc::unpackSaved(text, kept_.get(), dc::kMaxSaved);
  }
}

void DiscoverActivity::saveFilter() {
  Storage.ensureDirectoryExists(kDir);
  uint8_t bytes[4];
  dc::packFilter(filter_, bytes);
  HalFile file;
  if (!Storage.openFileForWrite("DISC", kFilterPath, file)) return;
  file.write(bytes, 4);
  file.flush();
}

void DiscoverActivity::saveList() {
  Storage.ensureDirectoryExists(kDir);
  std::string text = std::string(foundOn_) + "\n" + dc::packSaved(found_.get(), foundCount_);
  HalFile file;
  if (!Storage.openFileForWrite("DISC", kListPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  file.flush();
}

void DiscoverActivity::saveKept() {
  Storage.ensureDirectoryExists(kDir);
  const std::string text = dc::packSaved(kept_.get(), keptCount_);
  HalFile file;
  if (!Storage.openFileForWrite("DISC", kKeptPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  file.flush();
}

int DiscoverActivity::shownCount() const { return showingKept_ ? keptCount_ : foundCount_; }

const dc::Repo* DiscoverActivity::shown(const int index) const {
  if (index < 0 || index >= shownCount()) return nullptr;
  // The kept list reads newest first.
  return showingKept_ ? &kept_[static_cast<size_t>(keptCount_ - 1 - index)] : &found_[static_cast<size_t>(index)];
}

int DiscoverActivity::linesPerPage() const {
  const int top = toybox::kChromeHeight + 40;
  const int bottom = renderer.getScreenHeight() - kBottomReserve - 34;
  return (bottom - top) / kBodyLine;
}

int DiscoverActivity::readmePages() const {
  const int per = linesPerPage();
  const int pages = (static_cast<int>(lines_.size()) + per - 1) / per;
  return pages < 1 ? 1 : pages;
}

void DiscoverActivity::showStatus(const char* headline, const char* detail, const bool dismissable) {
  if (view_ != View::Status) statusBack_ = view_;
  statusHeadline_ = headline;
  std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", detail);
  statusDismissable_ = dismissable;
  view_ = View::Status;
}

void DiscoverActivity::start(const Job job) {
  job_ = job;
  step_ = Step::Joining;
  showStatus("JOINING WI-FI", "", false);
  wifiActivated_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    step_ = Step::Ready;
    return;
  }
  {
    // The saved network is joined behind the page; the picker is for when there is none.
    ActivityResultHandler afterWifi = [this](const ActivityResult& result) {
      if (result.isCancelled) {
        step_ = Step::Idle;
        job_ = Job::None;
        showStatus("NO WI-FI", "THE LAST LIST FETCHED AND WHAT YOU KEPT STAY ON THE CARD.", true);
      } else {
        step_ = Step::Ready;
      }
      requestUpdate();
    };
    if (autorefresh::joinSaved()) {
      afterWifi(ActivityResult{});
    } else {
      startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput), std::move(afterWifi));
    }
  }
}

void DiscoverActivity::runSearch() {
  view_ = statusBack_;
  const int32_t day = today();
  if (day == 0) {
    showStatus("NO DATE", "THE SEARCH IS BY DATE AND THE CLOCK IS NOT SET. SET IT IN THE CLOCK APP.", true);
    return;
  }
  char url[420];
  dc::searchUrl(filter_, day, url, sizeof(url));
  std::string json;
  if (!download(url, "application/vnd.github+json") ||
      !Storage.readFileToString("DISC", kStagedPath, kMaxAnswerBytes, json)) {
    Storage.remove(kStagedPath);
    const int status = HttpDownloader::lastStatus();
    showStatus("NO ANSWER",
               status == 403 || status == 429 ? "GITHUB ALLOWS TEN SEARCHES A MINUTE WITHOUT AN ACCOUNT. WAIT A MINUTE."
                                              : "GITHUB DID NOT ANSWER. TRY AGAIN LATER.",
               true);
    return;
  }
  Storage.remove(kStagedPath);
  int32_t total = 0;
  const int count = dc::parseSearch(json.data(), json.size(), found_.get(), dc::kPerPage, total);
  if (count < 0) {
    showStatus("NOT AN ANSWER", "WHAT GITHUB SENT BACK WAS NOT A LIST OF REPOSITORIES.", true);
    return;
  }
  foundCount_ = count;
  total_ = total;
  int y = 0, m = 0, d = 0;
  dc::civilOfDay(day, y, m, d);
  std::snprintf(foundOn_, sizeof(foundOn_), "%04d-%02d-%02d", y % 10000, m % 100, d % 100);
  saveList();
  showingKept_ = false;
  page_ = 0;
  cleanNext_ = true;
}

void DiscoverActivity::runReadme() {
  view_ = statusBack_;
  char url[160];
  dc::readmeUrl(open_.fullName, url, sizeof(url));
  std::string raw;
  if (!download(url, "text/plain") || !Storage.readFileToString("DISC", kStagedPath, kMaxReadmeBytes, raw)) {
    Storage.remove(kStagedPath);
    showStatus("NO README", "THERE IS NO README.MD AT THE TOP OF THAT REPOSITORY, OR IT DID NOT ARRIVE.", true);
    return;
  }
  Storage.remove(kStagedPath);
  stripTags(raw);
  std::vector<std::string> paragraphs = hn::paragraphsFromMarkdown(raw);
  raw.clear();
  raw.shrink_to_fit();
  lines_.clear();
  const int width = renderer.getScreenWidth() - toybox::kMargin * 2;
  for (std::string& p : paragraphs) {
    dc::latinOnly(p.data());
    p.resize(std::strlen(p.c_str()));
    if (p.empty()) continue;
    toyboxserif::wrap(renderer, kBodyFont, p.c_str(), width, 0, lines_);
    lines_.emplace_back();
    // A README can be a book; forty pages of it is plenty here.
    if (static_cast<int>(lines_.size()) > linesPerPage() * 40) break;
  }
  while (!lines_.empty() && lines_.back().empty()) lines_.pop_back();
  if (lines_.empty()) {
    showStatus("NOTHING TO READ", "THAT README HAS NO PROSE IN IT.", true);
    return;
  }
  readmePage_ = 0;
  view_ = View::Readme;
  cleanNext_ = true;
}

void DiscoverActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = job_ == Job::Search ? "ASKING GITHUB" : "FETCHING THE README";
    std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", job_ == Job::Readme ? open_.fullName : "");
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    step_ = Step::Idle;
    const Job job = job_;
    job_ = Job::None;
    if (job == Job::Search) runSearch();
    if (job == Job::Readme) runReadme();
    requestUpdate();
    return;
  }

  int pressX = 0;
  int pressY = 0;
  if (mappedInput.wasScreenTouchPressed(pressX, pressY)) pressX_ = pressX;

  auto turn = [&](const int by) {
    if (view_ == View::Readme) {
      if (readmePage_ + by >= 0 && readmePage_ + by < readmePages()) {
        readmePage_ += by;
      } else if (by > 0 || readmePage_ == 0) {
        view_ = View::Detail;
        cleanNext_ = true;
      }
    } else if (view_ == View::List) {
      const int pages = (shownCount() + kRows - 1) / kRows;
      if (page_ + by >= 0 && page_ + by < pages) page_ += by;
    }
  };

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    if (view_ == View::Readme && mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge && readmePage_ > 0) {
      turn(-1);
      requestUpdate();
      return;
    }
    if (view_ == View::List) {
      if (showingKept_) {
        showingKept_ = false;
        page_ = 0;
        requestUpdate();
        return;
      }
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view_ == View::Status) {
      view_ = statusBack_;
    } else if (view_ == View::Readme) {
      view_ = View::Detail;
    } else {
      view_ = View::List;
    }
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::None && step_ == Step::Idle &&
      (view_ == View::Readme || view_ == View::List)) {
    turn(swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up ? 1 : -1);
    requestUpdate();
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  if (view_ == View::Readme) {
    if (tapY < toybox::kChromeHeight + 32) {
      view_ = View::Detail;
      cleanNext_ = true;
    } else {
      turn(tapX < renderer.getScreenWidth() / 3 ? -1 : 1);
    }
    requestUpdate();
    return;
  }
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions_.route(input));
}

void DiscoverActivity::route(const fui::ActionEvent& event) {
  switch (event.action) {
    case ActionOpen: {
      const dc::Repo* repo = shown(page_ * kRows + event.value);
      if (repo == nullptr) return;
      open_ = *repo;
      view_ = View::Detail;
      cleanNext_ = true;
      break;
    }
    case ActionPagePrev:
      if (page_ == 0) return;
      --page_;
      break;
    case ActionPageNext:
      if ((page_ + 1) * kRows >= shownCount()) return;
      ++page_;
      break;
    case ActionKeptList:
      showingKept_ = !showingKept_;
      page_ = 0;
      break;
    case ActionRefresh:
      start(Job::Search);
      break;
    case ActionChoose:
      if (event.value < 0 || event.value > 3) return;
      choice_ = static_cast<Choice>(event.value);
      view_ = View::Picker;
      cleanNext_ = true;
      break;
    case ActionPick: {
      if (view_ != View::Picker || event.value < 0) return;
      if (choice_ == Choice::Topic && event.value < dc::kTopics) filter_.topic = static_cast<dc::Topic>(event.value);
      if (choice_ == Choice::Language && event.value < dc::kLanguages) {
        filter_.language = static_cast<dc::Language>(event.value);
      }
      if (choice_ == Choice::Stars && event.value < dc::kStarChoices)
        filter_.stars = static_cast<dc::Stars>(event.value);
      if (choice_ == Choice::Age && event.value < dc::kAges) filter_.age = static_cast<dc::Age>(event.value);
      saveFilter();
      view_ = View::List;
      showingKept_ = false;
      // A new filter is a new search.
      start(Job::Search);
      break;
    }
    case ActionKeep:
      keptCount_ = dc::toggleSaved(kept_.get(), keptCount_, open_);
      saveKept();
      break;
    case ActionReadme:
      start(Job::Readme);
      break;
    case ActionDismiss:
      view_ = statusBack_;
      cleanNext_ = true;
      break;
    default:
      return;
  }
  requestUpdate();
}

void DiscoverActivity::buildList(toybox::Screen& screen) {
  lab::chrome(screen, showingKept_ ? "DISCOVER: KEPT" : "DISCOVER");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  int y = toybox::kChromeHeight + 8;
  char text[96];

  if (!showingKept_) {
    // The four choices the search is made of.
    const int gap = 6;
    const int chipW = (inner - gap * 3) / 4;
    for (int i = 0; i < 4; ++i) {
      fui::ButtonProps chip;
      chip.label = chipText(filter_, i);
      chip.action = ActionChoose;
      chip.value = static_cast<int16_t>(i);
      chip.styles = toybox::rowStyles();
      chip.text = toybox::buttonText(screen.theme());
      chip.radius = lab::kRadius;
      chip.minTouchSize = 0;
      screen.button(chip, lab::rect(x + i * (chipW + gap), y, chipW, 42));
    }
    y += 42 + 6;
  }

  const int count = shownCount();
  const int pages = count > 0 ? (count + kRows - 1) / kRows : 1;
  if (page_ >= pages) page_ = pages - 1;
  if (count == 0) {
    lab::label(screen, lab::rect(x, y + 50, inner, toybox::kDisplayCut.inkHeight),
               showingKept_ ? "NOTHING KEPT" : "NOTHING YET", toybox::kDisplayFont, toybox::kDisplayCut,
               fui::TextAlign::Left);
    lab::paragraph(screen, x, y + 50 + toybox::kDisplayCut.inkHeight + 22, inner,
                   showingKept_
                       ? "KEEP A REPOSITORY FROM ITS PAGE AND IT IS LISTED HERE, WITH NO NETWORK NEEDED."
                       : "CHOOSE WHAT TO LOOK FOR ABOVE, OR TAP REFRESH FOR THIS WEEK'S NEW REPOSITORIES WITH THE "
                         "MOST STARS.",
                   4);
  }
  const int rowH = (footY - 6 - y) / kRows;
  for (int row = 0; row < kRows; ++row) {
    const dc::Repo* repo = shown(page_ * kRows + row);
    if (repo == nullptr) break;
    fui::ButtonProps open;
    open.label = "";
    open.action = ActionOpen;
    open.value = static_cast<int16_t>(row);
    open.styles = toybox::rowStyles();
    open.styles.normal.borderWidth = 0;
    open.minTouchSize = 0;
    screen.button(open, lab::rect(x, y, inner, rowH - 2));

    // The stars on the right, the name beside them, what it is under both.
    char stars[16];
    dc::formatCount(repo->stars, stars, sizeof(stars));
    const int starsW = renderer.getTextWidth(toybox::kUiFontId, stars);
    renderer.drawText(toybox::kUiFontId, x + inner - starsW, y - 3, stars, true);
    screen.target().bitmap(lab::rect(x + inner - starsW - 30, y + 6, 24, 24), fui::bitmapFromIcon(icon_star_24),
                           fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::Black));
    toyboxserif::draw(renderer, kBoldFont, 28, x, y + 2, inner - starsW - 40, dc::nameOf(*repo), 1);
    const int descH = rowH >= 112 ? 2 : 1;
    toyboxserif::draw(renderer, kBodyFont, 26, x, y + 32, inner,
                      repo->description[0] != 0 ? repo->description : "No description.", descH);
    char owner[40];
    std::snprintf(owner, sizeof(owner), "%.*s", static_cast<int>(dc::nameOf(*repo) - repo->fullName - 1),
                  repo->fullName);
    std::snprintf(text, sizeof(text), "%s%s%s   %s", owner, repo->language[0] ? "   " : "", repo->language,
                  repo->created);
    for (char* c = text; *c != 0; ++c) {
      if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    }
    lab::caption(screen, x, y + 32 + descH * 26 + 8, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
    y += rowH;
    lab::box(screen, x, y - 3, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }

  const int side = 56;
  lab::button(screen, lab::rect(x, footY, side, kFootH), "<", ActionPagePrev, 0, false, false);
  lab::button(screen, lab::rect(x + side + 6, footY, side, kFootH), ">", ActionPageNext, 0, false, false);
  std::snprintf(text, sizeof(text), "%d / %d", (page_ + 1) % 1000, pages % 1000);
  lab::caption(screen, x + side * 2 + 12, footY + 8, 78, text, fui::TextAlign::Center);
  if (!showingKept_ && foundOn_[0] != 0) {
    lab::caption(screen, x + side * 2 + 12, footY + 8 + toybox::kTileCut.inkHeight + 6, 78, foundOn_ + 5,
                 fui::TextAlign::Center, fui::Color::DarkGray);
  }
  const int starX = x + inner - 150 - 6 - side;
  fui::ButtonProps star;
  star.label = "";
  star.action = ActionKeptList;
  star.styles = toybox::rowStyles();
  star.radius = lab::kRadius;
  star.state = showingKept_ ? fui::StateSelected : fui::StateNormal;
  screen.button(star, lab::rect(starX, footY, side, kFootH));
  screen.target().bitmap(lab::rect(starX + (side - 24) / 2, footY + (kFootH - 24) / 2, 24, 24),
                         fui::bitmapFromIcon(icon_star_24), fui::BitmapMode::Contain,
                         fui::Paint::solid(showingKept_ ? fui::Color::White : fui::Color::Black));
  lab::button(screen, lab::rect(x + inner - 150, footY, 150, kFootH), "REFRESH", ActionRefresh, 0);
}

void DiscoverActivity::buildPicker(toybox::Screen& screen) {
  static constexpr const char* kTitles[4] = {"WHAT", "LANGUAGE", "STARS", "HOW NEW"};
  lab::chrome(screen, kTitles[static_cast<int>(choice_) % 4]);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int count = dc::kTopics;
  int current = static_cast<int>(filter_.topic);
  if (choice_ == Choice::Language) {
    count = dc::kLanguages;
    current = static_cast<int>(filter_.language);
  } else if (choice_ == Choice::Stars) {
    count = dc::kStarChoices;
    current = static_cast<int>(filter_.stars);
  } else if (choice_ == Choice::Age) {
    count = dc::kAges;
    current = static_cast<int>(filter_.age);
  }
  int y = toybox::kChromeHeight + 16;
  for (int i = 0; i < count; ++i) {
    const char* name = dc::topicName(static_cast<dc::Topic>(i));
    if (choice_ == Choice::Language) name = dc::languageName(static_cast<dc::Language>(i));
    if (choice_ == Choice::Stars) name = dc::starsName(static_cast<dc::Stars>(i));
    if (choice_ == Choice::Age) name = dc::ageName(static_cast<dc::Age>(i));
    lab::button(screen, lab::rect(x, y, inner, 60), name, ActionPick, i, i == current, false);
    y += 60 + 10;
  }
}

void DiscoverActivity::buildDetail(toybox::Screen& screen) {
  lab::chrome(screen, "DISCOVER");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const dc::Repo& r = open_;
  int y = toybox::kChromeHeight + 14;
  char text[120];

  char owner[40];
  std::snprintf(owner, sizeof(owner), "%.*s", static_cast<int>(dc::nameOf(r) - r.fullName - 1), r.fullName);
  for (char* c = owner; *c != 0; ++c) {
    if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
  }
  lab::caption(screen, x, y, inner, owner, fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 8;
  y += toyboxserif::draw(renderer, kHeadFont, 34, x, y, inner, dc::nameOf(r), 2) + 10;
  y += toyboxserif::draw(renderer, kBodyFont, 28, x, y, inner,
                         r.description[0] != 0 ? r.description : "No description.", 7) +
       14;
  lab::ink(screen, x, y, inner, 2);
  y += 14;

  // Three numbers across.
  const char* const names[3] = {"STARS", "FORKS", "OPEN ISSUES"};
  const int32_t values[3] = {r.stars, r.forks, r.issues};
  const int colW = inner / 3;
  for (int i = 0; i < 3; ++i) {
    char value[16];
    dc::formatCount(values[i], value, sizeof(value));
    lab::caption(screen, x + i * colW, y, colW, names[i], fui::TextAlign::Left, fui::Color::DarkGray);
    renderer.drawText(toybox::kDisplayFontId, x + i * colW, y + toybox::kTileCut.inkHeight + 8, value, true);
  }
  y += toybox::kTileCut.inkHeight + 8 + toybox::kDisplayCut.ascender + 16;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 14;

  auto row = [&](const char* name, const char* value) {
    if (value[0] == 0) return;
    lab::caption(screen, x, y + 5, 110, name, fui::TextAlign::Left, fui::Color::DarkGray);
    y += toyboxserif::draw(renderer, kBodyFont, 26, x + 116, y - 6, inner - 116, value, 2) + 6;
  };
  row("LANGUAGE", r.language);
  row("LICENCE", r.license);
  row("CREATED", r.created);
  row("LAST PUSH", r.pushed);
  row("TOPICS", r.topics);
  row("HOMEPAGE", r.homepage);

  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  const int gap = 10;
  const int half = (inner - gap) / 2;
  const bool kept = dc::findSaved(kept_.get(), keptCount_, r.fullName) >= 0;
  lab::button(screen, lab::rect(x, footY, half, kFootH), kept ? "KEPT. LET GO" : "KEEP", ActionKeep, 0, kept);
  lab::button(screen, lab::rect(x + half + gap, footY, half, kFootH), "READ THE README", ActionReadme, 0, !kept);
  (void)text;
}

void DiscoverActivity::buildReadme(toybox::Screen& screen) {
  lab::chrome(screen, "README");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int pages = readmePages();
  const int per = linesPerPage();
  lab::caption(screen, x, toybox::kChromeHeight + 12, inner, open_.fullName, fui::TextAlign::Left,
               fui::Color::DarkGray);
  int y = toybox::kChromeHeight + 40;
  for (int i = readmePage_ * per; i < static_cast<int>(lines_.size()) && i < (readmePage_ + 1) * per; ++i) {
    if (!lines_[static_cast<size_t>(i)].empty()) {
      renderer.drawText(kBodyFont, x, y, lines_[static_cast<size_t>(i)].c_str(), true);
    }
    y += kBodyLine;
  }
  const int barY = screen.device().height - kBottomReserve - 26;
  renderer.fillRectDither(x, barY, inner, 3, Color::LightGray);
  renderer.fillRect(x, barY, inner * (readmePage_ + 1) / pages, 3, true);
  char text[40];
  std::snprintf(text, sizeof(text), "PAGE %d OF %d", (readmePage_ + 1) % 1000, pages % 1000);
  lab::caption(screen, x, barY + 9, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
}

void DiscoverActivity::buildStatus(toybox::Screen& screen) {
  lab::chrome(screen, "DISCOVER");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int y = screen.device().height / 2 - 90;
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), statusHeadline_, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Center);
  fui::TextStyle s;
  s.font = toybox::kTileFont;
  s.align = fui::TextAlign::Center;
  s.maxLines = 4;
  screen.target().text(lab::rect(x, y + toybox::kDisplayCut.inkHeight + 24, inner, toybox::kTileCut.lineHeight * 4),
                       statusDetail_, s);
  if (statusDismissable_) {
    lab::button(screen, lab::rect(x + inner / 4, y + toybox::kDisplayCut.inkHeight + 130, inner / 2, 56), "OK",
                ActionDismiss, 0, true);
  }
}

void DiscoverActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::List:
      buildList(screen);
      break;
    case View::Picker:
      buildPicker(screen);
      break;
    case View::Detail:
      buildDetail(screen);
      break;
    case View::Readme:
      buildReadme(screen);
      break;
    case View::Status:
      buildStatus(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Discover");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
