#include "WikipediaActivity.h"

#include <Arduino.h>
#include <FontCacheManager.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_random.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../CrossPointSettings.h"
#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/RenderLock.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "../../network/HttpDownloader.h"
#include "../Shelf.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;

namespace {

constexpr const char* kTag = "WIKI";
constexpr const char* kCacheRoot = "/.crosspoint/wikipedia-es";
constexpr const char* kIndexPath = "/.crosspoint/wikipedia-es/index";
constexpr const char* kStatePath = "/.crosspoint/wikipedia-es/state.json";
constexpr const char* kStagedPath = "/.crosspoint/wikipedia-es/answer.tmp";
constexpr const char* kIncomingPath = "/.crosspoint/wikipedia-es/incoming.part";
constexpr int kListMax = 24;
constexpr const char* kMonths[] = {"enero", "febrero", "marzo",      "abril",   "mayo",      "junio",
                                   "julio", "agosto",  "septiembre", "octubre", "noviembre", "diciembre"};
// Wikimedia asks every client to say who it is.
constexpr const char* kAgent = "crossplay-wikipedia/1.0 (e-ink reader; https://github.com/davozola/crossplay)";
// Articles kept on the card. A long one is a few hundred KB with its laid-out
// pages, so this is some tens of MB at the very most.
constexpr int kCachedArticles = 200;
constexpr size_t kMaxSmallAnswer = 32 * 1024;
constexpr fui::KeyboardLayoutId kLayout = fui::KeyboardLayoutId::SpanishEs;
constexpr int16_t kPageSide = 18;
constexpr int16_t kPageTop = 6;
constexpr size_t kBuildMinHeap = 40 * 1024;

bool ensureDir(const char* path) { return Storage.exists(path) || Storage.mkdir(path); }

bool writeFile(const char* path, const std::string& data) {
  HalFile file;
  if (!Storage.openFileForWrite(kTag, path, file)) return false;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(data.data());
  size_t done = 0;
  while (done < data.size()) {
    const size_t want = std::min<size_t>(4096, data.size() - done);
    if (file.write(p + done, want) != want) return false;
    done += want;
  }
  return true;
}

std::string readFile(const char* path, const size_t cap) {
  std::string out;
  if (!Storage.exists(path) || !Storage.readFileToString(kTag, path, cap, out)) out.clear();
  return out;
}

// Removes an article's directory and the files it holds (one level deep is all it has).
void removeArticleCache(const std::string& dir) {
  const char* const names[] = {"/article.html", "/article.part", "/meta", "/sections/0.bin", "/sections/0.bin.part"};
  for (const char* n : names) {
    const std::string p = dir + n;
    if (Storage.exists(p.c_str())) Storage.remove(p.c_str());
  }
  const std::string sections = dir + "/sections";
  if (Storage.exists(sections.c_str())) Storage.rmdir(sections.c_str());
  if (Storage.exists(dir.c_str())) Storage.rmdir(dir.c_str());
}

class FileSink final : public wikies::Sink {
 public:
  explicit FileSink(HalFile& file) : file_(file) {}
  bool write(const char* data, const size_t len) override { return file_.write(data, len) == len; }

 private:
  HalFile& file_;
};

}  // namespace

std::unique_ptr<Activity> WikipediaActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<WikipediaActivity>(renderer, mappedInput);
}

WikipediaActivity::~WikipediaActivity() = default;

void WikipediaActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  keyboardShown_ = false;
  ensureDir("/.crosspoint");
  ensureDir(kCacheRoot);
  loadIndex();
  loadState();
  refreshFooter();
  go(View::Search);
}

void WikipediaActivity::onExit() {
  saveState();
  closeArticle();
  radio_.reset();
  Activity::onExit();
  if (netUsed_ && !devmode::holdsRadio()) {
    // The teardown every Wi-Fi user in this firmware performs.
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

bool WikipediaActivity::skipLoopDelay() {
  return view_ == View::Article && section_ && section_->isBuilding() && !section_->isBuildComplete();
}

void WikipediaActivity::go(const View next) {
  view_ = next;
  interactionsReady_ = false;
  if (next == View::Search) kbGate_.arm();
  requestUpdate();
}

void WikipediaActivity::showNotice(const char* headline, const char* body, const char* actionLabel,
                                   const fui::ActionId action) {
  noticeHead_ = headline;
  noticeBody_ = body;
  noticeAction_ = actionLabel;
  noticeActionId_ = action;
  go(View::Notice);
}

void WikipediaActivity::noticeAbout(const char* headline, const char* fmt, const std::string& title) {
  char body[260];
  snprintf(body, sizeof(body), fmt, title.c_str());
  showNotice(headline, body, "VOLVER", wikiui::ActionBack);
}

// ---------------------------------------------------------------- network

// Every piece of network work comes through here. With a network already up,
// or one saved to join alone, the job is armed at once and its status is the
// only thing shown; the picker appears only on a device that has never joined.
void WikipediaActivity::startJob(const Job job) {
  job_ = job;
  jobArmed_ = false;
  jobReady_ = false;
  if (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone()) {
    jobArmed_ = true;
    return;
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           if (result.isCancelled) {
                             job_ = Job::None;
                             showNotice("SIN WIFI",
                                        "Wikipedia necesita una red. Los artículos que ya abriste siguen en la "
                                        "tarjeta.",
                                        "VOLVER", wikiui::ActionBack);
                           } else {
                             jobArmed_ = true;
                           }
                           requestUpdate();
                         });
}

void WikipediaActivity::runJob() {
  const Job job = job_;
  job_ = Job::None;
  jobArmed_ = false;
  jobReady_ = false;
  netUsed_ = true;
  if (job == Job::Search) {
    runSearch();
  } else if (job == Job::Random) {
    runRandom();
  } else if (job == Job::Open) {
    openLocator(openId_, openPage_, "", openTitle_);
  } else if (job == Job::Feed) {
    runFeed();
  }
}

bool WikipediaActivity::online() {
  radioUsedMs_ = static_cast<uint32_t>(millis());
  if (radio_ && WiFi.status() == WL_CONNECTED) return true;
  radio_.reset();
  radio_.emplace();
  if (!radio_->online()) {
    radio_.reset();
    return false;
  }
  radioUsedMs_ = static_cast<uint32_t>(millis());
  return true;
}

// A list answer (a search, a category, the most viewed pages), by way of the
// card so the request can carry our User-Agent. Only the head of the answer is
// read into memory: the most-viewed one is a thousand rows and the first two
// dozen are all the app shows.
bool WikipediaActivity::fetchSmall(const std::string& url, std::string& body, const size_t cap) {
  body.clear();
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP) {
    LOG_ERR(kTag, "heap too low for TLS: %lu", static_cast<unsigned long>(ESP.getFreeHeap()));
    return false;
  }
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kAgent}, {"Accept", "application/json"}};
  const bool ok =
      HttpDownloader::downloadToFile(url, kStagedPath, nullptr, nullptr, "", "", headers) == HttpDownloader::OK;
  if (!ok) LOG_ERR(kTag, "fetch failed, status %d: %s", HttpDownloader::lastStatus(), url.c_str());
  if (ok) {
    HalFile in;
    auto buffer = makeUniqueNoThrow<char[]>(1024);
    if (buffer && Storage.openFileForRead(kTag, kStagedPath, in)) {
      while (body.size() < cap) {
        const int n = in.read(reinterpret_cast<uint8_t*>(buffer.get()), 1024);
        if (n <= 0) break;
        body.append(buffer.get(), std::min(static_cast<size_t>(n), cap - body.size()));
      }
    }
  }
  if (Storage.exists(kStagedPath)) Storage.remove(kStagedPath);
  radioUsedMs_ = static_cast<uint32_t>(millis());
  return ok && !body.empty();
}

