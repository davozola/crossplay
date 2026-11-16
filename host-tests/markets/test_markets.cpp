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

  // --- an index: bars, then the quote laid over them ---
  {
    const std::string bars = slurp("cnbc_spx.json");
    const std::string quote = slurp("cnbc_quote.json");
    check(!bars.empty() && !quote.empty(), "cnbc fixtures readable");
    Quote q;
    check(parseCnbcBars(bars.data(), bars.size(), q), "spx bars parse");
    check(q.valid && q.count == 75, "every bar kept");
    check(near(q.price, 7561.67) && near(q.dayHigh, 7621.78) && near(q.dayLow, 7499.40), "the last bar is the day");
    check(near(previousClose(q), 7592.40), "previous close is the bar before");
    check(near(dayChange(q), -30.73), "day change");
    check(std::strcmp(q.currency, "USD") == 0 && std::strcmp(q.zone, "EST") == 0 && q.gmtOffset == -18000, "new york");
    check(!sessionOpen(q), "no session is claimed");
    int y = 0, m = 0, d = 0;
    civilOfDay(q.days[q.count - 1], y, m, d);
    check(y == 2026 && m == 11 && d == 13, "last bar's day");
    civilOfDay(q.days[0], y, m, d);
    check(y == 2026 && m == 8 && d == 3, "first bar's day");
    const Civil c = civilOf(q.time, q.gmtOffset);
    check(c.month == 11 && c.day == 13 && c.hour == 16, "stamped at that day's bell");
    float lo = 0, hi = 0;
    closeRange(q, 0, lo, hi);
    check(near(q.yearHigh, hi, 0.01) && near(q.yearLow, lo, 0.01), "without a quote the year is the range held");
    check(applyCnbcQuote(quote.data(), quote.size(), Asset::Sp500, q), "spx quote applies");
    check(q.count == 75 && near(q.price, 7561.67) && near(q.yearHigh, 7844.52) && near(q.yearLow, 6316.91),
          "same day: the price stays the last close and the year's range arrives");
    double pct = 0;
    check(changeOver(q, 5, pct), "week change exists");
    check(near(pct, (q.price - q.closes[q.count - 6]) / q.closes[q.count - 6] * 100.0, 0.001), "week change value");
    check(changeOver(q, 5000, pct) && !changeOver(q, 0, pct), "spans clamp");

    // A quote from a session the bars do not hold yet becomes a new close.
    const std::string ixic = slurp("cnbc_ixic.json");
    Quote n;
    check(parseCnbcBars(ixic.data(), ixic.size(), n) && n.count == 75 && n.volume > 1e9,
          "ixic bars parse, with volume");
    const float before = n.closes[n.count - 1];
    check(applyCnbcQuote(quote.data(), quote.size(), Asset::Nasdaq, n), "ixic quote applies");
    check(n.count == 76 && near(n.price, 25136.273) && near(previousClose(n), before, 0.01), "today is appended");
    civilOfDay(n.days[n.count - 1], y, m, d);
    check(m == 11 && d == 16 && near(n.dayHigh, 25423.089), "dated from a full timestamp");
    Quote stale = q;
    stale.days[stale.count - 1] += 3;
    stale.price = 1;
    check(applyCnbcQuote(quote.data(), quote.size(), Asset::Sp500, stale) && near(stale.price, 1),
          "an older quote does not move the price");
    Quote none;
    check(!applyCnbcQuote(quote.data(), quote.size(), Asset::Sp500, none), "nothing to lay a quote over");
    check(!applyCnbcQuote("{}", 2, Asset::Sp500, q) && near(q.price, 7561.67),
          "a quote without the symbol changes nothing");
  }

  // --- bitcoin ---
  {
    const std::string json = slurp("coinbase.json");
    Quote q;
    check(parseCoinbase(json.data(), json.size(), q), "coinbase parses");
    check(q.count == kMaxCloses, "the newest closes fill the buffer");
    check(near(q.price, 63961.67) && near(q.dayHigh, 65366.39) && near(q.dayLow, 63922), "today's candle");
    check(near(previousClose(q), 65315.35), "yesterday's close");
    check(q.days[q.count - 1] - q.days[0] == kMaxCloses - 1, "oldest first, a day apart");
    int y = 0, m = 0, d = 0;
    civilOfDay(q.days[q.count - 1], y, m, d);
    check(y == 2026 && m == 11 && d == 16, "today");
    check(q.yearHigh > q.dayHigh && q.yearLow < q.dayLow && q.yearLow > 0, "the range covers every candle");
    check(near(q.volume, 4666.779, 0.01) && std::strcmp(q.zone, "UTC") == 0 && q.gmtOffset == 0, "volume and zone");
  }

  // --- rejects ---
  {
    Quote q;
    check(!parseCoinbase("", 0, q) && !parseCoinbase("Too Many Requests", 17, q) && !parseCoinbase("{}", 2, q),
          "not candles");
    check(!parseCoinbase("[[1794787200,1,2,3,4,5]]", 24, q), "one candle is not a history");
    check(!parseCnbcBars("", 0, q) && !parseCnbcBars("<!DOCTYPE html>", 15, q) &&
              !parseCnbcBars("{\"barData\":{}}", 14, q),
          "not bars");
    const std::string json = slurp("cnbc_spx.json");
    Quote half;
    parseCnbcBars(json.data(), json.size() / 2, half);  // must not crash
    check(half.count <= kMaxCloses, "truncated answer safe");
    const std::string candles = slurp("coinbase.json");
    parseCoinbase(candles.data(), candles.size() / 3, half);
    check(half.count <= kMaxCloses, "truncated candles safe");
  }

  // --- more bars than the buffer keeps the newest ---
  {
    std::string json = "{\"barData\":{\"priceBars\":[";
    for (int i = 0; i < 200; ++i) {
      char bar[96];
      std::snprintf(bar, sizeof(bar),
                    "%s{\"close\":\"%d.5\",\"high\":\"1\",\"low\":\"1\",\"tradeTime\":\"2026%02d%02d000000\"}",
                    i ? "," : "", 100 + i, 1 + i / 28, 1 + i % 28);
      json += bar;
    }
    json += "]}}";
    Quote q;
    check(parseCnbcBars(json.data(), json.size(), q) && q.count == kMaxCloses && near(q.price, 299.5),
          "long history keeps the newest");
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
    char url[240];
    coinbaseUrl(url, sizeof(url));
    check(std::strstr(url, "api.exchange.coinbase.com/products/BTC-USD/candles?granularity=86400") != nullptr,
          "btc url");
    cnbcBarsUrl(Asset::Sp500, daysFromCivil(2026, 11, 16), url, sizeof(url));
    check(std::strstr(url, "/bars/.SPX/1D/20260714000000/20261118000000/adjusted/EST5EDT.json") != nullptr, "bars url");
    cnbcQuoteUrl(url, sizeof(url));
    check(std::strstr(url, "symbols=.SPX%7C.IXIC&") != nullptr && std::strlen(url) < sizeof(url) - 1, "quote url");
    check(daysFromCivil(1970, 1, 1) == 0 && daysFromCivil(2026, 11, 16) == 20773, "days from civil");
    check(infoOf(Asset::Bitcoin).always && !infoOf(Asset::Nasdaq).always, "only bitcoin trades around the clock");
  }

  if (failures == 0) std::printf("markets: all tests passed\n");
  return failures == 0 ? 0 : 1;
}
