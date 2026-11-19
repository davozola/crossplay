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
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../components/UITheme.h"
#include "../../network/HttpDownloader.h"
#include "../../util/QrUtils.h"
#include "../Shelf.h"
#include "../hackernews/HackerNewsCore.h"
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
  ActionTab,         // value: Tab
  ActionOwnTerm,     // value: which of the reader's searches
  ActionForgetTerm,  // value: which of the reader's searches
  ActionType,
  ActionSkipSeen,
  ActionWatch,
  ActionQr,
  ActionOpenWatched,  // value: the row on this page
  ActionAddWatched,
  ActionCheck,
  ActionNotes,
  ActionUnwatch,
};

constexpr char kDir[] = "/.crosspoint/discover";
constexpr char kFilterPath[] = "/.crosspoint/discover/filter";
constexpr char kListPath[] = "/.crosspoint/discover/list.txt";
constexpr char kKeptPath[] = "/.crosspoint/discover/kept.txt";
constexpr char kStagedPath[] = "/.crosspoint/discover/answer.tmp";
constexpr char kTermsPath[] = "/.crosspoint/discover/terms.txt";
constexpr char kSeenPath[] = "/.crosspoint/discover/seen";
constexpr char kWatchPath[] = "/.crosspoint/discover/watch.txt";
constexpr char kTabPath[] = "/.crosspoint/discover/tab";
constexpr char kNotesDir[] = "/.crosspoint/discover/notes";
// Watched from the first run: the reader this firmware is built on, and the
// fork it is.
constexpr const char* kWatchedAtFirst[] = {"crosspoint-reader/crosspoint-reader", "ma-r-s/crossplay"};
// A rising search reads further pages until it has this many nobody has seen.
constexpr int kMaxFound = 30;
constexpr int kMaxSearchPages = 3;
constexpr size_t kMaxAnswerBytes = 320u * 1024u;
constexpr size_t kMaxReadmeBytes = 160u * 1024u;
constexpr char kAgent[] = "crossplay-discover";

constexpr int kBottomReserve = 40;
constexpr int kBackSwipeEdge = 28;
constexpr uint8_t kCleanAfterPaints = 8;
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

void upper(char* text) {
  for (char* c = text; *c != 0; ++c) {
    if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
  }
}

void todayText(char* out, const size_t cap) {
  const int32_t day = today();
  if (day == 0) {
    // Checked, on a day the clock could not name.
    std::snprintf(out, cap, "-");
    return;
  }
  int y = 0, m = 0, d = 0;
  dc::civilOfDay(day, y, m, d);
  std::snprintf(out, cap, "%04d-%02d-%02d", y % 10000, m % 100, d % 100);
}

void notesPath(const char* fullName, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%08x.md", kNotesDir, static_cast<unsigned>(dc::nameHash(fullName)));
}