// Fetches one article and leaves it on the card as the reader's XHTML, with
// its title and headings beside it. The answer goes to the card first and is
// converted from there a piece at a time: a long article is hundreds of KB.
// With id 0 the article is asked for by title, and `id` comes back as the
// page's own.
WikipediaActivity::Fail WikipediaActivity::download(uint32_t& id, const std::string& titleHint) {
  if (!online()) return Fail::Network;
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP) {
    LOG_ERR(kTag, "heap too low for TLS: %lu", static_cast<unsigned long>(ESP.getFreeHeap()));
    return Fail::Network;
  }
  const uint32_t start = millis();
  const std::string url = id != 0 ? wikies::articleUrl(id) : wikies::articleUrlByTitle(titleHint);
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kAgent}, {"Accept", "application/json"}};
  if (HttpDownloader::downloadToFile(url, kStagedPath, nullptr, nullptr, "", "", headers) != HttpDownloader::OK) {
    LOG_ERR(kTag, "article %lu: fetch failed, status %d", static_cast<unsigned long>(id), HttpDownloader::lastStatus());
    if (Storage.exists(kStagedPath)) Storage.remove(kStagedPath);
    return Fail::Network;
  }
  radioUsedMs_ = static_cast<uint32_t>(millis());
  const uint32_t fetched = millis();

  if (Storage.exists(kIncomingPath)) Storage.remove(kIncomingPath);
  bool ok = false;
  bool missing = false;
  uint32_t pageId = 0;
  std::string meta;
  {
    HalFile in;
    HalFile outFile;
    auto buffer = makeUniqueNoThrow<char[]>(2048);
    if (!buffer || !Storage.openFileForRead(kTag, kStagedPath, in) ||
        !Storage.openFileForWrite(kTag, kIncomingPath, outFile)) {
      LOG_ERR(kTag, "article %lu: could not stage", static_cast<unsigned long>(id));
    } else {
      FileSink sink(outFile);
      wikies::ExtractReader reader(sink);
      reader.setCredit("Texto de Wikipedia en español, bajo licencia CC BY-SA 4.0.");
      while (true) {
        const int n = in.read(reinterpret_cast<uint8_t*>(buffer.get()), 2048);
        if (n <= 0) break;
        reader.feed(buffer.get(), static_cast<size_t>(n));
      }
      ok = reader.finish();
      outFile.flush();
      missing = reader.missing();
      pageId = reader.pageId();
      meta = reader.title().empty() ? titleHint : reader.title();
      for (const std::string& heading : reader.headings()) {
        meta += '\n';
        meta += heading;
      }
      LOG_INF(kTag, "PERF article %lu: fetch %lums, convert %lums, %u bytes, %u headings",
              static_cast<unsigned long>(pageId), static_cast<unsigned long>(fetched - start),
              static_cast<unsigned long>(millis() - fetched), static_cast<unsigned>(reader.bytesOut()),
              static_cast<unsigned>(reader.headings().size()));
    }
  }
  Storage.remove(kStagedPath);
  if (ok && id == 0) {
    id = pageId;
    if (id == 0) {
      ok = false;
      missing = true;
    }
  }
  if (ok) {
    const std::string dir = dirFor(id);
    removeArticleCache(dir);
    ok = ensureDir(dir.c_str()) && Storage.rename(kIncomingPath, (dir + "/article.html").c_str()) &&
         writeFile((dir + "/meta").c_str(), meta);
    if (!ok) removeArticleCache(dir);
  }
  if (Storage.exists(kIncomingPath)) Storage.remove(kIncomingPath);
  if (!ok) return missing ? Fail::Missing : Fail::Card;
  return Fail::None;
}

void WikipediaActivity::runSearch() {
  refreshLocal();
  searched_ = true;
  offline_ = true;
  std::string body;
  if (online() && fetchSmall(wikies::searchUrl(query_), body, kMaxSmallAnswer)) {
    offline_ = false;
    // The saved titles that matched stay on top; Wikipedia's own follow,
    // without repeating them.
    wikies::parseHits(body.data(), body.size(), wikiui::kMaxResults, results_);
  }
  if (results_.size() > static_cast<size_t>(wikiui::kMaxResults)) results_.resize(wikiui::kMaxResults);
  go(View::Search);
}

void WikipediaActivity::runRandom() {
  std::string body;
  std::vector<wikies::Hit> hits;
  if (!online() || !fetchSmall(wikies::randomUrl(), body, kMaxSmallAnswer) ||
      wikies::parseHits(body.data(), body.size(), 1, hits) < 1) {
    showNotice("NO RESPONDE", "No se ha podido llegar a Wikipedia. Comprueba la wifi y prueba otra vez.", "VOLVER",
               wikiui::ActionBack);
    return;
  }
  history_.clear();
  openLocator(hits[0].id, 0, "", hits[0].title);
}

// ------------------------------------------------------------------ lists

namespace {
const char* feedName(const int feed) {
  static const char* const kNames[] = {"Más leídos ayer", "Más leídos del mes", "Artículos destacados",
                                       "Artículos buenos"};
  return kNames[feed];
}
}  // namespace

void WikipediaActivity::showList(const ListKind kind, const std::string& title) {
  listKind_ = kind;
  listTitle_ = title;
  listFirst_ = 0;
  listShown_ = 0;
  go(View::List);
}

