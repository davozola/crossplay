#include "MarketsStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <vector>

#include "../../network/HttpDownloader.h"

namespace marketsstore {

namespace mk = markets;

namespace {

constexpr char kCacheDir[] = "/.crosspoint/markets";
constexpr uint8_t kCacheVersion = 4;     // 1 kept the source's raw answer
constexpr size_t kMaxBody = 128 * 1024;  // 300 daily candles are ~22 KB
// CNBC's endpoints are written for a browser; the others take anything.
constexpr char kBrowserAgent[] =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/126.0 Safari/537.36";

constexpr char kPicksPath[] = "/.crosspoint/markets/picks";
constexpr char kCustomsPath[] = "/.crosspoint/markets/custom";
constexpr char kPortfolioPath[] = "/.crosspoint/markets/cartera";
bool picksLoaded = false;

void cachePath(const int instrument, const char* ext, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%s.%s", kCacheDir, mk::instrumentAt(instrument).file, ext);
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

bool loadInstrument(const int instrument, mk::Quote& out) {
  char path[64];
  cachePath(instrument, "q", path, sizeof(path));
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

void saveInstrument(const int instrument, const mk::Quote& quote) {
  char path[64];
  cachePath(instrument, "q", path, sizeof(path));
  HalFile file;
  if (!Storage.openFileForWrite("MKTS", path, file)) return;
  file.write(&kCacheVersion, 1);
  file.write(reinterpret_cast<const uint8_t*>(&quote), sizeof(mk::Quote));
  file.flush();
}

}  // namespace

void loadPicks() {
  if (picksLoaded) return;
  picksLoaded = true;
  // What was added by hand first: a pick may be one of them.
  mk::clearCustoms();
  if (Storage.exists(kCustomsPath)) {
    const String kept = Storage.readFile(kCustomsPath);
    mk::unpackCustoms(kept.c_str(), kept.length());
  }
  mk::setPicks(mk::defaultPicks());
  if (!Storage.exists(kPicksPath)) return;
  HalFile file;
  uint8_t data[1 + mk::kAssets] = {};
  if (!Storage.openFileForRead("MKTS", kPicksPath, file)) return;
  if (file.read(data, sizeof(data)) != sizeof(data) || data[0] != 1) return;
  mk::Picks picks;
  for (int i = 0; i < mk::kAssets; ++i) picks.slot[i] = data[1 + i];
  mk::setPicks(picks);
}

void savePicks(const mk::Picks& picks) {
  mk::setPicks(picks);
  picksLoaded = true;
  Storage.ensureDirectoryExists(kCacheDir);
  HalFile file;
  if (!Storage.openFileForWrite("MKTS", kPicksPath, file)) return;
  const mk::Picks kept = mk::picks();
  uint8_t data[1 + mk::kAssets] = {1};
  for (int i = 0; i < mk::kAssets; ++i) data[1 + i] = kept.slot[i];
  file.write(data, sizeof(data));
  file.flush();
}

void saveCustoms() {
  Storage.ensureDirectoryExists(kCacheDir);
  char text[mk::kCustomsBytes];
  if (mk::packCustoms(text, sizeof(text)) == 0) return;
  if (!Storage.writeFile(kCustomsPath, String(text))) LOG_ERR("MKTS", "Could not write %s", kCustomsPath);
}

void forget(const int instrument) {
  if (!mk::present(instrument)) return;
  char path[64];
  cachePath(instrument, "q", path, sizeof(path));
  if (Storage.exists(path)) Storage.remove(path);
}

int search(const char* query, mk::Found* out, const int cap, int& status) {
  Storage.ensureDirectoryExists(kCacheDir);
  char url[256];
  mk::lookupUrl(query, url, sizeof(url));
  std::unique_ptr<char[]> body;
  size_t size = 0;
  if (!get(url, body, size, status)) return -1;
  return mk::parseLookup(body.get(), size, out, cap);
}

void loadPortfolio(mk::Portfolio& out) {
  out.count = 0;
  loadPicks();
  if (!Storage.exists(kPortfolioPath)) return;
  HalFile file;
  uint8_t bytes[mk::kPortfolioBytes];
  if (!Storage.openFileForRead("MKTS", kPortfolioPath, file)) return;
  const int got = file.read(bytes, sizeof(bytes));
  if (got > 0) mk::unpackPortfolio(bytes, static_cast<size_t>(got), out);
}

void savePortfolio(const mk::Portfolio& portfolio) {
  Storage.ensureDirectoryExists(kCacheDir);
  HalFile file;
  uint8_t bytes[mk::kPortfolioBytes];
  if (mk::packPortfolio(portfolio, bytes, sizeof(bytes)) == 0) return;
  if (!Storage.openFileForWrite("MKTS", kPortfolioPath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

void pricePortfolio(const mk::Portfolio& portfolio, mk::Priced* priced, double& euro, const mk::Period period) {
  euro = 0;
  auto quote = makeUniqueNoThrow<mk::Quote>();
  if (!quote) {
    LOG_ERR("MKTS", "OOM: a quote");
    for (int i = 0; i < portfolio.count; ++i) priced[i] = mk::Priced{};
    return;
  }
  if (mk::euroInstrument() >= 0 && loadInstrument(mk::euroInstrument(), *quote)) euro = quote->price;
  for (int i = 0; i < portfolio.count; ++i) {
    priced[i] = mk::Priced{};
    const int instrument = portfolio.positions[i].instrument;
    if (!loadInstrument(instrument, *quote)) continue;
    // Everything is totalled in dollars: euros at the rate fetched with them,
    // and what is counted in anything else is left unpriced.
    double rate = 1;
    const char* currency = mk::currencyOf(instrument);
    if (std::strcmp(currency, "EUR") == 0) {
      rate = euro;
    } else if (std::strcmp(currency, "USD") != 0) {
      rate = 0;
    }
    if (!(rate > 0)) continue;
    priced[i].valid = true;
    priced[i].price = quote->price * rate;
    priced[i].previous = mk::closeBefore(*quote, period) * rate;
  }
}

bool loadCached(const mk::Asset asset, mk::Quote& out) {
  loadPicks();
  return loadInstrument(mk::instrumentOf(asset), out);
}

// Everything wanted is fetched one instrument at a time: crypto from Coinbase's
// candles, the rest from CNBC's bars, and then one CNBC quote laid over all of
// those. A quote replaces the one held only once it has parsed, so an error
// page never costs a good one.
int fetchAll(mk::Quote quotes[mk::kAssets], char* report, const size_t reportCap) {
  loadPicks();
  Storage.ensureDirectoryExists(kCacheDir);

  // The three cards first, then what the portfolio holds beyond them, then the
  // euro if there is a portfolio to total.
  constexpr int kMaxWanted = mk::kAssets + mk::kMaxPositions + 1;
  int wanted[kMaxWanted];
  int count = 0;
  auto want = [&](const int instrument) {
    for (int i = 0; i < count; ++i) {
      if (wanted[i] == instrument) return;
    }
    if (count < kMaxWanted) wanted[count++] = instrument;
  };
  for (int i = 0; i < mk::kAssets; ++i) wanted[count++] = mk::instrumentOf(static_cast<mk::Asset>(i));
  {
    mk::Portfolio portfolio;
    loadPortfolio(portfolio);
    for (int i = 0; i < portfolio.count; ++i) want(portfolio.positions[i].instrument);
    if (portfolio.count > 0 && mk::euroInstrument() >= 0) want(mk::euroInstrument());
  }

  auto fetched = makeUniqueNoThrow<mk::Quote[]>(static_cast<size_t>(count));
  if (!fetched) {
    LOG_ERR("MKTS", "OOM: %d quotes", count);
    std::snprintf(report, reportCap, "OUT OF MEMORY.");
    return 0;
  }
  bool have[kMaxWanted] = {};
  char url[384];
  std::unique_ptr<char[]> body;
  size_t size = 0;
  int status = 0;
  char word[12];
  char failed[96] = "";
  auto fail = [&](const int instrument) {
    const size_t at = std::strlen(failed);
    std::snprintf(failed + at, sizeof(failed) - at, "%s%s %s", at ? ", " : "", mk::instrumentAt(instrument).name, word);
  };

  // Today, for the bars' date range: the device's clock when it has been set,
  // and otherwise the newest candle's day from the first crypto answer.
  const time_t clock = time(nullptr);
  int32_t today = clock > 1700000000 ? static_cast<int32_t>(clock / 86400) : 0;

  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < count; ++i) {
      const mk::AssetInfo& info = mk::instrumentAt(wanted[i]);
      if (info.always != (pass == 0)) continue;
      bool parsed = false;
      if (info.always) {
        mk::coinbaseUrlFor(info.symbol, url, sizeof(url));
        parsed = get(url, body, size, status) && mk::parseCoinbase(body.get(), size, fetched[i]);
        if (parsed) {
          // The day's change of what never closes is over the last 24 hours.
          // Without this answer it falls back to the daily candle's.
          mk::coinbaseStatsUrlFor(info.symbol, url, sizeof(url));
          int statsStatus = 0;
          if (get(url, body, size, statsStatus)) mk::applyCoinbaseStats(body.get(), size, fetched[i]);
          if (clock > 1700000000) fetched[i].time = static_cast<int64_t>(clock);
          if (today == 0) today = fetched[i].days[fetched[i].count - 1];
        }
        outcome(parsed, status, word, sizeof(word));
      } else if (today == 0) {
        std::snprintf(word, sizeof(word), "NO DATE");
      } else {
        // A year of bars, and the four months this always asked for if the
        // source will not give that many.
        mk::cnbcBarsUrlSpan(info.symbol, today, mk::kYearOfDays, url, sizeof(url));
        parsed = get(url, body, size, status) && mk::parseCnbcBars(body.get(), size, fetched[i]);
        if (!parsed) {
          body.reset();
          mk::cnbcBarsUrlFor(info.symbol, today, url, sizeof(url));
          parsed = get(url, body, size, status) && mk::parseCnbcBars(body.get(), size, fetched[i]);
        }
        outcome(parsed, status, word, sizeof(word));
      }
      body.reset();
      if (parsed) std::snprintf(fetched[i].currency, sizeof(fetched[i].currency), "%s", mk::currencyOf(wanted[i]));
      have[i] = parsed;
      if (!parsed) fail(wanted[i]);
    }
  }

  // One quote answer for every CNBC instrument that has bars.
  const char* symbols[kMaxWanted];
  int symbolCount = 0;
  for (int i = 0; i < count; ++i) {
    if (have[i] && !mk::instrumentAt(wanted[i]).always) symbols[symbolCount++] = mk::instrumentAt(wanted[i]).symbol;
  }
  if (symbolCount > 0) {
    mk::cnbcQuoteUrlFor(symbols, symbolCount, url, sizeof(url));
    if (get(url, body, size, status)) {
      for (int i = 0; i < count; ++i) {
        if (have[i] && !mk::instrumentAt(wanted[i]).always) {
          mk::applyCnbcQuoteFor(body.get(), size, mk::instrumentAt(wanted[i]).symbol, fetched[i]);
        }
      }
    }
    body.reset();
  }

  int arrived = 0;
  int cards = 0;
  bool learnt = false;
  for (int i = 0; i < count; ++i) {
    if (have[i] && mk::setCustomCurrency(wanted[i], fetched[i].currency)) learnt = true;
  }
  if (learnt) saveCustoms();
  for (int i = 0; i < count; ++i) {
    if (!have[i]) continue;
    ++arrived;
    saveInstrument(wanted[i], fetched[i]);
    if (i < mk::kAssets) {
      quotes[i] = fetched[i];
      ++cards;
    }
  }
  if (arrived == count) {
    std::snprintf(report, reportCap, "ALL %d ANSWERED.", count % 100);
  } else {
    std::snprintf(report, reportCap, "%d OF %d ANSWERED. NOT: %s.", arrived % 100, count % 100, failed);
  }
  return cards;
}

}  // namespace marketsstore