void writeText(const char* path, const std::string& text) {
  Storage.ensureDirectoryExists(kDir);
  HalFile file;
  if (!Storage.openFileForWrite("DISC", path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  file.flush();
}

// What a chip says: the choice in a word.
const char* chipText(const dc::Filter& f, const int choice) {
  static constexpr const char* kLanguages[dc::kLanguages] = {"ANY LANG", "C",  "C++", "RUST",
                                                             "PYTHON",   "GO", "TS",  "ZIG"};
  static constexpr const char* kStars[dc::kStarChoices] = {"10+", "50+", "200+", "1000+", "ANY"};
  static constexpr const char* kAges[dc::kAges] = {"WEEK", "MONTH", "YEAR", "ACTIVE", "RISING", "EVER"};
  switch (choice) {
    case 0:
      if (f.topic == dc::Topic::Own) {
        static char words[16];
        std::snprintf(words, sizeof(words), "%s", f.query);
        upper(words);
        return words;
      }
      return dc::topicName(f.topic);
    case 1:
      return kLanguages[static_cast<int>(f.language) % dc::kLanguages];
    case 2:
      return kStars[static_cast<int>(f.stars) % dc::kStarChoices];
    default:
      return kAges[static_cast<int>(f.age) % dc::kAges];
  }
}

// A list's items as paragraphs of their own, each led by a dash: run together
// they read as one sentence.
void splitItems(std::string& text) {
  std::string out;
  out.reserve(text.size() + 64);
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    size_t first = at;
    while (first < end && (text[first] == ' ' || text[first] == '\t')) ++first;
    const bool item =
        first + 1 < end && (text[first] == '-' || text[first] == '*' || text[first] == '+') && text[first + 1] == ' ';
    if (item) {
      out += "\n- ";
      out.append(text, first + 2, end - first - 2);
    } else {
      out.append(text, at, end - at);
    }
    out.push_back('\n');
    at = end + 1;
  }
  text.swap(out);
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
  found_ = makeUniqueNoThrow<dc::Repo[]>(kMaxFound);
  kept_ = makeUniqueNoThrow<dc::Repo[]>(dc::kMaxSaved);
  seen_ = makeUniqueNoThrow<dc::Seen>();
  watched_ = makeUniqueNoThrow<dc::Watched[]>(dc::kMaxWatched);
  if (!found_ || !kept_ || !seen_ || !watched_) {
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
  saveSeen();
  radio_.reset();
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
  watchedCount_ = 0;
  foundOn_[0] = 0;
  std::string text;
  if (Storage.exists(kFilterPath) && Storage.readFileToString("DISC", kFilterPath, 64, text)) {
    dc::unpackFilter(text, filter_);
  }
  text.clear();
  if (Storage.exists(kListPath) && Storage.readFileToString("DISC", kListPath, 64u * 1024u, text)) {
    // The first line is the day it was fetched.
    const size_t nl = text.find('\n');
    if (nl != std::string::npos && nl < sizeof(foundOn_)) {
      std::memcpy(foundOn_, text.data(), nl);
      foundOn_[nl] = 0;
      foundCount_ = dc::unpackSaved(std::string_view(text).substr(nl + 1), found_.get(), kMaxFound);
    }
  }
  text.clear();
  if (Storage.exists(kKeptPath) && Storage.readFileToString("DISC", kKeptPath, 48u * 1024u, text)) {
    keptCount_ = dc::unpackSaved(text, kept_.get(), dc::kMaxSaved);
  }
  text.clear();
  if (Storage.exists(kTermsPath) && Storage.readFileToString("DISC", kTermsPath, 1024, text)) {
    dc::unpackTerms(text, terms_);
  }
  text.clear();
  if (Storage.exists(kSeenPath) && Storage.readFileToString("DISC", kSeenPath, sizeof(seen_->names), text)) {
    seen_->count = static_cast<int>(text.size() / sizeof(uint32_t));
    std::memcpy(seen_->names, text.data(), static_cast<size_t>(seen_->count) * sizeof(uint32_t));
  }
  text.clear();
  if (!Storage.exists(kWatchPath)) {
    for (const char* name : kWatchedAtFirst) {
      watchedCount_ = dc::toggleWatched(watched_.get(), watchedCount_, name);
    }
  } else if (Storage.readFileToString("DISC", kWatchPath, 8u * 1024u, text)) {
    watchedCount_ = dc::unpackWatched(text, watched_.get(), dc::kMaxWatched);
  }
  text.clear();
  if (Storage.exists(kTabPath) && Storage.readFileToString("DISC", kTabPath, 4, text) && !text.empty() &&
      static_cast<uint8_t>(text[0]) <= static_cast<uint8_t>(Tab::Kept)) {
    tab_ = static_cast<Tab>(text[0]);
  }
}

void DiscoverActivity::saveFilter() { writeText(kFilterPath, dc::packFilter(filter_)); }

void DiscoverActivity::saveList() {
  writeText(kListPath, std::string(foundOn_) + "\n" + dc::packSaved(found_.get(), foundCount_));
}

void DiscoverActivity::saveKept() { writeText(kKeptPath, dc::packSaved(kept_.get(), keptCount_)); }

void DiscoverActivity::saveTerms() { writeText(kTermsPath, dc::packTerms(terms_)); }

void DiscoverActivity::saveSeen() {
  if (!seenDirty_ || !seen_) return;
  seenDirty_ = false;
  writeText(kSeenPath, std::string(reinterpret_cast<const char*>(seen_->names),
                                   static_cast<size_t>(seen_->count) * sizeof(uint32_t)));
}

void DiscoverActivity::saveWatched() { writeText(kWatchPath, dc::packWatched(watched_.get(), watchedCount_)); }

void DiscoverActivity::saveTab() { writeText(kTabPath, std::string(1, static_cast<char>(tab_))); }

// A page of the search holds four, with room for two lines of what each one
// is; a release says it in one, so its page holds five.
int DiscoverActivity::rows() const { return tab_ == Tab::Releases ? 5 : 4; }

int DiscoverActivity::shownCount() const {
  if (tab_ == Tab::Releases) return watchedCount_;
  return tab_ == Tab::Kept ? keptCount_ : foundCount_;
}

const dc::Repo* DiscoverActivity::shown(const int index) const {
  if (tab_ == Tab::Releases || index < 0 || index >= shownCount()) return nullptr;
  // The kept list reads newest first.
  return tab_ == Tab::Kept ? &kept_[static_cast<size_t>(keptCount_ - 1 - index)] : &found_[static_cast<size_t>(index)];
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
  // Connected already, or a network saved to join by itself: the join happens
  // inside the work, with no picker in front of the page.
  if (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone()) {
    step_ = Step::Ready;
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             step_ = Step::Idle;
                             job_ = Job::None;
                             showStatus("NO WI-FI", "WHAT WAS FETCHED LAST AND WHAT YOU KEPT STAY ON THE CARD.", true);
                           } else {
                             step_ = Step::Ready;
                           }
                           requestUpdate();
                         });
}

void DiscoverActivity::runSearch() {
  view_ = statusBack_;
  const int32_t day = today();
  if (day == 0 && filter_.age != dc::Age::Ever) {
    showStatus("NO DATE", "THE SEARCH IS BY DATE AND THE CLOCK IS NOT SET. SET IT IN THE CLOCK APP.", true);
    return;
  }
  saveSeen();
  const bool skipping = dc::skipsSeen(filter_);
  const bool rising = filter_.age == dc::Age::Rising;
  // A page of the answer at a time, so a page the reader has seen all of
  // costs no more memory than one that is all new.
  auto fresh = makeUniqueNoThrow<dc::Repo[]>(kMaxFound);
  auto page = makeUniqueNoThrow<dc::Repo[]>(dc::kPerPage);
  if (!fresh || !page) {
    LOG_ERR("DISC", "OOM: the answer");
    showStatus("NO ROOM", "NOT ENOUGH MEMORY TO READ THE ANSWER.", true);
    return;
  }
  int freshCount = 0;
  int32_t total = 0;
  const int pages = skipping || rising ? kMaxSearchPages : 1;
  for (int at = 1; at <= pages && freshCount < kMaxFound; ++at) {
    char url[460];
    dc::searchUrl(filter_, day, at, url, sizeof(url));
    std::string json;
    if (!download(url, "application/vnd.github+json") ||
        !Storage.readFileToString("DISC", kStagedPath, kMaxAnswerBytes, json)) {
      Storage.remove(kStagedPath);
      // A later page that fails leaves the earlier ones standing.
      if (at > 1) break;
      const int status = HttpDownloader::lastStatus();
      showStatus("NO ANSWER",
                 status == 403 || status == 429
                     ? "GITHUB ALLOWS TEN SEARCHES A MINUTE WITHOUT AN ACCOUNT. WAIT A MINUTE."
                     : "GITHUB DID NOT ANSWER. TRY AGAIN LATER.",
                 true);
      return;
    }
    Storage.remove(kStagedPath);
    int count = dc::parseSearch(json.data(), json.size(), page.get(), dc::kPerPage, total);
    if (count < 0) {
      if (at > 1) break;
      showStatus("NOT AN ANSWER", "WHAT GITHUB SENT BACK WAS NOT A LIST OF REPOSITORIES.", true);
      return;
    }
    const bool last = count < dc::kPerPage;
    if (skipping) count = dc::dropSeen(page.get(), count, *seen_);
    for (int i = 0; i < count && freshCount < kMaxFound; ++i) fresh[static_cast<size_t>(freshCount++)] = page[i];
    if (last) break;
  }
  if (freshCount == 0 && skipping && total > 0) {
    showStatus("NOTHING NEW",
               "EVERY REPOSITORY THIS SEARCH FINDS TODAY HAS BEEN ON A PAGE HERE ALREADY. LOOK AGAIN TOMORROW, OR "
               "TURN SKIPPING OFF UNDER HOW NEW.",
               true);
    return;
  }
  if (rising) dc::rankByPace(fresh.get(), freshCount, day);
  for (int i = 0; i < freshCount; ++i) found_[static_cast<size_t>(i)] = fresh[static_cast<size_t>(i)];
  foundCount_ = freshCount;
  total_ = total;
  todayText(foundOn_, sizeof(foundOn_));
  saveList();
  tab_ = Tab::Discover;
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
  setReading(raw);
  if (lines_.empty()) {
    showStatus("NOTHING TO READ", "THAT README HAS NO PROSE IN IT.", true);
    return;
  }
  readingTitle_ = "README";
  std::snprintf(readingName_, sizeof(readingName_), "%s", open_.fullName);
  readingBack_ = View::Detail;
  view_ = View::Readme;
  cleanNext_ = true;
}