// The ways to find something to read without typing.
void WikipediaActivity::showDiscover() {
  listRows_.clear();
  listHits_.clear();
  for (int feed = 0; feed < 4; ++feed) listRows_.push_back(feedName(feed));
  listRows_.push_back("Un artículo al azar");
  showList(ListKind::Discover, "Descubrir");
}

// The articles on the card, most recently opened first. Read mode opens them;
// in delete mode a tap removes one.
void WikipediaActivity::showSaved(const bool deleting) {
  const int keepFirst = listKind_ == ListKind::Delete && deleting ? listFirst_ : 0;
  listRows_.clear();
  listHits_.clear();
  char line[48];
  if (deleting) {
    snprintf(line, sizeof(line), "BORRAR LOS %d", static_cast<int>(saved_.size()));
    listRows_.push_back(saved_.size() > 1 ? line : "BORRAR TODO");
  } else {
    listRows_.push_back("BORRAR ARTÍCULOS...");
  }
  for (const wikies::Hit& hit : saved_) listRows_.push_back(hit.title);
  showList(deleting ? ListKind::Delete : ListKind::Saved, deleting ? "Toca uno para borrarlo" : "En la tarjeta");
  if (keepFirst < static_cast<int>(listRows_.size())) listFirst_ = keepFirst;
}

void WikipediaActivity::forget(const uint32_t id) {
  removeArticleCache(dirFor(id));
  for (size_t i = 0; i < saved_.size(); ++i) {
    if (saved_[i].id == id) {
      saved_.erase(saved_.begin() + static_cast<long>(i));
      break;
    }
  }
  // It leaves the home too: a row there would only download it again.
  for (size_t i = 0; i < state_.recent.size();) {
    if (state_.recent[i].locator == id) {
      state_.recent.erase(state_.recent.begin() + static_cast<long>(i));
    } else {
      ++i;
    }
  }
  if (state_.current.locator == id) {
    state_.current = wikipedia::RecentEntry{};
    state_.currentPage = 0;
  }
}

void WikipediaActivity::closeList() {
  if (listKind_ == ListKind::Delete) {
    showSaved(false);
  } else if (listKind_ == ListKind::Hits) {
    showDiscover();
  } else {
    go(View::Search);
  }
}

void WikipediaActivity::listTapped(const int row) {
  if (row < 0 || row >= static_cast<int>(listRows_.size())) return;
  switch (listKind_) {
    case ListKind::Discover:
      if (row < 4) {
        feed_ = static_cast<Feed>(row);
        startJob(Job::Feed);
      } else {
        startJob(Job::Random);
      }
      return;
    case ListKind::Hits:
      if (row < static_cast<int>(listHits_.size())) {
        const wikies::Hit hit = listHits_[row];
        history_.clear();
        fromList_ = true;
        open(hit.id, 0, hit.title);
      }
      return;
    case ListKind::Saved:
      if (row == 0) {
        if (!saved_.empty()) showSaved(true);
      } else if (row - 1 < static_cast<int>(saved_.size())) {
        const wikies::Hit hit = saved_[row - 1];
        history_.clear();
        fromList_ = true;
        open(hit.id, 0, hit.title);
      }
      return;
    case ListKind::Delete:
      if (row == 0) {
        char body[160];
        snprintf(body, sizeof(body),
                 "Se borrarán de la tarjeta los %d artículos guardados. Para dejarlos, vuelve atrás.",
                 static_cast<int>(saved_.size()));
        showNotice("BORRAR TODO", body, "BORRAR", wikiui::ActionRetry);
      } else if (row - 1 < static_cast<int>(saved_.size())) {
        forget(saved_[row - 1].id);
        saveIndex();
        saveState();
        refreshFooter();
        if (saved_.empty()) {
          go(View::Search);
        } else {
          showSaved(true);
        }
      }
      return;
  }
}

void WikipediaActivity::forgetAll() {
  showNotice("BORRANDO", "Un momento.", nullptr, 0);
  requestUpdateAndWait();
  while (!saved_.empty()) forget(saved_.back().id);
  state_ = wikipedia::State{};
  saveIndex();
  saveState();
  refreshFooter();
  go(View::Search);
}

// One of the lists Wikipedia keeps: the most viewed pages of yesterday or of
// last month, and the articles its editors have marked as their best.
void WikipediaActivity::runFeed() {
  const auto fail = [this](const char* headline, const char* body) {
    showNotice(headline, body, "VOLVER", wikiui::ActionBack);
  };
  if (!online()) {
    fail("NO RESPONDE", "No se ha podido llegar a Wikipedia. Comprueba la wifi y prueba otra vez.");
    return;
  }
  std::vector<wikies::Hit> hits;
  std::string body;
  char title[64];
  snprintf(title, sizeof(title), "%s", feedName(static_cast<int>(feed_)));
  if (feed_ == Feed::TopDay || feed_ == Feed::TopMonth) {
    time_t now = time(nullptr);
    if (now < 1700000000 && halClock.syncFromNTP()) now = time(nullptr);
    if (now < 1700000000) {
      fail("SIN HORA", "Hace falta la hora del dispositivo para saber qué día pedir.");
      return;
    }
    // The counts are kept by UTC day and arrive some hours after it ends, so
    // the newest one may not be there yet: step back until one answers.
    for (int back = 1; back <= 3 && hits.empty(); ++back) {
      struct tm when{};
      int day = 0;
      if (feed_ == Feed::TopDay) {
        const time_t then = now - static_cast<time_t>(back) * 86400;
        gmtime_r(&then, &when);
        day = when.tm_mday;
        snprintf(title, sizeof(title), "Más leídos el %d de %s", when.tm_mday, kMonths[when.tm_mon]);
      } else {
        gmtime_r(&now, &when);
        when.tm_mon -= back;
        if (when.tm_mon < 0) {
          when.tm_mon += 12;
          when.tm_year -= 1;
        }
        snprintf(title, sizeof(title), "Más leídos en %s", kMonths[when.tm_mon]);
      }
      if (fetchSmall(wikies::topUrl(when.tm_year + 1900, when.tm_mon + 1, day), body, 12 * 1024)) {
        wikies::parseTop(body.data(), body.size(), kListMax, hits);
      }
    }
  } else {
    // A different stretch of the list each time: it starts at a random pair
    // of letters and, near the end of the alphabet, wraps to the beginning.
    const char* category = feed_ == Feed::Featured ? "Wikipedia:Artículos destacados" : "Wikipedia:Artículos buenos";
    const uint32_t r = esp_random();
    const char start[3] = {static_cast<char>('A' + r % 26), static_cast<char>('A' + (r >> 8) % 26), 0};
    if (fetchSmall(wikies::categoryUrl(category, start), body, kMaxSmallAnswer)) {
      wikies::parseHits(body.data(), body.size(), kListMax, hits);
    }
    if (hits.size() < 8 && fetchSmall(wikies::categoryUrl(category, ""), body, kMaxSmallAnswer)) {
      wikies::parseHits(body.data(), body.size(), kListMax - static_cast<int>(hits.size()), hits);
    }
  }
  if (hits.empty()) {
    fail("NO RESPONDE", "Wikipedia no ha devuelto esa lista. Prueba otra vez dentro de un rato.");
    return;
  }
  listHits_ = std::move(hits);
  listRows_.clear();
  for (const wikies::Hit& hit : listHits_) listRows_.push_back(hit.title);
  showList(ListKind::Hits, title);
}

