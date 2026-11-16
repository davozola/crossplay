#include "MarketsStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <ctime>
#include <memory>
#include <vector>

#include "../../network/HttpDownloader.h"

namespace marketsstore {

namespace mk = markets;

namespace {

constexpr char kCacheDir[] = "/.crosspoint/markets";
constexpr uint8_t kCacheVersion = 2;    // 1 kept the source's raw answer
constexpr size_t kMaxBody = 64 * 1024;  // 300 daily candles are ~22 KB
// CNBC's endpoints are written for a browser; the others take anything.
constexpr char kBrowserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36";

void cachePath(const mk::Asset asset, const char* ext, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%s.%s", kCacheDir, mk::infoOf(asset).file, ext);
}

// A GET into memory, by way of a file on the card: the downloader that takes
// request headers writes to one. Leaves the HTTP status (0 when nothing
// answered) in `status`.
bool get(const char* url, std::unique_ptr<char[]>& body, size_t& size, int& status) {
  char staged[64];
  std::snprintf(staged, sizeof(staged), "%s/answer.tmp", kCacheDir);
  const std::vector<HttpDownloader::Header> headers = {{"User-Agent", kBrowserAgent}, {"Accept", "application/json"}};
  const auto result = HttpDownloader::downloadToFile(url, staged, nullptr, nullptr, "", "", headers);
  status = HttpDownloader::lastStatus();
  if (result != HttpDownloader::OK) {
    LOG_ERR("MKTS", "fetch failed, status %d: %s", status, url);
    return false;
  }
  bool ok = false;
  {
    HalFile file;
    if (Storage.openFileForRead("MKTS", staged, file)) {
      size = file.fileSize();
      if (size > 0 && size <= kMaxBody) {
        body = makeUniqueNoThrow<char[]>(size);
        if (!body) {
          LOG_ERR("MKTS", "OOM: %u bytes for an answer", static_cast<unsigned>(size));
        } else {
          ok = file.read(body.get(), size) == static_cast<int>(size);
        }
      }
    }
  }
  Storage.remove(staged);
  return ok;
}

// "200", or what went wrong in a word, for the line shown when nothing came.
void outcome(const bool parsed, const int status, char* out, const size_t cap) {
  if (parsed)
    std::snprintf(out, cap, "OK");
  else if (status == 0)
    std::snprintf(out, cap, "NO LINK");
  else if (status == 200)
    std::snprintf(out, cap, "UNREADABLE");
  else
    std::snprintf(out, cap, "HTTP %d", status % 1000);
}

}  // namespace

bool loadCached(const mk::Asset asset, mk::Quote& out) {
  char path[64];
  cachePath(asset, "q", path, sizeof(path));
  if (!Storage.exists(path)) return false;
  HalFile file;
  if (!Storage.openFileForRead("MKTS", path, file)) return false;
  uint8_t version = 0;
  if (file.read(&version, 1) != 1 || version != kCacheVersion) return false;
  auto fresh = makeUniqueNoThrow<mk::Quote>();
  if (!fresh) {
    LOG_ERR("MKTS", "OOM: a quote");
    return false;
  }
  if (file.read(reinterpret_cast<uint8_t*>(fresh.get()), sizeof(mk::Quote)) != sizeof(mk::Quote)) return false;
  if (!fresh->valid || fresh->count < 2 || fresh->count > mk::kMaxCloses) return false;
  fresh->currency[sizeof(fresh->currency) - 1] = 0;
  fresh->zone[sizeof(fresh->zone) - 1] = 0;
  out = *fresh;
  return true;
}

void saveCached(const mk::Asset asset, const mk::Quote& quote) {
  char path[64];
  cachePath(asset, "q", path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("MKTS", path, file)) return;
  file.write(&kCacheVersion, 1);
  file.write(reinterpret_cast<const uint8_t*>(&quote), sizeof(mk::Quote));
  file.flush();
}

// Bitcoin from Coinbase's candles, then each index from CNBC's bars, then one
// CNBC quote laid over both. A quote replaces the one held only once it has
// parsed, so an error page never costs a good one. Returns how many arrived.
int fetchAll(mk::Quote quotes[mk::kAssets], char* report, const size_t reportCap) {
  Storage.ensureDirectoryExists(kCacheDir);
  int ok = 0;
  char url[256];
  std::unique_ptr<char[]> body;
  size_t size = 0;
  int status = 0;
  char words[3][12];
  auto fresh = makeUniqueNoThrow<mk::Quote>();
  if (!fresh) {
    LOG_ERR("MKTS", "OOM: a quote");
    return 0;
  }

  mk::coinbaseUrl(url, sizeof(url));
  bool parsed = get(url, body, size, status) && mk::parseCoinbase(body.get(), size, *fresh);
  outcome(parsed, status, words[0], sizeof(words[0]));
  // Today, for the bars' date range: the device's clock when it has been set,
  // and otherwise the newest candle's day.
  const time_t clock = time(nullptr);
  int32_t today = clock > 1700000000 ? static_cast<int32_t>(clock / 86400) : 0;
  if (parsed) {
    if (clock > 1700000000) fresh->time = static_cast<int64_t>(clock);
    if (today == 0) today = fresh->days[fresh->count - 1];
    quotes[0] = *fresh;
    saveCached(mk::Asset::Bitcoin, quotes[0]);
    ++ok;
  }
  body.reset();

  bool haveIndex[mk::kAssets] = {};
  for (int i = 1; i < mk::kAssets; ++i) {
    const mk::Asset asset = static_cast<mk::Asset>(i);
    if (today == 0) {
      std::snprintf(words[i], sizeof(words[i]), "NO DATE");
      continue;
    }
    mk::cnbcBarsUrl(asset, today, url, sizeof(url));
    parsed = get(url, body, size, status) && mk::parseCnbcBars(body.get(), size, *fresh);
    outcome(parsed, status, words[i], sizeof(words[i]));
    body.reset();
    if (!parsed) continue;
    quotes[i] = *fresh;
    haveIndex[i] = true;
    ++ok;
  }
  if (haveIndex[1] || haveIndex[2]) {
    mk::cnbcQuoteUrl(url, sizeof(url));
    if (get(url, body, size, status)) {
      for (int i = 1; i < mk::kAssets; ++i) {
        if (haveIndex[i]) mk::applyCnbcQuote(body.get(), size, static_cast<mk::Asset>(i), quotes[i]);
      }
    }
    for (int i = 1; i < mk::kAssets; ++i) {
      if (haveIndex[i]) saveCached(static_cast<mk::Asset>(i), quotes[i]);
    }
  }
  std::snprintf(report, reportCap, "COINBASE: %s.  CNBC S&P: %s.  CNBC NASDAQ: %s.", words[0], words[1], words[2]);
  return ok;
}

}  // namespace marketsstore