// Markdown into the lines of the reading pages.
void DiscoverActivity::setReading(std::string& raw) {
  stripTags(raw);
  splitItems(raw);
  std::vector<std::string> paragraphs = hn::paragraphsFromMarkdown(raw);
  raw.clear();
  raw.shrink_to_fit();
  lines_.clear();
  const int width = renderer.getScreenWidth() - toybox::kMargin * 2;
  bool wasItem = false;
  for (std::string& p : paragraphs) {
    dc::latinOnly(p.data());
    p.resize(std::strlen(p.c_str()));
    if (p.empty()) continue;
    // The items of one list sit together, with no line between them.
    const bool item = p.size() > 2 && p[0] == '-' && p[1] == ' ';
    if (item && wasItem && !lines_.empty() && lines_.back().empty()) lines_.pop_back();
    wasItem = item;
    toyboxserif::wrap(renderer, kBodyFont, p.c_str(), width, 0, lines_);
    lines_.emplace_back();
    // A README can be a book; forty pages of it is plenty here.
    if (static_cast<int>(lines_.size()) > linesPerPage() * 40) break;
  }
  while (!lines_.empty() && lines_.back().empty()) lines_.pop_back();
  readmePage_ = 0;
}

// One repository of the check. True while there are more to ask about.
bool DiscoverActivity::checkStep() {
  auto skip = [&] {
    while (checkAt_ < watchedCount_ && checkNewOnly_ && watched_[static_cast<size_t>(checkAt_)].checked[0] != 0) {
      ++checkAt_;
    }
  };
  skip();
  if (checkAt_ < watchedCount_) {
    const Checked result = checkOne(watched_[static_cast<size_t>(checkAt_)]);
    if (result == Checked::Failed) ++checkFailed_;
    if (result == Checked::Limited) {
      checkLimited_ = true;
      checkAt_ = watchedCount_;
    }
    ++checkAt_;
    skip();
  }
  if (checkAt_ >= watchedCount_) {
    finishCheck();
    return false;
  }
  std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", watched_[static_cast<size_t>(checkAt_)].fullName);
  upper(statusDetail_);
  return true;
}

DiscoverActivity::Checked DiscoverActivity::checkOne(dc::Watched& watched) {
  char url[160];
  char path[64];
  char day[11];
  todayText(day, sizeof(day));
  notesPath(watched.fullName, path, sizeof(path));
  dc::releaseUrl(watched.fullName, url, sizeof(url));
  std::string json;
  if (download(url, "application/vnd.github+json") &&
      Storage.readFileToString("DISC", kStagedPath, kMaxAnswerBytes, json)) {
    Storage.remove(kStagedPath);
    dc::Watched fetched;
    std::string notes;
    if (!dc::parseRelease(json.data(), json.size(), fetched, notes)) return Checked::Failed;
    dc::noteRelease(watched, fetched, day);
    Storage.ensureDirectoryExists(kNotesDir);
    if (notes.empty()) {
      Storage.remove(path);
    } else {
      writeText(path, notes);
    }
    return Checked::Answered;
  }
  Storage.remove(kStagedPath);
  int status = HttpDownloader::lastStatus();
  if (status == 403 || status == 429) return Checked::Limited;
  if (status != 404) return Checked::Failed;

  // No release: a repository may tag its versions without releasing them.
  dc::tagsUrl(watched.fullName, url, sizeof(url));
  json.clear();
  if (download(url, "application/vnd.github+json") &&
      Storage.readFileToString("DISC", kStagedPath, kMaxAnswerBytes, json)) {
    Storage.remove(kStagedPath);
    dc::Watched fetched;
    if (dc::parseTags(json.data(), json.size(), fetched)) {
      dc::noteRelease(watched, fetched, day);
      Storage.remove(path);
    } else {
      std::snprintf(watched.title, sizeof(watched.title), "No versions published yet.");
      std::snprintf(watched.checked, sizeof(watched.checked), "%s", day);
    }
    return Checked::Answered;
  }
  Storage.remove(kStagedPath);
  status = HttpDownloader::lastStatus();
  if (status == 403 || status == 429) return Checked::Limited;
  if (status != 404) return Checked::Failed;
  std::snprintf(watched.title, sizeof(watched.title), "Not found on GitHub.");
  std::snprintf(watched.checked, sizeof(watched.checked), "%s", day);
  return Checked::Answered;
}

void DiscoverActivity::finishCheck() {
  dc::sortWatched(watched_.get(), watchedCount_);
  saveWatched();
  view_ = View::List;
  tab_ = Tab::Releases;
  page_ = 0;
  cleanNext_ = true;
  if (checkLimited_) {
    showStatus("ASKED TOO OFTEN", "GITHUB ALLOWS SIXTY QUESTIONS AN HOUR WITHOUT AN ACCOUNT. CHECK AGAIN IN A WHILE.",
               true);
  } else if (checkFailed_ > 0) {
    char detail[96];
    std::snprintf(detail, sizeof(detail), "%d OF THEM DID NOT ANSWER. WHAT WAS KNOWN ABOUT THEM STAYS.",
                  checkFailed_ % 100);
    showStatus("NOT ALL ANSWERED", detail, true);
  }
}