// ---------------------------------------------------------------- the card

std::string WikipediaActivity::dirFor(const uint32_t id) const {
  return std::string(kCacheRoot) + "/" + std::to_string(id);
}

bool WikipediaActivity::isCached(const uint32_t id) const {
  const std::string dir = dirFor(id);
  return Storage.exists((dir + "/article.html").c_str()) && Storage.exists((dir + "/meta").c_str());
}

// The title and headings of the article in cacheDir_.
bool WikipediaActivity::loadMeta() {
  article_ = Doc{};
  const std::string meta = readFile((cacheDir_ + "/meta").c_str(), 16 * 1024);
  if (meta.empty()) return false;
  size_t pos = 0;
  bool first = true;
  while (pos <= meta.size()) {
    const size_t nl = meta.find('\n', pos);
    const std::string line = meta.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    if (first) {
      article_.title = line;
      first = false;
    } else if (!line.empty()) {
      article_.headings.push_back(line);
    }
    if (nl == std::string::npos) break;
    pos = nl + 1;
  }
  HalFile file;
  if (!Storage.openFileForRead(kTag, (cacheDir_ + "/article.html").c_str(), file)) return false;
  article_.bytes = file.size();
  return !article_.title.empty() && article_.bytes > 0;
}

// "id<TAB>title" a line, most recently opened first.
void WikipediaActivity::loadIndex() {
  saved_.clear();
  const std::string text = readFile(kIndexPath, 32 * 1024);
  size_t pos = 0;
  while (pos < text.size()) {
    const size_t nl = text.find('\n', pos);
    const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    const size_t tab = line.find('\t');
    if (tab != std::string::npos && tab > 0 && tab + 1 < line.size()) {
      wikies::Hit hit;
      hit.id = static_cast<uint32_t>(strtoul(line.substr(0, tab).c_str(), nullptr, 10));
      hit.title = line.substr(tab + 1);
      if (hit.id != 0) saved_.push_back(std::move(hit));
    }
    if (nl == std::string::npos) break;
    pos = nl + 1;
  }
}

void WikipediaActivity::saveIndex() const {
  std::string text;
  for (const wikies::Hit& hit : saved_) {
    text += std::to_string(hit.id);
    text += '\t';
    text += hit.title;
    text += '\n';
  }
  writeFile(kIndexPath, text);
}

void WikipediaActivity::touchIndex(const uint32_t id, const std::string& title) {
  for (size_t i = 0; i < saved_.size(); ++i) {
    if (saved_[i].id == id) {
      saved_.erase(saved_.begin() + static_cast<long>(i));
      break;
    }
  }
  wikies::Hit hit;
  hit.id = id;
  hit.title = title;
  saved_.insert(saved_.begin(), std::move(hit));
  while (saved_.size() > static_cast<size_t>(kCachedArticles)) {
    removeArticleCache(dirFor(saved_.back().id));
    saved_.pop_back();
  }
  saveIndex();
  refreshFooter();
}

void WikipediaActivity::loadState() {
  state_ = wikipedia::State{};
  const std::string json = readFile(kStatePath, 8 * 1024);
  if (!json.empty()) state_.fromJson(json.data(), json.size());
}

void WikipediaActivity::saveState() { writeFile(kStatePath, state_.toJson()); }

void WikipediaActivity::refreshFooter() {
  char line[64];
  if (saved_.empty()) {
    snprintf(line, sizeof(line), "WIKIPEDIA EN ESPANOL");
  } else {
    snprintf(line, sizeof(line), "%d %s EN LA TARJETA", static_cast<int>(saved_.size()),
             saved_.size() == 1 ? "ARTICULO" : "ARTICULOS");
  }
  footer_ = line;
}

// ---------------------------------------------------------------- search

// The saved articles whose title has the words typed: what the card can answer
// while typing, before Wikipedia is asked, and all there is with no Wi-Fi.
void WikipediaActivity::refreshLocal() {
  results_.clear();
  searched_ = false;
  offline_ = false;
  if (query_.empty()) return;
  for (const wikies::Hit& hit : saved_) {
    if (static_cast<int>(results_.size()) >= wikiui::kMaxResults) break;
    if (wikies::titleMatches(hit.title, query_)) results_.push_back(hit);
  }
}

void WikipediaActivity::handleKey(const int value) {
  const fui::KeyboardLayout& layout = fui::builtinKeyboardLayout(kLayout, shifted_, symbols_, true, false);
  switch (value) {
    case fui::QWERTY_KEY_SHIFT:
      shifted_ = !shifted_;
      break;
    case fui::QWERTY_KEY_MODE:
      symbols_ = !symbols_;
      shifted_ = false;
      break;
    case fui::QWERTY_KEY_LANG:
      return;
    case fui::QWERTY_KEY_ENTER:
      // IR means "ask Wikipedia": the keyboard goes down and the matches get
      // the panel.
      keyboardShown_ = false;
      if (!query_.empty()) startJob(Job::Search);
      break;
    case fui::QWERTY_KEY_BACKSPACE: {
      if (query_.empty()) break;
      size_t pos = query_.size() - 1;
      while (pos > 0 && (static_cast<uint8_t>(query_[pos]) & 0xC0) == 0x80) --pos;
      query_.erase(pos);
      refreshLocal();
      break;
    }
    default: {
      const char* out = fui::keyboardOutputFor(layout, static_cast<int16_t>(value));
      if (!out) return;
      if (query_.size() + strlen(out) > 120) return;
      query_ += out;
      if (shifted_ && !symbols_) shifted_ = false;
      refreshLocal();
      break;
    }
  }
  requestUpdate();
}

// An article on the card opens at once; any other is a job for the network.
// A hit from a list may carry only a title (id 0): the card is asked by name.
void WikipediaActivity::open(uint32_t id, const int page, const std::string& title) {
  if (id == 0) id = savedIdFor(title);
  if (id != 0 && isCached(id)) {
    openLocator(id, page, "", title);
    return;
  }
  openId_ = id;
  openPage_ = page;
  openTitle_ = title;
  startJob(Job::Open);
}

