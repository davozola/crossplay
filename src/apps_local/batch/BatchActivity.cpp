#include "BatchActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <cstdio>
#include <cstring>

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
namespace bt = batch;

namespace {

enum : fui::ActionId {
  ActionOpenIssue = 1,  // value: the archive row
  ActionPagePrev,
  ActionPageNext,
  ActionStarFilter,
  ActionSync,
  ActionOpenSection,  // value: the section
  ActionKeepIssue,
  ActionKeepSection,
  ActionReadAll,
  ActionDismiss,
};

constexpr char kDir[] = "/.crosspoint/batch";
constexpr char kIndexPath[] = "/.crosspoint/batch/index.txt";
constexpr char kShelfPath[] = "/.crosspoint/batch/shelf";
constexpr char kStagedPath[] = "/.crosspoint/batch/answer.tmp";
constexpr size_t kMaxIssueBytes = 320u * 1024u;
constexpr size_t kMaxFeedBytes = 480u * 1024u;
constexpr char kAgent[] =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36";

constexpr int kBottomReserve = 40;
constexpr int kBackSwipeEdge = 28;
constexpr uint8_t kCleanAfterPaints = 8;
constexpr int kRowH = 98;
constexpr int kFootH = 48;
constexpr int kBodyLine = 29;

constexpr int kBodyFont = toybox::kReadingSmallFontId;
constexpr int kBoldFont = toybox::kReadingBoldSmallFontId;
constexpr int kHeadFont = toybox::kReadingBoldFontId;

void issuePath(const int number, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/issue-%d.md", kDir, number % 100000);
}

using toyboxserif::wrap;

int drawWrapped(const GfxRenderer& r, const int font, const int lineHeight, const int x, const int y, const int width,
                const char* text, const int maxLines) {
  return toyboxserif::draw(r, font, lineHeight, x, y, width, text, maxLines);
}

bool download(const char* url, const char* accept) {
  Storage.ensureDirectoryExists(kDir);
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kAgent}, {"Accept", accept}};
  const auto result = HttpDownloader::downloadToFile(url, kStagedPath, nullptr, nullptr, "", "", headers);
  if (result != HttpDownloader::OK) {
    LOG_ERR("BATCH", "fetch failed, status %d: %s", HttpDownloader::lastStatus(), url);
    Storage.remove(kStagedPath);
    return false;
  }
  return true;
}

}  // namespace

std::unique_ptr<Activity> BatchActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<BatchActivity>(renderer, mappedInput);
}

void BatchActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadIndex();
  loadShelf();
  view_ = View::Archive;
  archivePage_ = 0;
  cleanNext_ = true;
  requestUpdate();
}