// The keyboard: words to search for, or for the watched list a repository by
// its owner and name.
void DiscoverActivity::askWords(const bool forWatch) {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput,
                                                           forWatch ? "OWNER/NAME, OR WORDS" : "SEARCH GITHUB FOR", "",
                                                           forWatch ? sizeof(open_.fullName) - 1 : dc::kQueryCap - 1);
  if (!keyboard) {
    LOG_ERR("DISC", "OOM: the keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this, forWatch](const ActivityResult& result) {
    interactionsReady_ = false;
    cleanNext_ = true;
    if (!result.isCancelled) typed(std::get<KeyboardResult>(result.data).text.c_str(), forWatch);
    requestUpdate();
  });
}

void DiscoverActivity::typed(const char* text, const bool forWatch) {
  char name[sizeof(open_.fullName)];
  if (forWatch && dc::tidyFullName(text, name, sizeof(name))) {
    watch(name, true);
    return;
  }
  char words[dc::kQueryCap];
  dc::tidyQuery(text, words, sizeof(words));
  if (words[0] == 0) return;
  // Words typed for the first time look everywhere: any age, any stars.
  filter_.stars = dc::Stars::Any;
  filter_.age = dc::Age::Ever;
  searchFor(words);
}

void DiscoverActivity::searchFor(const char* words) {
  dc::rememberTerm(terms_, words);
  saveTerms();
  filter_.topic = dc::Topic::Own;
  std::snprintf(filter_.query, sizeof(filter_.query), "%s", terms_.items[0]);
  saveFilter();
  view_ = View::List;
  tab_ = Tab::Discover;
  saveTab();
  start(Job::Search);
}

void DiscoverActivity::watch(const char* fullName, const bool checkNow) {
  const bool there = dc::findWatched(watched_.get(), watchedCount_, fullName) >= 0;
  if (!there && watchedCount_ >= dc::kMaxWatched) {
    showStatus("THE LIST IS FULL", "TWELVE REPOSITORIES CAN BE WATCHED. STOP WATCHING ONE TO MAKE ROOM.", true);
    return;
  }
  if (!there) {
    watchedCount_ = dc::toggleWatched(watched_.get(), watchedCount_, fullName);
    saveWatched();
  }
  if (!checkNow) return;
  view_ = View::List;
  tab_ = Tab::Releases;
  saveTab();
  page_ = 0;
  if (there) return;
  checkAt_ = 0;
  checkFailed_ = 0;
  checkLimited_ = false;
  checkNewOnly_ = true;
  start(Job::Check);
}

void DiscoverActivity::openRelease(const int index) {
  if (index < 0 || index >= watchedCount_) return;
  dc::Watched& w = watched_[static_cast<size_t>(index)];
  openWatched_ = index;
  sinceTag_[0] = 0;
  if (dc::isNew(w)) {
    // Opening it is reading the news.
    std::memcpy(sinceTag_, w.seenTag, sizeof(sinceTag_));
    std::memcpy(w.seenTag, w.tag, sizeof(w.seenTag));
    saveWatched();
  }
  view_ = View::Release;
  cleanNext_ = true;
}

void DiscoverActivity::openNotes() {
  if (openWatched_ < 0 || openWatched_ >= watchedCount_) return;
  const dc::Watched& w = watched_[static_cast<size_t>(openWatched_)];
  char path[64];
  notesPath(w.fullName, path, sizeof(path));
  std::string raw;
  if (!Storage.exists(path) || !Storage.readFileToString("DISC", path, 32u * 1024u, raw)) raw.clear();
  setReading(raw);
  if (lines_.empty()) {
    showStatus("NO NOTES", "THIS VERSION WAS PUBLISHED WITH NO NOTES. THE CODE OPENS ITS PAGE ON A PHONE.", true);
    return;
  }
  readingTitle_ = "RELEASE NOTES";
  std::snprintf(readingName_, sizeof(readingName_), "%s  %s", w.fullName, w.tag);
  readingBack_ = View::Release;
  view_ = View::Readme;
  cleanNext_ = true;
}

void DiscoverActivity::showQr(const char* title, const char* url, const View back) {
  std::snprintf(qrTitle_, sizeof(qrTitle_), "%s", title);
  std::snprintf(qrUrl_, sizeof(qrUrl_), "%s", url);
  qrBack_ = back;
  view_ = View::Qr;
  cleanNext_ = true;
}

void DiscoverActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    statusHeadline_ = job_ == Job::Search ? "ASKING GITHUB" : (job_ == Job::Check ? "CHECKING" : "FETCHING THE README");
    statusDetail_[0] = 0;
    if (job_ == Job::Readme) std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", open_.fullName);
    if (job_ == Job::Search && filter_.topic == dc::Topic::Own) {
      std::snprintf(statusDetail_, sizeof(statusDetail_), "%s", filter_.query);
      upper(statusDetail_);
    }
    if (job_ == Job::Check) std::snprintf(statusDetail_, sizeof(statusDetail_), "FOR NEW VERSIONS");
    // The status frame must be on the panel before the fetch blocks.
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    if (!radio_) {
      radio_.emplace();
      if (!radio_->online()) {
        radio_.reset();
        step_ = Step::Idle;
        job_ = Job::None;
        view_ = statusBack_;
        showStatus("NO WI-FI", "COULD NOT JOIN THE SAVED NETWORK. WHAT WAS FETCHED LAST STAYS ON THE CARD.", true);
        requestUpdate();
        return;
      }
    }
    if (job_ == Job::Check && checkStep()) {
      // The next name is on the panel before its fetch blocks.
      requestUpdateAndWait();
      return;
    }
    const Job job = job_;
    step_ = Step::Idle;
    job_ = Job::None;
    if (job == Job::Search) runSearch();
    if (job == Job::Readme) runReadme();
    radio_.reset();
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
        view_ = readingBack_;
        cleanNext_ = true;
      }
    } else if (view_ == View::List) {
      const int pages = (shownCount() + rows() - 1) / rows();
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
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view_ == View::Status) {
      view_ = statusBack_;
    } else if (view_ == View::Readme) {
      view_ = readingBack_;
    } else if (view_ == View::Qr) {
      view_ = qrBack_;
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
      view_ = readingBack_;
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
      const dc::Repo* repo = shown(page_ * rows() + event.value);
      if (repo == nullptr) return;
      open_ = *repo;
      view_ = View::Detail;
      cleanNext_ = true;
      break;
    }
    case ActionOpenWatched:
      if (tab_ != Tab::Releases) return;
      openRelease(page_ * rows() + event.value);
      break;
    case ActionPagePrev:
      if (page_ == 0) return;
      --page_;
      break;
    case ActionPageNext:
      if ((page_ + 1) * rows() >= shownCount()) return;
      ++page_;
      break;
    case ActionTab:
      if (event.value < 0 || event.value > static_cast<int>(Tab::Kept)) return;
      if (tab_ == static_cast<Tab>(event.value)) return;
      tab_ = static_cast<Tab>(event.value);
      saveTab();
      page_ = 0;
      cleanNext_ = true;
      break;
    case ActionRefresh:
      start(Job::Search);
      break;
    case ActionCheck:
      if (watchedCount_ == 0) return;
      checkAt_ = 0;
      checkFailed_ = 0;
      checkLimited_ = false;
      checkNewOnly_ = false;
      start(Job::Check);
      break;
    case ActionAddWatched:
      askWords(true);
      return;
    case ActionType:
      askWords(false);
      return;
    case ActionChoose:
      if (event.value < 0 || event.value > 3) return;
      choice_ = static_cast<Choice>(event.value);
      view_ = View::Picker;
      cleanNext_ = true;
      break;
    case ActionOwnTerm: {
      if (view_ != View::Picker || event.value < 0 || event.value >= terms_.count) return;
      char words[dc::kQueryCap];
      std::memcpy(words, terms_.items[event.value], sizeof(words));
      searchFor(words);
      break;
    }
    case ActionForgetTerm:
      if (view_ != View::Picker || event.value < 0 || event.value >= terms_.count) return;
      // Forgetting the words being searched for leaves the search with none.
      if (filter_.topic == dc::Topic::Own && std::strcmp(filter_.query, terms_.items[event.value]) == 0) {
        filter_.topic = dc::Topic::Everything;
        filter_.query[0] = 0;
        saveFilter();
      }
      dc::forgetTerm(terms_, event.value);
      saveTerms();
      break;
    case ActionSkipSeen:
      filter_.skipSeen = !filter_.skipSeen;
      saveFilter();
      break;
    case ActionPick: {
      if (view_ != View::Picker || event.value < 0) return;
      if (choice_ == Choice::Topic && event.value < dc::kTopics) {
        filter_.topic = static_cast<dc::Topic>(event.value);
        filter_.query[0] = 0;
      }
      if (choice_ == Choice::Language && event.value < dc::kLanguages) {
        filter_.language = static_cast<dc::Language>(event.value);
      }
      if (choice_ == Choice::Stars && event.value < dc::kStarChoices) filter_.stars = dc::starsAt(event.value);
      if (choice_ == Choice::Age && event.value < dc::kAges) filter_.age = dc::ageAt(event.value);
      saveFilter();
      view_ = View::List;
      tab_ = Tab::Discover;
      // A new filter is a new search.
      start(Job::Search);
      break;
    }
    case ActionKeep:
      keptCount_ = dc::toggleSaved(kept_.get(), keptCount_, open_);
      saveKept();
      break;
    case ActionWatch:
      if (dc::findWatched(watched_.get(), watchedCount_, open_.fullName) >= 0) {
        char path[64];
        notesPath(open_.fullName, path, sizeof(path));
        Storage.remove(path);
        watchedCount_ = dc::toggleWatched(watched_.get(), watchedCount_, open_.fullName);
        saveWatched();
      } else {
        watch(open_.fullName, false);
      }
      break;
    case ActionReadme:
      start(Job::Readme);
      break;
    case ActionQr: {
      char url[160];
      dc::pageUrl(open_.fullName, url, sizeof(url));
      showQr(open_.fullName, url, View::Detail);
      break;
    }
    case ActionNotes:
      openNotes();
      break;
    case ActionUnwatch: {
      if (openWatched_ < 0 || openWatched_ >= watchedCount_) return;
      char name[sizeof(open_.fullName)];
      std::memcpy(name, watched_[static_cast<size_t>(openWatched_)].fullName, sizeof(name));
      char path[64];
      notesPath(name, path, sizeof(path));
      Storage.remove(path);
      watchedCount_ = dc::toggleWatched(watched_.get(), watchedCount_, name);
      saveWatched();
      openWatched_ = -1;
      page_ = 0;
      view_ = View::List;
      cleanNext_ = true;
      break;
    }
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
  lab::chrome(screen, "DISCOVER");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  int y = toybox::kChromeHeight + 8;
  char text[96];

  // Where the reader is: the search, the versions watched for, the ones kept.
  int news = 0;
  for (int i = 0; i < watchedCount_; ++i) news += dc::isNew(watched_[static_cast<size_t>(i)]) ? 1 : 0;
  char releases[16] = "RELEASES";
  if (news > 0) std::snprintf(releases, sizeof(releases), "RELEASES  %d", news % 100);
  const char* const tabNames[3] = {"SEARCH", releases, "KEPT"};
  y = lab::tabs(screen, y, tabNames, 3, static_cast<int>(tab_), ActionTab) + 6;

  if (tab_ == Tab::Discover) {
    // The four choices the search is made of; what it looks for gets the room.
    const int gap = 6;
    const int narrow = (inner - gap * 3) * 21 / 100;
    const int wide = inner - gap * 3 - narrow * 3;
    int chipX = x;
    for (int i = 0; i < 4; ++i) {
      const int chipW = i == 0 ? wide : narrow;
      fui::ButtonProps chip;
      chip.label = chipText(filter_, i);
      chip.action = ActionChoose;
      chip.value = static_cast<int16_t>(i);
      chip.styles = toybox::rowStyles();
      chip.text = toybox::buttonText(screen.theme());
      chip.radius = lab::kRadius;
      chip.minTouchSize = 0;
      screen.button(chip, lab::rect(chipX, y, chipW, 40));
      chipX += chipW + gap;
    }
    y += 40 + 8;
  } else {
    y += 4;
  }

  const int count = shownCount();
  const int pages = count > 0 ? (count + rows() - 1) / rows() : 1;
  if (page_ >= pages) page_ = pages - 1;
  if (count == 0) {
    static constexpr const char* kNothing[3] = {"NOTHING YET", "NOTHING WATCHED", "NOTHING KEPT"};
    static constexpr const char* kHow[3] = {
        "TAP REFRESH FOR WHAT IS RISING ON GITHUB NOW, OR TAP A CHOICE ABOVE TO LOOK FOR SOMETHING OF YOUR OWN.",
        "TAP ADD AND TYPE A REPOSITORY AS OWNER/NAME, OR TAP WATCH ON A REPOSITORY'S PAGE. CHECK THEN SAYS WHICH HAVE "
        "A NEW VERSION.",
        "KEEP A REPOSITORY FROM ITS PAGE AND IT IS LISTED HERE, WITH NO NETWORK NEEDED."};
    const int which = static_cast<int>(tab_) % 3;
    lab::label(screen, lab::rect(x, y + 50, inner, toybox::kDisplayCut.inkHeight), kNothing[which],
               toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    lab::paragraph(screen, x, y + 50 + toybox::kDisplayCut.inkHeight + 22, inner, kHow[which], 5);
  }
  const int rowH = (footY - 4 - y) / rows();
  if (tab_ == Tab::Releases) {
    buildWatchedRows(screen, y, rowH);
  } else {
    buildFoundRows(screen, y, rowH);
  }

  const int side = 56;
  lab::button(screen, lab::rect(x, footY, side, kFootH), "<", ActionPagePrev, 0, false, false);
  lab::button(screen, lab::rect(x + side + 6, footY, side, kFootH), ">", ActionPageNext, 0, false, false);
  std::snprintf(text, sizeof(text), "%d / %d", (page_ + 1) % 1000, pages % 1000);
  const int noteX = x + side * 2 + 12;
  lab::caption(screen, noteX, footY + 8, 78, text, fui::TextAlign::Center);
  if (tab_ == Tab::Discover) {
    // The day of the list, without its year.
    if (std::strlen(foundOn_) == 10) {
      lab::caption(screen, noteX, footY + 8 + toybox::kTileCut.inkHeight + 6, 78, foundOn_ + 5, fui::TextAlign::Center,
                   fui::Color::DarkGray);
    }
    lab::button(screen, lab::rect(x + inner - 150, footY, 150, kFootH), "REFRESH", ActionRefresh, 0);
  } else if (tab_ == Tab::Releases) {
    lab::button(screen, lab::rect(x + inner - 120 - 6 - 90, footY, 90, kFootH), "ADD", ActionAddWatched, 0);
    lab::button(screen, lab::rect(x + inner - 120, footY, 120, kFootH), "CHECK", ActionCheck, 0);
  }
}