uint32_t WikipediaActivity::savedIdFor(const std::string& title) const {
  for (const wikies::Hit& hit : saved_) {
    if (hit.title == title) return hit.id;
  }
  return 0;
}

fui::Rect WikipediaActivity::keyboardRect() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const fui::KeyboardLayout& layout = fui::builtinKeyboardLayout(kLayout, shifted_, symbols_, true, false);
  const int rows = layout.rowCount;
  const int gap = metrics.keyboardKeySpacing;
  const int height = rows * metrics.keyboardKeyHeight + (rows > 1 ? (rows - 1) * gap : 0);
  const int width = pageWidth * metrics.keyboardWidthPercent / 100;
  const int x = (pageWidth - width) / 2;
  const int y = pageHeight - 8 - height;
  return fui::Rect{static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(width),
                   static_cast<int16_t>(height)};
}

// The keyboard has its own table: it registers more hit rects than a toybox
// screen holds, and it is routed first.
void WikipediaActivity::drawKeyboard() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  kbInteractions_.beginPublishCycle();
  fui::GfxRendererTarget target(renderer);
  target.setFont(fui::GfxRendererTarget::FONT_SMALL, SMALL_FONT_ID);
  target.setFont(fui::GfxRendererTarget::FONT_BODY, UI_12_FONT_ID);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  fui::Frame<56> frame(target, device, noInput, kbInteractions_);

  fui::KeyboardProps props;
  const fui::KeyboardLayout& layout = fui::builtinKeyboardLayout(kLayout, shifted_, symbols_, true, false);
  props.layout = &layout;
  props.keyAction = wikiui::ActionKey;
  props.okLabel = "IR";
  props.shiftLabel = tr(STR_KEY_SHIFT);
  props.modeLabel = symbols_ ? tr(STR_KEY_MODE_ABC) : tr(STR_KEY_MODE_SYMBOLS);
  props.inputMask = static_cast<uint16_t>(fui::InputTouch);
  props.selectedIndex = -1;
  props.labelText.font = fui::GfxRendererTarget::FONT_BODY;
  props.altText.font = fui::GfxRendererTarget::FONT_SMALL;
  props.gap = static_cast<int16_t>(metrics.keyboardKeySpacing);
  props.padding = fui::Insets{0, 0, 0, 0};
  const fui::Rect kb = keyboardRect();
  props.bottomHitOverflow = static_cast<int16_t>(renderer.getScreenHeight() - kb.bottom());
  fui::keyboard(frame, kb, props);
  kbInteractions_.publish();
  kbGate_.markBuilt();
}

// --------------------------------------------------------------- article

// The article's own band, with the title and an empty page, while the article
// is fetched and laid out. Where the panel can refresh by itself the work runs
// under its waveform; where it cannot, the frame is simply shown first.
void WikipediaActivity::paintOpeningCue(const std::string& title, const char* status) {
  renderer.clearScreen();
  const int readerFont = SETTINGS.getReaderFontId();
  const int readerSmall =
      SETTINGS.fontFamily == CrossPointSettings::NOTOSANS ? NOTOSANS_12_FONT_ID : NOTOSERIF_12_FONT_ID;
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::Faces{readerSmall, readerFont, readerFont});
  const fui::InputSnapshot noInput{};
  // Its own interactions: the cue is not touchable, and the render that
  // follows publishes the real ones a moment later.
  toybox::Interactions untouched;
  toybox::Frame frame(target, target.deviceContext(), noInput, untouched);
  toybox::Screen screen(frame);
  wikiui::ArticleChromeModel chrome;
  chrome.title = title.c_str();
  chrome.contents = false;  // nothing to jump to until the layout exists
  chrome.back = false;
  cueBody_ = wikiui::buildArticleChrome(screen, chrome);
  wikiui::ArticleFooterModel foot;
  foot.left = status;
  wikiui::buildArticleFooter(screen, foot);
  if (renderer.supportsAsyncRefresh()) {
    renderer.displayBufferAsync();
    cuePainted_ = true;
  } else {
    renderer.displayBuffer();
  }
}

bool WikipediaActivity::openLocator(uint32_t id, const int page, const std::string& anchor, const std::string& title) {
  // The render task may be mid-layout on the old section; the lock is what the
  // reader takes before touching its own.
  RenderLock lock;
  closeArticle();
  cacheDir_.clear();
  const auto fail = [this](const char* headline, const char* body) {
    if (cuePainted_) {
      // The cue is on the waveform and the notice is about to draw over it.
      renderer.waitRefreshComplete();
      cuePainted_ = false;
    }
    showNotice(headline, body, "VOLVER", wikiui::ActionBack);
    return false;
  };
  if (id == 0 || !isCached(id)) {
    paintOpeningCue(title, "Descargando");
    const Fail why = download(id, title);
    if (why == Fail::Network) {
      return fail("NO RESPONDE", "No se ha podido llegar a Wikipedia. Comprueba la wifi y prueba otra vez.");
    }
    if (why == Fail::Missing) return fail("NO EXISTE", "Wikipedia no tiene texto para este artículo.");
    if (why == Fail::Card) return fail("LO SIENTO", "No se ha podido guardar el artículo en la tarjeta.");
  }
  cacheDir_ = dirFor(id);
  if (!loadMeta()) {
    removeArticleCache(cacheDir_);
    return fail("LO SIENTO", "No se ha podido abrir este artículo.");
  }
  touchIndex(id, article_.title);
  locator_ = id;
  targetPage_ = page;
  pendingAnchor_ = anchor;
  buildFailed_ = false;
  headingPages_.assign(article_.headings.size(), -1);
  headingPagesFinal_ = false;
  layOutUnderCue();
  state_.touch({article_.title, id});
  state_.current = {article_.title, id};
  state_.currentPage = page;
  saveState();
  go(View::Article);
  return true;
}

void WikipediaActivity::closeArticle() {
  section_.reset();
  article_ = Doc{};
}

// Lays out up to the page or anchor wanted, creating the section on first use.
// Called from render(), which owns the geometry the spec is made from.
bool WikipediaActivity::ensureBuilt() {
  if (!section_) return false;
  while (section_->isBuilding() && !section_->isBuildComplete()) {
    if (!pendingAnchor_.empty()) {
      if (section_->findAnchor(pendingAnchor_)) break;
    } else if (section_->pageCount > targetPage_) {
      break;
    }
    if (!section_->buildSomeMore(8)) {
      buildFailed_ = true;
      return false;
    }
  }
  if (!pendingAnchor_.empty()) {
    if (const auto p = section_->findAnchor(pendingAnchor_)) targetPage_ = *p;
    pendingAnchor_.clear();
  }
  if (section_->pageCount > 0 && targetPage_ >= section_->pageCount) targetPage_ = section_->pageCount - 1;
  if (targetPage_ < 0) targetPage_ = 0;
  section_->currentPage = targetPage_;
  return true;
}

