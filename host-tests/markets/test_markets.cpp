// MARKETS host tests: the Yahoo chart reader against fixtures in the answer's
// shape, the change arithmetic, and the number formatting.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "MarketsCore.h"

namespace {

int failures = 0;

void check(const bool ok, const char* what) {
  if (!ok) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

std::string slurp(const char* path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream s;
  s << in.rdbuf();
  return s.str();
}

bool near(const double a, const double b, const double eps = 0.011) { return std::fabs(a - b) < eps; }

bool fmtPrice(const double v, const char* want) {
  char out[32];
  markets::formatPrice(v, out, sizeof(out));
  if (std::strcmp(out, want) != 0) std::printf("  price %f -> '%s', want '%s'\n", v, out, want);
  return std::strcmp(out, want) == 0;
}

bool fmtChange(const double v, const char* want) {
  char out[32];
  markets::formatChange(v, out, sizeof(out));
  if (std::strcmp(out, want) != 0) std::printf("  change %f -> '%s', want '%s'\n", v, out, want);
  return std::strcmp(out, want) == 0;
}

bool fmtPercent(const double v, const char* want) {
  char out[32];
  markets::formatPercent(v, out, sizeof(out));
  return std::strcmp(out, want) == 0;
}

bool fmtVolume(const double v, const char* want) {
  char out[32];
  markets::formatVolume(v, out, sizeof(out));
  if (std::strcmp(out, want) != 0) std::printf("  volume %f -> '%s', want '%s'\n", v, out, want);
  return std::strcmp(out, want) == 0;
}

}  // namespace

int main() {
  using namespace markets;

  // --- an index, after the close ---
  {
    const std::string json = slurp("sp500.json");
    check(!json.empty(), "fixture sp500.json readable");
    Quote q;
    check(parseQuote(json.data(), json.size(), q), "sp500 parses");
    check(q.valid && q.count == 64, "the null close is dropped with its stamp");
    check(near(q.price, 6459.6), "price from meta");
    check(near(previousClose(q), 6441.19), "previous close is the close before the last");
    check(near(dayChange(q), 18.41), "day change");
    check(near(dayChangePercent(q), 18.41 / 6441.19 * 100.0, 0.001), "day change percent");
    check(std::strcmp(q.currency, "USD") == 0 && std::strcmp(q.zone, "EST") == 0, "currency and zone");
    check(q.gmtOffset == -18000, "gmt offset");
    check(!sessionOpen(q), "a price stamped at the bell is a closed session");
    const Civil c = civilOf(q.time, q.gmtOffset);
    check(c.year == 2026 && c.month == 11 && c.day == 13 && c.hour == 16 && c.minute == 0,
          "last trade in exchange time");
    int y = 0, m = 0, d = 0;
    civilOfDay(q.days[q.count - 1], y, m, d);
    check(y == 2026 && m == 11 && d == 13, "last close's day");
    civilOfDay(q.days[0], y, m, d);
    check(y == 2026 && m == 8 && d == 17, "first close's day");
    float lo = 0, hi = 0;
    closeRange(q, 0, lo, hi);
    check(near(lo, 6122.23) && near(hi, 6462.01), "range over everything held");
    closeRange(q, 5, lo, hi);
    check(lo >= 6122.23f && hi <= 6462.01f && lo <= hi, "range over a week sits inside it");
    double pct = 0;
    check(changeOver(q, 5, pct), "week change exists");
    check(near(pct, (q.price - q.closes[q.count - 6]) / q.closes[q.count - 6] * 100.0, 0.001), "week change value");
    check(changeOver(q, 5000, pct), "a span longer than the history clamps to its start");
    check(near(pct, (q.price - q.closes[0]) / q.closes[0] * 100.0, 0.001), "clamped change value");
    check(!changeOver(q, 0, pct), "no change over no sessions");
  }

  // --- bitcoin, mid-session ---
  {
    const std::string json = slurp("btc.json");
    Quote q;
    check(parseQuote(json.data(), json.size(), q), "btc parses");
    check(q.count == 92, "92 daily closes kept");
    check(sessionOpen(q), "a price inside the trading period is an open session");
    check(q.gmtOffset == 0 && std::strcmp(q.zone, "UTC") == 0, "utc");
    const Civil c = civilOf(q.time, q.gmtOffset);
    check(c.month == 11 && c.day == 16 && c.hour == 7 && c.minute == 58, "btc last trade time");
    check(near(q.volume, 4.21e10, 1), "volume");
  }

  // --- rejects ---
  {
    Quote q;
    const char* err = "{\"chart\":{\"result\":null,\"error\":{\"code\":\"Not Found\",\"description\":\"No data\"}}}";
    check(!parseQuote(err, std::strlen(err), q) && !q.valid, "error answer rejected");
    check(!parseQuote("", 0, q), "empty rejected");
    check(!parseQuote("Too Many Requests", 17, q), "plain-text 429 body rejected");
    const std::string json = slurp("nasdaq.json");
    check(parseQuote(json.data(), json.size(), q), "nasdaq parses");
    Quote half;
    parseQuote(json.data(), json.size() / 2, half);  // must not crash
    check(half.count <= kMaxCloses, "truncated answer safe");
  }

  // --- more closes than the buffer keeps the newest ---
  {
    std::string json = "{\"chart\":{\"result\":[{\"meta\":{\"regularMarketPrice\":300,\"gmtoffset\":0},\"timestamp\":[";
    for (int i = 0; i < 200; ++i) json += (i ? "," : "") + std::to_string(86400 * (20000 + i));
    json += "],\"indicators\":{\"quote\":[{\"close\":[";
    for (int i = 0; i < 200; ++i) json += (i ? "," : "") + std::to_string(100 + i);
    json += "]}]}}]}}";
    Quote q;
    check(parseQuote(json.data(), json.size(), q), "long history parses");
    check(q.count == kMaxCloses, "capped at the buffer");
    check(near(q.closes[q.count - 1], 299) && near(q.closes[0], 300 - kMaxCloses),
          "the newest closes are the ones kept");
    check(q.days[q.count - 1] == 20199, "stamps stay in step with their closes");
  }

  // --- formatting ---
  check(fmtPrice(104250.4, "104,250"), "btc price, no decimals");
  check(fmtPrice(6734.108, "6,734.11"), "index price, two decimals");
  check(fmtPrice(999.5, "999.50"), "under a thousand");
  check(fmtPrice(1234567.0, "1,234,567"), "millions grouped");
  check(fmtPrice(0, "0.00"), "zero");
  {
    char out[32];
    formatPriceShort(6459.6, out, sizeof(out));
    check(std::strcmp(out, "6,460") == 0, "short price rounds to whole units");
    formatPriceShort(112535.5, out, sizeof(out));
    check(std::strcmp(out, "112,536") == 0, "short btc price");
    formatPriceShort(87.456, out, sizeof(out));
    check(std::strcmp(out, "87.46") == 0, "short price keeps cents under a thousand");
  }
  check(fmtChange(1190.4, "+1,190"), "large change");
  check(fmtChange(-12.4, "-12.40"), "small loss");
  check(fmtChange(0.001, "0.00"), "a change that rounds to nothing carries no sign");
  check(fmtChange(-0.004, "0.00"), "nor a minus");
  check(fmtPercent(1.236, "+1.24%"), "percent up");
  check(fmtPercent(-0.5, "-0.50%"), "percent down");
  check(fmtPercent(-0.001, "0.00%"), "percent that rounds to zero is unsigned");
  check(fmtVolume(3312000000.0, "3.31B"), "billions");
  check(fmtVolume(842000000.0, "842M"), "hundreds of millions");
  check(fmtVolume(12400.0, "12.4K"), "thousands");
  check(fmtVolume(950.0, "950"), "units");
  check(fmtVolume(4.21e10, "42.1B"), "tens of billions");

  // --- dates ---
  {
    int y = 0, m = 0, d = 0;
    civilOfDay(0, y, m, d);
    check(y == 1970 && m == 1 && d == 1, "epoch day");
    civilOfDay(19782, y, m, d);
    check(y == 2024 && m == 2 && d == 29, "leap day");
    const Civil c = civilOf(-1, 0);
    check(c.year == 1969 && c.month == 12 && c.day == 31 && c.hour == 23 && c.minute == 59, "before the epoch");
    check(std::strcmp(monthName(11), "NOV") == 0 && monthName(13)[0] == 0, "month names");
  }

  // --- urls and the table ---
  {
    char url[160];
    quoteUrl(Asset::Sp500, url, sizeof(url));
    check(std::strstr(url, "/chart/%5EGSPC?range=3mo&interval=1d") != nullptr, "index url");
    quoteUrl(Asset::Bitcoin, url, sizeof(url));
    check(std::strstr(url, "/chart/BTC-USD?") != nullptr, "btc url");
    check(infoOf(Asset::Bitcoin).always && !infoOf(Asset::Nasdaq).always, "only bitcoin trades around the clock");
  }

  if (failures == 0) std::printf("markets: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