void DiscoverActivity::buildFoundRows(toybox::Screen& screen, int y, const int rowH) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const bool pace = tab_ == Tab::Discover && filter_.age == dc::Age::Rising;
  const bool marking = tab_ == Tab::Discover && dc::skipsSeen(filter_);
  const int32_t day = today();
  char text[96];
  for (int row = 0; row < rows(); ++row) {
    const dc::Repo* repo = shown(page_ * rows() + row);
    if (repo == nullptr) break;
    // On a page is seen: the next refresh brings others.
    if (marking && dc::markSeen(*seen_, repo->fullName)) seenDirty_ = true;
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
    const int descH = rowH >= 108 ? 2 : 1;
    toyboxserif::draw(renderer, kBodyFont, 26, x, y + 32, inner,
                      repo->description[0] != 0 ? repo->description : "No description.", descH);
    char owner[40];
    std::snprintf(owner, sizeof(owner), "%.*s", static_cast<int>(dc::nameOf(*repo) - repo->fullName - 1),
                  repo->fullName);
    char when[24];
    std::snprintf(when, sizeof(when), "%s", repo->created);
    if (pace && day > 0) {
      char each[12];
      dc::formatCount(dc::starsPerDay(*repo, day), each, sizeof(each));
      std::snprintf(when, sizeof(when), "+%s A DAY", each);
    }
    std::snprintf(text, sizeof(text), "%s%s%s   %s", owner, repo->language[0] ? "   " : "", repo->language, when);
    upper(text);
    lab::caption(screen, x, y + 32 + descH * 26 + 8, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
    y += rowH;
    lab::box(screen, x, y - 3, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }
}

void DiscoverActivity::buildWatchedRows(toybox::Screen& screen, int y, const int rowH) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  char text[120];
  for (int row = 0; row < rows(); ++row) {
    const int index = page_ * rows() + row;
    if (index >= watchedCount_) break;
    const dc::Watched& w = watched_[static_cast<size_t>(index)];
    fui::ButtonProps open;
    open.label = "";
    open.action = ActionOpenWatched;
    open.value = static_cast<int16_t>(row);
    open.styles = toybox::rowStyles();
    open.styles.normal.borderWidth = 0;
    open.minTouchSize = 0;
    screen.button(open, lab::rect(x, y, inner, rowH - 2));

    // The version on the right, the name beside it, what the release is called under both.
    const char* slash = std::strchr(w.fullName, '/');
    const char* name = slash != nullptr ? slash + 1 : w.fullName;
    const int tagW = w.tag[0] != 0 ? renderer.getTextWidth(toybox::kUiFontId, w.tag) : 0;
    if (tagW > 0 && tagW < inner / 2) renderer.drawText(toybox::kUiFontId, x + inner - tagW, y - 3, w.tag, true);
    toyboxserif::draw(renderer, kBoldFont, 28, x, y + 2, inner - (tagW < inner / 2 ? tagW : 0) - 16, name, 1);
    const char* line = w.title;
    if (line[0] == 0) line = w.tag[0] != 0 ? "A new version." : "Not checked yet. Tap CHECK.";
    if (w.title[0] == 0 && w.tag[0] != 0 && !dc::isNew(w)) line = "The latest version.";
    toyboxserif::draw(renderer, kBodyFont, 26, x, y + 32, inner, line, 1);
    const bool fresh = dc::isNew(w);
    const int pillW = 58;
    std::snprintf(text, sizeof(text), "%.*s%s%s", static_cast<int>(slash != nullptr ? slash - w.fullName : 0),
                  w.fullName, w.published[0] != 0 ? "   " : "", w.published);
    if (fresh) {
      const size_t used = std::strlen(text);
      std::snprintf(text + used, sizeof(text) - used, "   WAS %s", w.seenTag);
    }
    upper(text);
    const int capY = y + 32 + 26 + 8;
    lab::caption(screen, x, capY, inner - (fresh ? pillW + 8 : 0), text, fui::TextAlign::Left, fui::Color::DarkGray);
    if (fresh) {
      lab::box(screen, x + inner - pillW, capY - 6, pillW, toybox::kTileCut.inkHeight + 12,
               fui::Paint::solid(fui::Color::Black), 6);
      lab::caption(screen, x + inner - pillW, capY, pillW, "NEW", fui::TextAlign::Center, fui::Color::White);
    }
    y += rowH;
    lab::box(screen, x, y - 3, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }
}

// What to look for: the six kinds built in, the reader's own searches, and
// the keyboard for a new one.
void DiscoverActivity::buildTopicPicker(toybox::Screen& screen) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int gap = 8;
  const int half = (inner - gap) / 2;
  int y = toybox::kChromeHeight + 14;
  for (int i = 0; i < dc::kTopics; ++i) {
    lab::button(screen, lab::rect(x + (i % 2) * (half + gap), y + (i / 2) * (52 + gap), half, 52),
                dc::topicName(static_cast<dc::Topic>(i)), ActionPick, i, static_cast<int>(filter_.topic) == i, false);
  }
  y += 3 * (52 + gap) + 14;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 14;
  lab::caption(screen, x, y, inner, "YOUR SEARCHES", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 12;
  if (terms_.count == 0) {
    lab::paragraph(
        screen, x, y, inner,
        "TYPE ANYTHING: A WORD, A FEW, OR GITHUB'S OWN TERMS SUCH AS TOPIC:EINK OR USER:ADA. WHAT YOU SEARCH "
        "FOR IS REMEMBERED HERE.",
        4);
  }
  const int forget = 52;
  for (int i = 0; i < terms_.count; ++i) {
    char words[dc::kQueryCap];
    std::snprintf(words, sizeof(words), "%s", terms_.items[i]);
    upper(words);
    const bool current = filter_.topic == dc::Topic::Own && std::strcmp(filter_.query, terms_.items[i]) == 0;
    lab::button(screen, lab::rect(x, y, inner - forget - gap, 48), words, ActionOwnTerm, i, current, false);
    lab::button(screen, lab::rect(x + inner - forget, y, forget, 48), "X", ActionForgetTerm, i, false, false);
    y += 48 + gap;
  }
  const int footY = screen.device().height - kBottomReserve - 56 - 6;
  lab::button(screen, lab::rect(x, footY, inner, 56), "TYPE A NEW SEARCH", ActionType, 0, true, false);
}

void DiscoverActivity::buildPicker(toybox::Screen& screen) {
  static constexpr const char* kTitles[4] = {"WHAT", "LANGUAGE", "STARS", "HOW NEW"};
  lab::chrome(screen, kTitles[static_cast<int>(choice_) % 4]);
  if (choice_ == Choice::Topic) {
    buildTopicPicker(screen);
    return;
  }
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int count = dc::kLanguages;
  int y = toybox::kChromeHeight + 16;
  if (choice_ == Choice::Stars) count = dc::kStarChoices;
  if (choice_ == Choice::Age) count = dc::kAges;
  for (int i = 0; i < count; ++i) {
    const char* name = dc::languageName(static_cast<dc::Language>(i));
    bool current = static_cast<int>(filter_.language) == i;
    if (choice_ == Choice::Stars) {
      name = dc::starsName(dc::starsAt(i));
      current = filter_.stars == dc::starsAt(i);
    }
    if (choice_ == Choice::Age) {
      name = dc::ageName(dc::ageAt(i));
      current = filter_.age == dc::ageAt(i);
    }
    lab::button(screen, lab::rect(x, y, inner, 60), name, ActionPick, i, current, false);
    y += 60 + 10;
  }
  if (choice_ != Choice::Age) return;
  y += 6;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 16;
  lab::caption(screen, x, y + (48 - toybox::kTileCut.inkHeight) / 2, inner - 100, "SKIP WHAT A PAGE HAS SHOWN");
  lab::button(screen, lab::rect(x + inner - 90, y, 90, 48), filter_.skipSeen ? "ON" : "OFF", ActionSkipSeen, 0,
              filter_.skipSeen);
  y += 48 + 12;
  lab::paragraph(
      screen, x, y, inner,
      "RISING IS MADE IN THE LAST TWO MONTHS AND WORKED ON THIS WEEK, IN ORDER OF STARS A DAY. WITH SKIPPING "
      "ON, EACH REFRESH BRINGS REPOSITORIES NOT YET SEEN HERE.",
      4);
}

void DiscoverActivity::buildDetail(toybox::Screen& screen) {
  lab::chrome(screen, "DISCOVER");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const dc::Repo& r = open_;
  int y = toybox::kChromeHeight + 14;

  char owner[40];
  std::snprintf(owner, sizeof(owner), "%.*s", static_cast<int>(dc::nameOf(r) - r.fullName - 1), r.fullName);
  upper(owner);
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

  // Keep it, watch its versions, read it here, or carry it to a phone.
  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  const int gap = 6;
  const bool kept = dc::findSaved(kept_.get(), keptCount_, r.fullName) >= 0;
  const bool watching = dc::findWatched(watched_.get(), watchedCount_, r.fullName) >= 0;
  const int codeW = 62;
  const int each = (inner - codeW - gap * 3) / 3;
  lab::button(screen, lab::rect(x, footY, each, kFootH), kept ? "KEPT" : "KEEP", ActionKeep, 0, kept);
  lab::button(screen, lab::rect(x + each + gap, footY, each, kFootH), watching ? "WATCHED" : "WATCH", ActionWatch, 0,
              watching);
  lab::button(screen, lab::rect(x + (each + gap) * 2, footY, each, kFootH), "README", ActionReadme, 0);
  lab::button(screen, lab::rect(x + inner - codeW, footY, codeW, kFootH), "QR", ActionQr, 0);
}

// Returns where the code for its page goes, or an empty rectangle.
fui::Rect DiscoverActivity::buildRelease(toybox::Screen& screen) {
  lab::chrome(screen, "RELEASE");
  if (openWatched_ < 0 || openWatched_ >= watchedCount_) return lab::rect(0, 0, 0, 0);
  const dc::Watched& w = watched_[static_cast<size_t>(openWatched_)];
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;

  const char* slash = std::strchr(w.fullName, '/');
  char owner[72];
  std::snprintf(owner, sizeof(owner), "%.*s", static_cast<int>(slash != nullptr ? slash - w.fullName : 0), w.fullName);
  upper(owner);
  lab::caption(screen, x, y, inner, owner, fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 8;
  y += toyboxserif::draw(renderer, kHeadFont, 34, x, y, inner, slash != nullptr ? slash + 1 : w.fullName, 2) + 14;
  lab::ink(screen, x, y, inner, 2);
  y += 18;

  if (w.tag[0] == 0) {
    lab::paragraph(screen, x, y, inner,
                   w.title[0] != 0 ? "GITHUB LISTS NO VERSION OF IT. IT IS STILL WATCHED: CHECK ASKS AGAIN."
                                   : "NOT CHECKED YET. GO BACK AND TAP CHECK.",
                   3);
  } else {
    lab::caption(screen, x, y, inner, w.published[0] != 0 ? "LATEST RELEASE" : "LATEST TAG", fui::TextAlign::Left,
                 fui::Color::DarkGray);
    y += toybox::kTileCut.inkHeight + 10;
    // The version, as large as it fits.
    const bool fits = renderer.getTextWidth(toybox::kDisplayFontId, w.tag) <= inner;
    renderer.drawText(fits ? toybox::kDisplayFontId : toybox::kUiFontId, x, y, w.tag, true);
    y += (fits ? toybox::kDisplayCut.ascender : toybox::kUiCut.inkHeight + 8) + 16;
    if (sinceTag_[0] != 0) {
      char since[64];
      std::snprintf(since, sizeof(since), "NEW SINCE %s", sinceTag_);
      upper(since);
      const int pillW = renderer.getTextWidth(toybox::kTileFontId, since) + 24;
      lab::box(screen, x, y, pillW < inner ? pillW : inner, toybox::kTileCut.inkHeight + 14,
               fui::Paint::solid(fui::Color::Black), 6);
      lab::caption(screen, x, y + 7, pillW < inner ? pillW : inner, since, fui::TextAlign::Center, fui::Color::White);
      y += toybox::kTileCut.inkHeight + 14 + 16;
    }
    if (w.title[0] != 0) y += toyboxserif::draw(renderer, kBodyFont, 28, x, y, inner, w.title, 3) + 12;
    lab::ink(screen, x, y, inner, toybox::kHairline);
    y += 14;
    auto row = [&](const char* name, const char* value) {
      if (value[0] == 0 || std::strcmp(value, "-") == 0) return;
      lab::caption(screen, x, y + 5, 110, name, fui::TextAlign::Left, fui::Color::DarkGray);
      y += toyboxserif::draw(renderer, kBodyFont, 26, x + 116, y - 6, inner - 116, value, 1) + 6;
    };
    row("PUBLISHED", w.published);
    row("CHECKED", w.checked);
  }

  const int footY = screen.device().height - kBottomReserve - kFootH - 6;
  const int gap = 6;
  const int stopW = 130;
  lab::button(screen, lab::rect(x, footY, inner - stopW - gap, kFootH), "READ THE NOTES", ActionNotes, 0,
              w.tag[0] != 0);
  lab::button(screen, lab::rect(x + inner - stopW, footY, stopW, kFootH), "UNWATCH", ActionUnwatch, 0);

  // Its page as a code, in the room left over; none when there is no room.
  const int room = footY - 14 - y - 14;
  const int side = room > 210 ? 210 : room;
  if (side < 120) return lab::rect(0, 0, 0, 0);
  const int codeY = y + 14 + (room - side) / 2;
  lab::caption(screen, x, codeY + side / 2 - toybox::kTileCut.inkHeight - 4, inner - side - 16, "ITS PAGE,",
               fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, x, codeY + side / 2 + 4, inner - side - 16, "FOR A PHONE'S CAMERA", fui::TextAlign::Left,
               fui::Color::DarkGray);
  return lab::rect(x + inner - side, codeY, side, side);
}

// The page's address as a code, for the phone that will open it. Returns
// where the code goes; it is drawn after the frame.
fui::Rect DiscoverActivity::buildQr(toybox::Screen& screen) {
  lab::chrome(screen, "ON YOUR PHONE");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  int y = toybox::kChromeHeight + 22;
  y += toyboxserif::draw(renderer, kHeadFont, 34, x, y, inner, qrTitle_, 2) + 18;
  const int side = inner < 340 ? inner : 340;
  const fui::Rect code = lab::rect((width - side) / 2, y, side, side);
  y += side + 22;
  lab::caption(screen, x, y, inner, "POINT A PHONE'S CAMERA AT THE CODE", fui::TextAlign::Center);
  y += toybox::kTileCut.inkHeight + 16;
  // The address itself, for whoever would rather type it.
  const char* shown = std::strncmp(qrUrl_, "https://", 8) == 0 ? qrUrl_ + 8 : qrUrl_;
  std::vector<std::string> lines;
  lines.reserve(3);
  toyboxserif::wrap(renderer, kBodyFont, shown, inner, 3, lines);
  for (const std::string& line : lines) {
    const int w = renderer.getTextWidth(kBodyFont, line.c_str());
    renderer.drawText(kBodyFont, (width - w) / 2, y, line.c_str(), true);
    y += 26;
  }
  return code;
}

void DiscoverActivity::buildReadme(toybox::Screen& screen) {
  lab::chrome(screen, readingTitle_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int pages = readmePages();
  const int per = linesPerPage();
  lab::caption(screen, x, toybox::kChromeHeight + 12, inner, readingName_, fui::TextAlign::Left, fui::Color::DarkGray);
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
    case View::Release: {
      const fui::Rect code = buildRelease(screen);
      if (code.width > 0) {
        char url[200];
        dc::releasePageUrl(watched_[static_cast<size_t>(openWatched_)], url, sizeof(url));
        QrUtils::drawQrCode(renderer, Rect{code.x, code.y, code.width, code.height}, url);
      }
      break;
    }
    case View::Qr: {
      const fui::Rect code = buildQr(screen);
      QrUtils::drawQrCode(renderer, Rect{code.x, code.y, code.width, code.height}, qrUrl_);
      break;
    }
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