void WikipediaActivity::turnPage(const int delta) {
  RenderLock lock;
  if (!section_) return;
  int next = section_->currentPage + delta;
  if (next < 0) next = 0;
  if (section_->isBuildComplete() && next >= section_->pageCount) return;
  targetPage_ = next;
  state_.currentPage = next;
  requestUpdate();
}

void WikipediaActivity::pushHistory() {
  if (!section_) return;
  if (static_cast<int>(history_.size()) >= kHistoryDepth) history_.erase(history_.begin());
  history_.push_back({locator_, section_->currentPage, article_.title});
}

void WikipediaActivity::popHistory() {
  if (history_.empty()) {
    closeArticle();
    go(View::Search);
    return;
  }
  const Visit back = history_.back();
  history_.pop_back();
  open(back.id, back.page, back.title);
}

void WikipediaActivity::refreshHeadingPages() {
  if (!section_ || headingPagesFinal_) return;
  bool all = true;
  for (size_t i = 0; i < headingPages_.size(); ++i) {
    if (headingPages_[i] >= 0) continue;
    const std::string anchor = "s" + std::to_string(i + 1);
    if (const auto p = section_->findAnchor(anchor)) {
      headingPages_[i] = *p;
    } else {
      all = false;
    }
  }
  headingPagesFinal_ = all && section_->isBuildComplete();
}

int WikipediaActivity::headingForPage(const int page) const {
  int best = -1;
  for (size_t i = 0; i < headingPages_.size(); ++i) {
    if (headingPages_[i] >= 0 && headingPages_[i] <= page) best = static_cast<int>(i);
  }
  return best;
}

void WikipediaActivity::ensureSection(const fui::Rect& body) {
  if (section_) return;
  const uint16_t viewportWidth = static_cast<uint16_t>(body.width - kPageSide * 2);
  const uint16_t viewportHeight = static_cast<uint16_t>(body.height - kPageTop);
  ReaderRenderSpec spec = SETTINGS.readerRenderSpec(viewportWidth, viewportHeight);
  spec.embeddedStyle = false;
  // Ragged right, whatever the reader's setting for books: encyclopedia prose
  // is name-dense on a narrow measure and justified lines open rivers.
  // Hyphenation stays on; it tidies a ragged edge too.
  spec.paragraphAlignment = CrossPointSettings::LEFT_ALIGN;
  spec.hyphenationEnabled = true;
  // In a long article every section starts a fresh page.
  std::vector<std::string> anchors;
  if (article_.bytes > kFreshPageBytes) {
    anchors.reserve(article_.headings.size());
    for (size_t i = 0; i < article_.headings.size(); ++i) anchors.push_back("s" + std::to_string(i + 1));
  }
  section_ = makeUniqueNoThrow<Section>(cacheDir_ + "/article.html", cacheDir_, 0, renderer, std::move(anchors), false);
  if (!section_) {
    LOG_ERR(kTag, "OOM: Section");
    buildFailed_ = true;
  } else if (!section_->loadSectionFile(spec) || section_->isPartial()) {
    if (!section_->startBuild(spec, nullptr)) {
      LOG_ERR(kTag, "layout could not start");
      buildFailed_ = true;
    }
  }
}

// The panel is busy for most of a second after displayBufferAsync() returns;
// the first page is laid out inside that window.
void WikipediaActivity::layOutUnderCue() {
  if (!cuePainted_ || buildFailed_) return;
  ensureSection(cueBody_);
  if (!buildFailed_) ensureBuilt();
}

void WikipediaActivity::renderArticle(toybox::Screen& screen) {
  wikiui::ArticleChromeModel model;
  model.title = article_.title.c_str();
  model.contents = !article_.headings.empty();
  const fui::Rect body = wikiui::buildArticleChrome(screen, model);

  const int pageX = body.x + kPageSide;
  const int pageY = body.y + kPageTop;

  ensureSection(body);
  if (!buildFailed_) ensureBuilt();

  if (section_ && !buildFailed_ && section_->pageCount > 0) {
    auto page = section_->loadPage(section_->currentPage);
    if (page) {
      const int fontId = SETTINGS.getReaderFontId();
      auto* fcm = renderer.getFontCacheManager();
      auto scope = fcm->createPrewarmScope();
      page->render(renderer, fontId, pageX, pageY);
      scope.endScanAndPrewarm();
      page->render(renderer, fontId, pageX, pageY);
    }
  }
  // The footer, once the layout has said which page this is.
  char left[32] = "";
  char right[96] = "";
  // A section read back from its cache is complete without ever having built.
  const bool total = section_ && !buildFailed_ && (!section_->isBuilding() || section_->isBuildComplete());
  if (section_ && !buildFailed_) {
    refreshHeadingPages();
    const int shown = section_->currentPage + 1;
    if (total) {
      snprintf(left, sizeof(left), "%d de %d", shown, section_->pageCount);
    } else {
      snprintf(left, sizeof(left), "pág. %d", shown);
    }
    const int h = headingForPage(section_->currentPage);
    if (h >= 0 && h < static_cast<int>(article_.headings.size())) {
      snprintf(right, sizeof(right), "%s", article_.headings[h].c_str());
    }
  }
  wikiui::ArticleFooterModel footer;
  footer.left = left;
  footer.right = right;
  if (section_ && !buildFailed_) {
    footer.page = section_->currentPage + 1;
    footer.total = total ? section_->pageCount : 0;
  }
  wikiui::buildArticleFooter(screen, footer);
}

// ------------------------------------------------------------------ loop