void BatchActivity::onExit() {
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

void BatchActivity::loadIndex() {
  issues_.clear();
  std::string text;
  if (!Storage.exists(kIndexPath) || !Storage.readFileToString("BATCH", kIndexPath, 96u * 1024u, text)) return;
  bt::unpackIndex(text, issues_);
  bt::sortIssues(issues_);
}

void BatchActivity::saveIndex() {
  Storage.ensureDirectoryExists(kDir);
  const std::string text = bt::packIndex(issues_);
  HalFile file;
  if (!Storage.openFileForWrite("BATCH", kIndexPath, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  file.flush();
}

void BatchActivity::loadShelf() {
  shelf_ = bt::Shelf{};
  if (!Storage.exists(kShelfPath)) return;
  HalFile file;
  if (!Storage.openFileForRead("BATCH", kShelfPath, file)) return;
  uint8_t bytes[bt::kShelfBytes];
  if (file.read(bytes, sizeof(bytes)) != static_cast<int>(sizeof(bytes))) return;
  bt::unpackShelf(bytes, sizeof(bytes), shelf_);
}

void BatchActivity::saveShelf() {
  Storage.ensureDirectoryExists(kDir);
  uint8_t bytes[bt::kShelfBytes];
  if (bt::packShelf(shelf_, bytes, sizeof(bytes)) == 0) return;
  HalFile file;
  if (!Storage.openFileForWrite("BATCH", kShelfPath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

bool BatchActivity::cached(const int number) const {
  char path[64];
  issuePath(number, path, sizeof(path));
  return Storage.exists(path);
}

bool BatchActivity::loadIssue(const int number) {
  char path[64];
  issuePath(number, path, sizeof(path));
  std::string raw;
  if (!Storage.readFileToString("BATCH", path, kMaxIssueBytes, raw)) return false;
  const hn::Extracted extracted = hn::splitExtractorResponse(raw);
  raw.clear();
  raw.shrink_to_fit();
  bt::splitIssue(extracted.body, sections_);
  if (sections_.empty()) return false;
  number_ = number;
  // An issue that came with no title in any list takes the page's own.
  for (bt::Issue& issue : issues_) {
    if (issue.number == number && issue.title.empty() && !extracted.title.empty()) {
      issue.title = extracted.title;
      saveIndex();
    }
  }
  return true;
}

void BatchActivity::openIssue(const int number) {
  number_ = number;
  if (cached(number)) {
    if (loadIssue(number)) {
      view_ = View::Issue;
      cleanNext_ = true;
      return;
    }
    // A file that reads as nothing is fetched again rather than kept.
    char path[64];
    issuePath(number, path, sizeof(path));
    Storage.remove(path);
  }
  start(Job::Fetch);
}

int BatchActivity::linesPerPage() const {
  const int top = toybox::kChromeHeight + 44;
  const int bottom = renderer.getScreenHeight() - kBottomReserve - 34;
  return (bottom - top) / kBodyLine;
}

int BatchActivity::pageCount() const {
  const int per = linesPerPage();
  const int pages = (static_cast<int>(lines_.size()) + per - 1) / per;
  return pages < 1 ? 1 : pages;
}

void BatchActivity::layoutSection() {
  lines_.clear();
  if (section_ < 0 || section_ >= static_cast<int>(sections_.size())) return;
  const bt::Section& s = sections_[static_cast<size_t>(section_)];
  const int width = renderer.getScreenWidth() - toybox::kMargin * 2;
  lines_.reserve(static_cast<size_t>(s.words / 7 + 8));
  // The headline first, in bold, marked by a leading tab so the page knows.
  std::vector<std::string> head;
  wrap(renderer, kHeadFont, s.title.c_str(), width, 4, head);
  for (std::string& h : head) lines_.push_back("\t" + h);
  // The headline is set larger: a spare line under it keeps the grid.
  lines_.emplace_back();
  for (const std::string& p : s.paragraphs) {
    wrap(renderer, kBodyFont, p.c_str(), width, 0, lines_);
    lines_.emplace_back();
  }
  while (!lines_.empty() && lines_.back().empty()) lines_.pop_back();
}

void BatchActivity::openSection(const int section, const bool atEnd) {
  if (section < 0 || section >= static_cast<int>(sections_.size())) return;
  section_ = section;
  layoutSection();
  page_ = atEnd ? pageCount() - 1 : 0;
  view_ = View::Read;
  cleanNext_ = true;
  if (!bt::isRead(shelf_, number_)) {
    bt::markRead(shelf_, number_);
    saveShelf();
  }
}

void BatchActivity::turn(const int by) {
  const int next = page_ + by;
  if (next >= 0 && next < pageCount()) {
    page_ = next;
    return;
  }
  // Off the end of a story is the next one; off the last is the issue's index.
  if (by > 0 && section_ + 1 < static_cast<int>(sections_.size())) {
    openSection(section_ + 1, false);
  } else if (by < 0 && section_ > 0) {
    openSection(section_ - 1, true);
  } else {
    view_ = View::Issue;
    cleanNext_ = true;
  }
}

void BatchActivity::showStatus(const char* headline, const char* detail, const bool dismissable) {
  if (view_ != View::Status) statusBack_ = view_;
  statusHeadline_ = headline;
  std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", detail);
  statusDismissable_ = dismissable;
  view_ = View::Status;
}

void BatchActivity::start(const Job job) {
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
        showStatus("NO WI-FI", "ISSUES ALREADY OPENED STAY ON THE CARD AND READ WITHOUT IT.", true);
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

// The newest issues from the newsletter's own front page, and the whole
// archive from the feed the first time, when there is nothing to list yet.
void BatchActivity::runSync() {
  int added = 0;
  bool reached = false;
  char url[160];
  std::string text;
  bt::frontPageUrl(url, sizeof(url));
  if (download(url, "text/plain") && Storage.readFileToString("BATCH", kStagedPath, kMaxIssueBytes, text)) {
    reached = true;
    added += bt::addFromFrontPage(hn::splitExtractorResponse(text).body, issues_);
  }
  if (issues_.size() < 100) {
    text.clear();
    if (download(bt::kFeedUrl, "application/xml") &&
        Storage.readFileToString("BATCH", kStagedPath, kMaxFeedBytes, text)) {
      reached = true;
      added += bt::addFromFeed(text, issues_);
    }
  }
  Storage.remove(kStagedPath);
  bt::sortIssues(issues_);
  if (added > 0) saveIndex();
  view_ = statusBack_;
  archivePage_ = 0;
  if (!reached) {
    showStatus("NO ANSWER", "NEITHER THE NEWSLETTER'S PAGE NOR THE ARCHIVE FEED ANSWERED. TRY AGAIN LATER.", true);
  } else if (added == 0) {
    showStatus("NOTHING NEW", "THE LIST IS UP TO DATE.", true);
  }
}

void BatchActivity::runFetch() {
  char url[160];
  bt::issueUrl(number_, url, sizeof(url));
  char path[64];
  issuePath(number_, path, sizeof(path));
  view_ = statusBack_;
  if (!download(url, "text/plain")) {
    showStatus("NO ANSWER", "THE ISSUE COULD NOT BE FETCHED. TRY AGAIN LATER.", true);
    return;
  }
  Storage.remove(path);
  if (!Storage.rename(kStagedPath, path) || !loadIssue(number_)) {
    Storage.remove(path);
    showStatus("NOT READABLE", "WHAT CAME BACK FOR THAT ISSUE HAD NO STORIES IN IT.", true);
    return;
  }
  view_ = View::Issue;
  cleanNext_ = true;
}

int BatchActivity::listedCount() const {
  if (!favouritesOnly_) return static_cast<int>(issues_.size());
  int count = 0;
  for (const bt::Issue& issue : issues_) count += bt::hasFavourite(shelf_, issue.number) ? 1 : 0;
  return count;
}

int BatchActivity::listed(const int row) const {
  if (!favouritesOnly_) return row >= 0 && row < static_cast<int>(issues_.size()) ? row : -1;
  int seen = 0;
  for (size_t i = 0; i < issues_.size(); ++i) {
    if (!bt::hasFavourite(shelf_, issues_[i].number)) continue;
    if (seen == row) return static_cast<int>(i);
    ++seen;
  }
  return -1;
}

int BatchActivity::rowsPerPage() const {
  const int top = toybox::kChromeHeight + 8;
  const int bottom = renderer.getScreenHeight() - kBottomReserve - kFootH - 14;
  const int rows = (bottom - top) / kRowH;
  return rows < 1 ? 1 : rows;
}

void BatchActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = job_ == Job::Sync ? "LOOKING FOR ISSUES" : "FETCHING THE ISSUE";
    if (job_ == Job::Fetch) {
      std::snprintf(statusDetail_, sizeof(statusDetail_), "ISSUE %d", number_ % 100000);
    } else {
      std::snprintf(statusDetail_, sizeof(statusDetail_), "%s",
                    issues_.size() < 100 ? "THE WHOLE ARCHIVE THE FIRST TIME: A MINUTE OR SO." : "");
    }
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    step_ = Step::Idle;
    const Job job = job_;
    job_ = Job::None;
    if (job == Job::Sync) runSync();
    if (job == Job::Fetch) runFetch();
    requestUpdate();
    return;
  }

  int pressX = 0;
  int pressY = 0;
  if (mappedInput.wasScreenTouchPressed(pressX, pressY)) pressX_ = pressX;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (step_ != Step::Idle) return;
    // In a story, a swipe from mid-page that arrives as Back is the page before.
    if (view_ == View::Read && mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge) {
      turn(-1);
      requestUpdate();
      return;
    }
    if (view_ == View::Archive) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view_ == View::Status) {
      view_ = statusBack_;
    } else if (view_ == View::Read) {
      view_ = View::Issue;
    } else {
      view_ = View::Archive;
    }
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  if (swipe != MappedInputManager::SwipeDir::None && step_ == Step::Idle) {
    const int by = (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up) ? 1 : -1;
    if (view_ == View::Read) {
      turn(by);
      requestUpdate();
      return;
    }
    if (view_ == View::Archive) {
      const int pages = (listedCount() + rowsPerPage() - 1) / rowsPerPage();
      if (archivePage_ + by >= 0 && archivePage_ + by < pages) {
        archivePage_ += by;
        requestUpdate();
      }
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  if (view_ == View::Read && event.action == fui::NO_ACTION) {
    if (tapY < toybox::kChromeHeight + 36) {
      // The band and the line under it lead back to the issue's index.
      view_ = View::Issue;
      cleanNext_ = true;
    } else {
      turn(tapX < renderer.getScreenWidth() / 3 ? -1 : 1);
    }
    requestUpdate();
    return;
  }
  route(event);
}

void BatchActivity::route(const fui::ActionEvent& event) {
  switch (event.action) {
    case ActionOpenIssue: {
      const int index = listed(archivePage_ * rowsPerPage() + event.value);
      if (index < 0) return;
      openIssue(issues_[static_cast<size_t>(index)].number);
      break;
    }
    case ActionPagePrev:
      if (archivePage_ == 0) return;
      --archivePage_;
      break;
    case ActionPageNext:
      if ((archivePage_ + 1) * rowsPerPage() >= listedCount()) return;
      ++archivePage_;
      break;
    case ActionStarFilter:
      favouritesOnly_ = !favouritesOnly_;
      archivePage_ = 0;
      break;
    case ActionSync:
      start(Job::Sync);
      break;
    case ActionOpenSection:
      openSection(event.value, false);
      break;
    case ActionReadAll:
      openSection(0, false);
      break;
    case ActionKeepIssue:
      bt::toggleFavourite(shelf_, number_, bt::kWholeIssue);
      saveShelf();
      break;
    case ActionKeepSection:
      bt::toggleFavourite(shelf_, number_, static_cast<uint8_t>(section_));
      saveShelf();
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

void BatchActivity::buildArchive(toybox::Screen& screen) {
  lab::chrome(screen, favouritesOnly_ ? "THE BATCH: KEPT" : "THE BATCH");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  const int count = listedCount();
  const int per = rowsPerPage();
  const int pages = count > 0 ? (count + per - 1) / per : 1;
  if (archivePage_ >= pages) archivePage_ = pages - 1;

  if (count == 0) {
    const int y = toybox::kChromeHeight + 60;
    lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight),
               favouritesOnly_ ? "NOTHING KEPT" : "NO ISSUES YET", toybox::kDisplayFont, toybox::kDisplayCut,
               fui::TextAlign::Left);
    lab::paragraph(screen, x, y + toybox::kDisplayCut.inkHeight + 22, inner,
                   favouritesOnly_
                       ? "KEEP AN ISSUE OR A STORY WITH ITS STAR AND IT IS LISTED HERE."
                       : "TAP SYNC TO FETCH THE LIST OF ISSUES. EACH ONE YOU OPEN IS THEN KEPT ON THE CARD AND READS "
                         "WITH NO NETWORK.",
                   4);
  }

  int y = toybox::kChromeHeight + 8;
  char text[64];
  for (int row = 0; row < per; ++row) {
    const int index = listed(archivePage_ * per + row);
    if (index < 0) break;
    const bt::Issue& issue = issues_[static_cast<size_t>(index)];
    fui::ButtonProps open;
    open.label = "";
    open.action = ActionOpenIssue;
    open.value = static_cast<int16_t>(row);
    open.styles = toybox::rowStyles();
    open.styles.normal.borderWidth = 0;
    open.minTouchSize = 0;
    screen.button(open, lab::rect(x, y, inner, kRowH - 2));

    // Number and date above the headline; what is known about it on the right.
    std::snprintf(text, sizeof(text), "ISSUE %d   %s", issue.number % 100000, issue.date);
    lab::caption(screen, x, y + 8, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
    const bool read = bt::isRead(shelf_, issue.number);
    const bool here = cached(issue.number);
    lab::caption(screen, x, y + 8, inner - (bt::hasFavourite(shelf_, issue.number) ? 30 : 0),
                 read ? "READ" : (here ? "ON THE CARD" : ""), fui::TextAlign::Right, fui::Color::DarkGray);
    if (bt::hasFavourite(shelf_, issue.number)) {
      screen.target().bitmap(lab::rect(x + inner - 24, y + 2, 24, 24), fui::bitmapFromIcon(icon_star_24),
                             fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::Black));
    }
    char fallback[24];
    std::snprintf(fallback, sizeof(fallback), "Issue %d", issue.number % 100000);
    drawWrapped(renderer, read ? kBodyFont : kBoldFont, 28, x, y + 8 + toybox::kTileCut.inkHeight + 6, inner,
                issue.title.empty() ? fallback : issue.title.c_str(), 2);
    y += kRowH;
    lab::box(screen, x, y - 3, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }

  // The foot: the pages, the favourites and fetching the list.
  const int side = 56;
  lab::button(screen, lab::rect(x, footY, side, kFootH), "<", ActionPagePrev, 0, false, false);
  lab::button(screen, lab::rect(x + side + 6, footY, side, kFootH), ">", ActionPageNext, 0, false, false);
  std::snprintf(text, sizeof(text), "%d / %d", (archivePage_ + 1) % 1000, pages % 1000);
  lab::caption(screen, x + side * 2 + 12, footY + (kFootH - toybox::kTileCut.inkHeight) / 2, 78, text,
               fui::TextAlign::Center);
  const int starX = x + inner - 150 - 6 - side;
  fui::ButtonProps star;
  star.label = "";
  star.action = ActionStarFilter;
  star.styles = toybox::rowStyles();
  star.radius = lab::kRadius;
  star.state = favouritesOnly_ ? fui::StateSelected : fui::StateNormal;
  screen.button(star, lab::rect(starX, footY, side, kFootH));
  screen.target().bitmap(lab::rect(starX + (side - 24) / 2, footY + (kFootH - 24) / 2, 24, 24),
                         fui::bitmapFromIcon(icon_star_24), fui::BitmapMode::Contain,
                         fui::Paint::solid(favouritesOnly_ ? fui::Color::White : fui::Color::Black));
  lab::button(screen, lab::rect(x + inner - 150, footY, 150, kFootH), "SYNC", ActionSync, 0);
}

void BatchActivity::buildIssue(toybox::Screen& screen) {
  lab::chrome(screen, "THE BATCH");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const bt::Issue* issue = nullptr;
  for (const bt::Issue& i : issues_) {
    if (i.number == number_) issue = &i;
  }
  int y = toybox::kChromeHeight + 14;
  char text[72];
  std::snprintf(text, sizeof(text), "ISSUE %d   %s", number_ % 100000, issue != nullptr ? issue->date : "");
  lab::caption(screen, x, y, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
  std::snprintf(text, sizeof(text), "%d MIN READ", bt::minutesFor(bt::wordsIn(sections_)) % 1000);
  lab::caption(screen, x, y, inner, text, fui::TextAlign::Right);
  y += toybox::kTileCut.inkHeight + 10;
  if (issue != nullptr && !issue->title.empty()) {
    y += drawWrapped(renderer, kHeadFont, 34, x, y, inner, issue->title.c_str(), 3) + 8;
  }
  lab::ink(screen, x, y, inner, 2);
  y += 8;

  // The stories, each with how long it takes.
  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  const int count = static_cast<int>(sections_.size());
  int rowH = count > 0 ? (footY - 8 - y) / count : 80;
  if (rowH > 84) rowH = 84;
  if (rowH < 44) rowH = 44;
  for (int i = 0; i < count && y + rowH <= footY - 4; ++i) {
    const bt::Section& s = sections_[static_cast<size_t>(i)];
    fui::ButtonProps open;
    open.label = "";
    open.action = ActionOpenSection;
    open.value = static_cast<int16_t>(i);
    open.styles = toybox::rowStyles();
    open.styles.normal.borderWidth = 0;
    open.minTouchSize = 0;
    screen.button(open, lab::rect(x, y, inner, rowH - 2));
    const bool kept = bt::isFavourite(shelf_, number_, static_cast<uint8_t>(i));
    std::snprintf(text, sizeof(text), "%d MIN", bt::minutesFor(s.words) % 1000);
    lab::caption(screen, x, y + 10, inner - (kept ? 30 : 0), text, fui::TextAlign::Right);
    if (kept) {
      screen.target().bitmap(lab::rect(x + inner - 24, y + 4, 24, 24), fui::bitmapFromIcon(icon_star_24),
                             fui::BitmapMode::Contain, fui::Paint::solid(fui::Color::Black));
    }
    drawWrapped(renderer, kBoldFont, 28, x, y + 4, inner - 96, s.title.c_str(), rowH >= 64 ? 2 : 1);
    y += rowH;
    lab::box(screen, x, y - 3, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }

  const int gap = 10;
  const int half = (inner - gap) / 2;
  const bool kept = bt::isFavourite(shelf_, number_, bt::kWholeIssue);
  lab::button(screen, lab::rect(x, footY, half, kFootH), kept ? "KEPT. LET GO" : "KEEP THE ISSUE", ActionKeepIssue, 0,
              kept);
  lab::button(screen, lab::rect(x + half + gap, footY, half, kFootH), "READ IT ALL", ActionReadAll, 0, !kept);
}

void BatchActivity::buildRead(toybox::Screen& screen) {
  lab::chrome(screen, "THE BATCH");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int pages = pageCount();
  const int per = linesPerPage();
  char text[72];

  // Under the band: where this is, and the star that keeps the story.
  const int subY = toybox::kChromeHeight + 10;
  std::snprintf(text, sizeof(text), "ISSUE %d   STORY %d OF %d", number_ % 100000, (section_ + 1) % 100,
                static_cast<int>(sections_.size()) % 100);
  lab::caption(screen, x, subY + 4, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
  const bool kept = bt::isFavourite(shelf_, number_, static_cast<uint8_t>(section_));
  fui::ButtonProps keep;
  keep.label = "";
  keep.action = ActionKeepSection;
  keep.styles = toybox::rowStyles();
  keep.radius = lab::kRadius;
  keep.minTouchSize = 0;
  keep.state = kept ? fui::StateSelected : fui::StateNormal;
  screen.button(keep, lab::rect(x + inner - 44, subY - 6, 44, 34));
  screen.target().bitmap(lab::rect(x + inner - 34, subY - 1, 24, 24), fui::bitmapFromIcon(icon_star_24),
                         fui::BitmapMode::Contain, fui::Paint::solid(kept ? fui::Color::White : fui::Color::Black));

  int y = toybox::kChromeHeight + 44;
  int wordsLeft = 0;
  for (int i = page_ * per; i < static_cast<int>(lines_.size()); ++i) {
    const std::string& line = lines_[static_cast<size_t>(i)];
    if (i < (page_ + 1) * per) {
      if (!line.empty() && line[0] == '\t') {
        renderer.drawText(kHeadFont, x, y - 4, line.c_str() + 1, true);
      } else if (!line.empty()) {
        renderer.drawText(kBodyFont, x, y, line.c_str(), true);
      }
      y += kBodyLine;
    } else {
      // About nine words a line: close enough for a minute count.
      wordsLeft += line.empty() ? 0 : 9;
    }
  }

  // The foot: how far through the story, and what is left of it.
  const int barY = screen.device().height - kBottomReserve - 26;
  renderer.fillRectDither(x, barY, inner, 3, Color::LightGray);
  renderer.fillRect(x, barY, inner * (page_ + 1) / pages, 3, true);
  std::snprintf(text, sizeof(text), "PAGE %d OF %d", (page_ + 1) % 1000, pages % 1000);
  lab::caption(screen, x, barY + 9, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
  if (page_ + 1 < pages) {
    std::snprintf(text, sizeof(text), "%d MIN LEFT", bt::minutesFor(wordsLeft) % 1000);
  } else {
    std::snprintf(text, sizeof(text), "%s", section_ + 1 < static_cast<int>(sections_.size()) ? "NEXT STORY >" : "END");
  }
  lab::caption(screen, x, barY + 9, inner, text, fui::TextAlign::Right, fui::Color::DarkGray);
}

void BatchActivity::buildStatus(toybox::Screen& screen) {
  lab::chrome(screen, "THE BATCH");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
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

void BatchActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Archive:
      buildArchive(screen);
      break;
    case View::Issue:
      buildIssue(screen);
      break;
    case View::Read:
      buildRead(screen);
      break;
    case View::Status:
      buildStatus(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Batch");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