void WikipediaActivity::routeAction(const int action, const int value) {
  switch (action) {
    case wikiui::ActionDiscover:
      showDiscover();
      return;
    case wikiui::ActionSaved:
      if (!saved_.empty()) showSaved(false);
      return;
    case wikiui::ActionRetry:
      // The one confirmation the app asks for: emptying the card.
      if (listKind_ == ListKind::Delete) forgetAll();
      return;
    case wikiui::ActionResult:
      fromList_ = false;
      if (value >= 0 && value < static_cast<int>(results_.size())) {
        const wikies::Hit hit = results_[value];
        history_.clear();
        open(hit.id, 0, hit.title);
      }
      return;
    case wikiui::ActionRecent:
      fromList_ = false;
      if (value >= 0 && value < wikiui::kMaxRecent && recentRows_[value] >= 0 &&
          recentRows_[value] < static_cast<int>(state_.recent.size())) {
        const wikipedia::RecentEntry entry = state_.recent[recentRows_[value]];
        history_.clear();
        open(entry.locator, 0, entry.title);
      }
      return;
    case wikiui::ActionContinue: {
      fromList_ = false;
      const wikipedia::RecentEntry entry = state_.current;
      history_.clear();
      open(entry.locator, state_.currentPage, entry.title);
      return;
    }
    case wikiui::ActionRandom:
      fromList_ = false;
      startJob(Job::Random);
      return;
    case wikiui::ActionClear:
      // The X closes the search: the text goes and the keyboard with it.
      keyboardShown_ = false;
      query_.clear();
      refreshLocal();
      requestUpdate();
      return;
    case wikiui::ActionField:
      if (!keyboardShown_) {
        keyboardShown_ = true;
        kbGate_.arm();
        requestUpdate();
      }
      return;
    case wikiui::ActionContents:
      if (view_ == View::Article && !article_.headings.empty()) {
        refreshHeadingPages();
        // Row 0 is the top; heading h is row h + 1. Open on the window holding
        // the section the page is in.
        const int row = (section_ ? headingForPage(section_->currentPage) : -1) + 1;
        contentsFirst_ = (row / wikiui::kContentsRows) * wikiui::kContentsRows;
        go(View::Contents);
      }
      return;
    case wikiui::ActionHeading:
      if (view_ == View::List) {
        listTapped(value);
        return;
      }
      if (value == 0) {
        targetPage_ = 0;
        pendingAnchor_.clear();
        go(View::Article);
      } else if (value > 0 && value <= static_cast<int>(article_.headings.size())) {
        const int h = value - 1;
        pendingAnchor_ = "s" + std::to_string(h + 1);
        if (h < static_cast<int>(headingPages_.size()) && headingPages_[h] >= 0) {
          targetPage_ = headingPages_[h];
          pendingAnchor_.clear();
        }
        go(View::Article);
      }
      return;
    case wikiui::ActionClose:
      if (view_ == View::List) {
        closeList();
      } else {
        go(View::Article);
      }
      return;
    case wikiui::ActionMore: {
      if (view_ == View::List) {
        const int rows = static_cast<int>(listRows_.size());
        const int step = listShown_ > 0 ? listShown_ : wikiui::kContentsRows;
        listFirst_ = listFirst_ + step < rows ? listFirst_ + step : 0;
        requestUpdate();
        return;
      }
      // The next window starts after what the last render fitted.
      const int count = static_cast<int>(article_.headings.size()) + 1;
      const int step = contentsShown_ > 0 ? contentsShown_ : wikiui::kContentsRows;
      contentsFirst_ = contentsFirst_ + step < count ? contentsFirst_ + step : 0;
      requestUpdate();
      return;
    }
    case wikiui::ActionPrevious:
      saveState();
      popHistory();
      return;
    case wikiui::ActionBack:
      if (section_) {
        go(View::Article);
      } else {
        go(View::Search);
      }
      return;
    default:
      return;
  }
}

void WikipediaActivity::loop() {
  // A job waits for two things, in turn: a network to use, then its status on
  // the panel, because the fetch blocks and the frame must be there first.
  if (job_ != Job::None) {
    if (!jobArmed_) return;  // the Wi-Fi picker is in front
    if (!jobReady_) {
      if (job_ == Job::Search) {
        showNotice("BUSCANDO", query_.c_str(), nullptr, 0);
        requestUpdateAndWait();
      } else if (job_ == Job::Random) {
        showNotice("AL AZAR", "Buscando un artículo cualquiera.", nullptr, 0);
        requestUpdateAndWait();
      } else if (job_ == Job::Feed) {
        showNotice("BUSCANDO",
                   listRows_.size() > static_cast<size_t>(feed_) ? listRows_[static_cast<size_t>(feed_)].c_str() : "",
                   nullptr, 0);
        requestUpdateAndWait();
      }
      jobReady_ = true;
      return;
    }
    runJob();
    return;
  }
  // The radio goes down a while after the last fetch.
  if (radio_ && static_cast<uint32_t>(millis()) - radioUsedMs_ >= kRadioIdleMs) radio_.reset();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    switch (view_) {
      case View::Search:
        if (!query_.empty()) {
          query_.clear();
          refreshLocal();
          requestUpdate();
        } else if (keyboardShown_) {
          keyboardShown_ = false;
          requestUpdate();
        } else {
          shelf::leave(renderer, mappedInput);
        }
        return;
      case View::Article:
        // The edge swipe leaves the article for the main page; the band's
        // chevron is the one that walks the trail back.
        saveState();
        history_.clear();
        closeArticle();
        if (fromList_) {
          // Back to the list it was picked from. The saved one is rebuilt:
          // opening an article moved it to the top.
          fromList_ = false;
          if (listKind_ == ListKind::Saved) {
            showSaved(false);
          } else {
            go(View::List);
          }
        } else {
          go(View::Search);
        }
        return;
      case View::Contents:
        go(View::Article);
        return;
      case View::List:
        closeList();
        return;
      case View::Notice:
        routeAction(wikiui::ActionBack, 0);
        return;
      default:
        return;
    }
  }

  if (view_ == View::Article) {
    // The rest of the article lays out between renders, a couple of pages a
    // tick, so a page turn is a seek rather than a wait. Under the render
    // lock, as the reader does: render() lays out on the other task.
    if (section_ && section_->isBuilding() && !section_->isBuildComplete() && !buildFailed_ && !RenderLock::peek() &&
        ESP.getFreeHeap() > kBuildMinHeap) {
      RenderLock lock;
      if (section_->isBuilding() && !section_->isBuildComplete()) {
        if (!section_->buildSomeMore(2)) {
          buildFailed_ = true;
        } else if (section_->isBuildComplete()) {
          requestUpdate();
        }
      }
    }
    // A vertical swipe turns the page too; a horizontal one is not read here,
    // because the left-edge swipe IS Button::Back.
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Up) {
      turnPage(1);
      return;
    }
    if (swipe == MappedInputManager::SwipeDir::Down) {
      turnPage(-1);
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::PageBack) ||
        mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      turnPage(-1);
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::PageForward) ||
        mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      turnPage(1);
      return;
    }
  } else if (view_ == View::List) {
    const int count = static_cast<int>(listRows_.size());
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      const int step = listShown_ > 0 ? listShown_ : wikiui::kContentsRows;
      if (listFirst_ + step < count) {
        listFirst_ += step;
        requestUpdate();
      }
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      if (listFirst_ > 0) {
        listFirst_ = std::max(0, listFirst_ - wikiui::kContentsRows);
        requestUpdate();
      }
      return;
    }
  } else if (view_ == View::Contents) {
    const int rows = wikiui::kContentsRows;
    const int count = static_cast<int>(article_.headings.size()) + 1;
    if (mappedInput.wasPressed(MappedInputManager::Button::Down)) {
      const int step = contentsShown_ > 0 ? contentsShown_ : rows;
      if (contentsFirst_ + step < count) {
        contentsFirst_ += step;
        requestUpdate();
      }
      return;
    }
    if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
      if (contentsFirst_ > 0) {
        contentsFirst_ = std::max(0, contentsFirst_ - rows);
        requestUpdate();
      }
      return;
    }
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY)) return;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);

  if (view_ == View::Search && keyboardShown_ && kbGate_.revealed()) {
    const fui::ActionEvent key = kbInteractions_.routePublished(input);
    if (key.action == wikiui::ActionKey) {
      handleKey(static_cast<int>(key.value));
      return;
    }
  }
  if (!interactionsReady_) return;

  if (view_ == View::Article) {
    const fui::ActionEvent ev = interactions_.route(input);
    if (ev.action != fui::NO_ACTION) {
      routeAction(static_cast<int>(ev.action), static_cast<int>(ev.value));
      return;
    }
    // Tap zones: the left third turns back, the rest turns forward.
    if (tapY > toybox::kChromeHeight) turnPage(tapX < renderer.getScreenWidth() / 3 ? -1 : 1);
    return;
  }

  const fui::ActionEvent ev = interactions_.route(input);
  routeAction(static_cast<int>(ev.action), static_cast<int>(ev.value));
}

// ---------------------------------------------------------------- render

void WikipediaActivity::render(RenderLock&&) {
  // A cue may still be on the panel's waveform; the framebuffer is its until
  // it lands. No-op when none is outstanding.
  if (cuePainted_) {
    renderer.waitRefreshComplete();
    cuePainted_ = false;
  }
  renderer.clearScreen();
  // Titles are somebody else's words, and the toybox faces stop at Latin-1.
  // So every slot that shows a title is the reader's own face, the one the
  // page is set in; Jersey is for the app's own captions and buttons.
  const int readerFont = SETTINGS.getReaderFontId();
  const int readerSmall =
      SETTINGS.fontFamily == CrossPointSettings::NOTOSANS ? NOTOSANS_12_FONT_ID : NOTOSERIF_12_FONT_ID;
  const bool prose = view_ == View::Article || view_ == View::Contents || view_ == View::List;
  const toybox::Faces faces = prose ? toybox::Faces{readerSmall, readerFont, readerFont}
                                    : toybox::Faces{toybox::kButtonFontId, readerFont, toybox::kDisplayFontId};
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, faces);
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions_);
  toybox::Screen screen(frame);

  switch (view_) {
    case View::Search: {
      wikiui::SearchModel model;
      model.query = query_.c_str();
      model.resultCount = static_cast<int>(std::min<size_t>(results_.size(), wikiui::kMaxResults));
      for (int i = 0; i < model.resultCount; ++i) model.results[i].title = results_[i].title.c_str();
      model.noMatch = searched_ && !offline_ && results_.empty();
      if (!query_.empty()) {
        if (!searched_) {
          model.hint = "IR: BUSCAR EN WIKIPEDIA";
        } else if (offline_) {
          model.hint = results_.empty() ? "SIN WIFI. NADA GUARDADO" : "SIN WIFI: SOLO GUARDADOS";
        }
      }
      model.continueTitle = state_.current.title.empty() ? nullptr : state_.current.title.c_str();
      model.continuePage = state_.current.title.empty() ? 0 : state_.currentPage + 1;
      // The trail leaves out the article CONTINUAR already names; VALUE is the
      // row in state_.recent, so the activity keeps the map.
      model.recentCount = 0;
      for (size_t i = 0; i < state_.recent.size() && model.recentCount < wikiui::kMaxRecent; ++i) {
        if (!state_.current.title.empty() && state_.recent[i].locator == state_.current.locator) continue;
        model.recent[model.recentCount].title = state_.recent[i].title.c_str();
        recentRows_[model.recentCount] = static_cast<int>(i);
        ++model.recentCount;
      }
      model.footer = footer_.c_str();
      model.manage = !saved_.empty();
      if (keyboardShown_) {
        const fui::Rect kb = keyboardRect();
        model.keyboardHeight = static_cast<int16_t>(renderer.getScreenHeight() - kb.y + 8);
      }
      wikiui::buildSearch(screen, model);
      break;
    }
    case View::Article:
      renderArticle(screen);
      break;
    case View::Contents: {
      // The top first (page 1), then every section with its page once the
      // layout has reached it.
      std::vector<const char*> names;
      std::vector<int> pages;
      names.reserve(article_.headings.size() + 1);
      pages.reserve(article_.headings.size() + 1);
      names.push_back("Inicio");
      pages.push_back(0);
      for (size_t i = 0; i < article_.headings.size(); ++i) {
        names.push_back(article_.headings[i].c_str());
        pages.push_back(i < headingPages_.size() ? headingPages_[i] : -1);
      }
      wikiui::ContentsModel model;
      model.title = article_.title.c_str();
      model.headings = names.data();
      model.pages = pages.data();
      model.count = static_cast<int>(names.size());
      model.current = (section_ ? headingForPage(section_->currentPage) : -1) + 1;
      model.first = contentsFirst_;
      contentsShown_ = wikiui::buildContents(screen, model);
      break;
    }
    case View::List: {
      std::vector<const char*> names;
      names.reserve(listRows_.size());
      for (const std::string& row : listRows_) names.push_back(row.c_str());
      wikiui::ContentsModel model;
      model.title = listTitle_.c_str();
      model.headings = names.data();
      model.pages = nullptr;
      model.count = static_cast<int>(names.size());
      model.current = -1;
      model.first = listFirst_;
      listShown_ = wikiui::buildContents(screen, model);
      break;
    }
    case View::Notice: {
      wikiui::NoticeModel model;
      model.headline = noticeHead_.c_str();
      model.body = noticeBody_.c_str();
      model.actionLabel = noticeAction_;
      model.action = noticeActionId_;
      wikiui::buildNotice(screen, model);
      break;
    }
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Wikipedia");
  if (view_ == View::Search && keyboardShown_) drawKeyboard();
  renderer.displayBuffer();
}
