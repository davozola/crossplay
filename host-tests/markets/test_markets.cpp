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
    {
      // Around the clock there is no close: the day's change is over 24 hours.
      Quote day = q;
      const double candle = dayChangePercent(day);
      const char stats[] =
          "{\"open\":\"50000.00\",\"high\":\"52000\",\"low\":\"49000\",\"last\":\"51000.00\",\"volume\":\"1\"}";
      check(applyCoinbaseStats(stats, sizeof(stats) - 1, day), "coinbase stats apply");
      check(day.price == 51000.0 && previousClose(day) == 50000.0, "stats give the price now and a day ago");
      check(std::fabs(dayChangePercent(day) - 2.0) < 1e-9, "the day's change is the last 24 hours");
      Quote bad = q;
      check(!applyCoinbaseStats("{}", 2, bad) && dayChangePercent(bad) == candle, "no stats keeps the candle's change");
      char statsUrl[128];
      coinbaseStatsUrlFor("BTC-USD", statsUrl, sizeof(statsUrl));
      check(std::strcmp(statsUrl, "https://api.exchange.coinbase.com/products/BTC-USD/stats") == 0, "stats url");
    }
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
    for (int i = 0; i < 400; ++i) {
      char bar[96];
      std::snprintf(bar, sizeof(bar),
                    "%s{\"close\":\"%d.5\",\"high\":\"1\",\"low\":\"1\",\"tradeTime\":\"20%02d%02d%02d000000\"}",
                    i ? "," : "", 100 + i, 25 + i / 336, 1 + (i / 28) % 12, 1 + i % 28);
      json += bar;
    }
    json += "]}}";
    Quote q;
    check(parseCnbcBars(json.data(), json.size(), q) && q.count == kMaxCloses && near(q.price, 499.5),
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

  {  // the catalogue: stems and symbols are all different, and the first three are the cards' defaults
    for (int a = 0; a < kBuiltIn; ++a) {
      for (int b = a + 1; b < kBuiltIn; ++b) {
        check(std::strcmp(instrumentAt(a).file, instrumentAt(b).file) != 0, "cache stems differ");
        check(std::strcmp(instrumentAt(a).symbol, instrumentAt(b).symbol) != 0, "symbols differ");
      }
      check(std::strlen(instrumentAt(a).name) <= 9, instrumentAt(a).name);
      // Around the clock means Coinbase, whose products all end in -USD.
      check(instrumentAt(a).always == (std::strstr(instrumentAt(a).symbol, "-USD") != nullptr), "crypto is Coinbase's");
    }
    check(std::strcmp(instrumentAt(euroInstrument()).symbol, "EUR=") == 0, "the euro is where it says");
    check(std::strcmp(currencyOf(6), "EUR") == 0 && std::strcmp(instrumentAt(6).name, "DAX") == 0 &&
              std::strcmp(instrumentAt(8).name, "STOXX 50") == 0 && std::strcmp(currencyOf(9), "USD") == 0,
          "the European indexes are in euros");
    check(std::strcmp(infoOf(Asset::Sp500).symbol, ".SPX") == 0, "default picks");
    Picks p = defaultPicks();
    p.slot[1] = 9;
    p.slot[2] = 200;
    setPicks(p);
    check(std::strcmp(infoOf(Asset::Sp500).symbol, "AAPL") == 0 && instrumentOf(Asset::Nasdaq) == 2,
          "a pick takes; a bad one falls back");
    setPicks(defaultPicks());
    char url[320];
    cnbcBarsUrlFor("@GC.1", daysFromCivil(2026, 11, 16), url, sizeof(url));
    check(std::strstr(url, "/bars/%40GC.1/1D/") != nullptr, "symbols are escaped in the bars url");
    const char* const symbols[3] = {".SPX", "EUR=", "AAPL"};
    cnbcQuoteUrlFor(symbols, 3, url, sizeof(url));
    check(std::strstr(url, "symbols=.SPX%7CEUR%3D%7CAAPL&requestMethod") != nullptr, "several symbols, escaped");
    coinbaseUrlFor("ETH-USD", url, sizeof(url));
    check(std::strstr(url, "/products/ETH-USD/candles") != nullptr, "any product");
  }
  {  // the portfolio
    Portfolio pf;
    check(!setPosition(pf, euroInstrument(), 5) && pf.count == 0, "the euro rate cannot be held");
    check(setPosition(pf, 1, 2) && setPosition(pf, 1, 0) && pf.count == 0, "an index can");
    check(setPosition(pf, 0, 0.25) && setPosition(pf, 9, 10) && pf.count == 2, "two holdings");
    check(setPosition(pf, 0, 0.5) && pf.count == 2 && near(pf.positions[0].amount, 0.5), "an amount changes in place");
    check(positionOf(pf, 9) == 1 && positionOf(pf, 10) == -1, "found by instrument");
    Priced priced[kMaxPositions];
    priced[0] = {true, 100000, 98000};
    priced[1] = {true, 200, 210};
    Totals t = totalsOf(pf, priced);
    check(near(t.value, 52000) && near(t.previous, 51100) && t.priced == 2, "totals");
    check(near(percentOf(t.value, t.previous), 900.0 / 51100.0 * 100.0) && near(percentOf(5, 0), 0), "percent");
    priced[1].valid = false;
    t = totalsOf(pf, priced);
    check(near(t.value, 50000) && t.priced == 1, "an unpriced holding is left out, not counted as nothing");
    uint8_t bytes[kPortfolioBytes];
    check(packPortfolio(pf, bytes, sizeof(bytes)) == kPortfolioBytes && packPortfolio(pf, bytes, 5) == 0, "pack size");
    Portfolio back;
    check(unpackPortfolio(bytes, sizeof(bytes), back) && back.count == 2 && back.positions[1].instrument == 9 &&
              near(back.positions[1].amount, 10),
          "round trip");
    bytes[2] = static_cast<uint8_t>(euroInstrument());
    check(!unpackPortfolio(bytes, sizeof(bytes), back) && back.count == 0, "a file holding the euro rate is not ours");
    check(setPosition(pf, 9, 0) && pf.count == 1 && pf.positions[0].instrument == 0, "zero removes");
    for (int k = 0; k < kInstruments; ++k) setPosition(pf, k, 1);
    check(pf.count > 6 && pf.count <= kMaxPositions, "holds more than the six it once did, and no more than its room");
    {
      // A file written when a portfolio held six: 2 + 6 * 9 bytes, and still read.
      uint8_t old[2 + 6 * 9] = {1, 1, 0};
      const double amount = 0.5;
      std::memcpy(old + 3, &amount, sizeof(amount));
      Portfolio was;
      check(unpackPortfolio(old, sizeof(old), was) && was.count == 1 && near(was.positions[0].amount, 0.5),
            "the shorter file of before still loads");
      check(!unpackPortfolio(old, 5, was), "a file cut short does not");
    }
  }
  {  // typing an amount
    AmountEntry e;
    for (const char c : {'0', '0', '1', '2', '.', '5', '.', '0', '7'}) entryKey(e, c);
    check(std::strcmp(e.text, "12.507") == 0 && near(entryValue(e), 12.507), "digits and one point");
    check(entryKey(e, '<') && std::strcmp(e.text, "12.50") == 0, "rubbing out");
    AmountEntry dot;
    entryKey(dot, '.');
    check(std::strcmp(dot.text, "0.") == 0 && !entryKey(dot, 'x'), "a leading point");
    for (int k = 0; k < 12; ++k) entryKey(dot, '3');
    check(std::strcmp(dot.text, "0.333333") == 0, "six decimals");
    AmountEntry empty;
    check(!entryKey(empty, '<') && near(entryValue(empty), 0), "nothing to rub out");
    AmountEntry from;
    entryFrom(0.25, from);
    check(std::strcmp(from.text, "0.25") == 0, "from an amount");
    entryFrom(1500, from);
    check(std::strcmp(from.text, "1500") == 0, "from a whole amount");
    char text[24];
    formatAmount(1250.5, text, sizeof(text));
    check(std::strcmp(text, "1,250.5") == 0, "amount grouped");
    formatAmount(0.034567, text, sizeof(text));
    check(std::strcmp(text, "0.034567") == 0, "small amount");
    formatAmount(12, text, sizeof(text));
    check(std::strcmp(text, "12") == 0, "whole amount");
  }

  if (failures == 0) std::printf("markets: all tests passed\n");
  {  // what a period is measured from
    Quote q;
    q.valid = true;
    // A close a day from 1 January 2025 to 10 March 2026.
    const int32_t first = daysFromCivil(2025, 12, 1);
    q.count = 100;
    for (int i = 0; i < q.count; ++i) {
      q.days[i] = first + i;
      q.closes[i] = static_cast<float>(1000 + i);
    }
    q.price = q.closes[q.count - 1];
    check(near(closeBefore(q, Period::Day), 1098), "yesterday's");
    check(near(closeBefore(q, Period::Week), 1092) && near(closeBefore(q, Period::Month), 1069), "a week, a month");
    check(near(closeBefore(q, Period::Ytd), 1030), "the last close of the year before");
    check(near(closeBefore(q, Period::Year), 1000), "the oldest held when a year is not");
    q.count = 1;
    check(near(closeBefore(q, Period::Week), q.price), "nothing to compare with");
  }

  {  // instruments added by ticker or name
    clearCustoms();
    check(!present(kBuiltIn) && present(kBuiltIn - 1) && !present(kInstruments), "the slots begin empty");
    bool recycled = true;
    const int san = addCustom("SAN-ES", "Banco Santander, S.A.", nullptr, &recycled);
    check(san == kBuiltIn && !recycled && present(san), "the first one takes the first slot");
    check(std::strcmp(instrumentAt(san).name, "SAN-ES") == 0 && std::strcmp(instrumentAt(san).symbol, "SAN-ES") == 0,
          "named by its symbol");
    check(std::strcmp(instrumentAt(san).caption, "BANCO SANTANDER S.A.") == 0, instrumentAt(san).caption);
    check(!instrumentAt(san).always && instrumentAt(san).ownable, "CNBC's, and in dollars until it says otherwise");
    check(addCustom("SAN-ES", "", nullptr, nullptr) == san, "adding it again finds it");
    check(addCustom("AAPL", "", nullptr, nullptr) == 9, "a catalogue symbol is the catalogue's");
    check(addCustom("", "", nullptr, nullptr) == -1 && addCustom("TOO-LONG-SYMBOL", "", nullptr, nullptr) == -1 &&
              addCustom("a b", "", nullptr, nullptr) == -1,
          "what cannot be a symbol is refused");
    check(setCustomCurrency(san, "EUR") && !setCustomCurrency(san, "EUR") && !setCustomCurrency(3, "EUR"),
          "its currency is learnt once");
    check(std::strcmp(currencyOf(san), "EUR") == 0 && instrumentAt(san).ownable, "euros can be counted");
    check(setCustomCurrency(san, "GBP") && !instrumentAt(san).ownable && setCustomCurrency(san, "EUR"),
          "other currencies cannot");
    Picks p = defaultPicks();
    p.slot[0] = static_cast<uint8_t>(san);
    p.slot[1] = static_cast<uint8_t>(san + 1);
    setPicks(p);
    check(instrumentOf(Asset::Bitcoin) == san && instrumentOf(Asset::Sp500) == 1, "an empty slot is not a pick");
    setPicks(defaultPicks());

    char packed[kCustomsBytes];
    const size_t size = packCustoms(packed, sizeof(packed));
    check(size > 0, "packs");
    clearCustoms();
    check(!present(san), "cleared");
    unpackCustoms(packed, size);
    check(present(san) && std::strcmp(instrumentAt(san).symbol, "SAN-ES") == 0 &&
              std::strcmp(currencyOf(san), "EUR") == 0 &&
              std::strcmp(instrumentAt(san).caption, "BANCO SANTANDER S.A.") == 0,
          "and comes back in its slot");

    char name[kSymbolChars];
    for (int i = 1; i < kCustom; ++i) {
      std::snprintf(name, sizeof(name), "T%d", i);
      check(addCustom(name, "", nullptr, nullptr) == kBuiltIn + i, "fills the slots in order");
    }
    bool used[kCustom];
    for (int i = 0; i < kCustom; ++i) used[i] = true;
    check(addCustom("FULL", "", used, &recycled) == -1, "full, with every slot in use");
    used[4] = false;
    check(addCustom("FULL", "", used, &recycled) == kBuiltIn + 4 && recycled, "the spare slot is taken over");
    check(findSymbol("T4") == -1 && findSymbol("FULL") == kBuiltIn + 4, "and what it held is gone");
    clearCustoms();
  }

  {  // searching
    char query[24];
    tidyQuery("  banco   santander ", query, sizeof(query));
    check(std::strcmp(query, "BANCO SANTANDER") == 0, query);
    check(!tickerLike(query) && tickerLike("SAN-ES") && tickerLike("BRK.B") && !tickerLike(""),
          "what could be a symbol");
    char url[200];
    lookupUrl(query, url, sizeof(url));
    check(std::strcmp(url,
                      "https://symlookup.cnbc.com/symservice/symlookup.do?prefix=BANCO%20SANTANDER"
                      "&partnerid=20064&pgok=1&pgsize=12") == 0,
          url);
    const char* answer =
        "[{\"prefix\":\"SAN\",\"count\":\"3\"},"
        "{\"symbolName\":\"SAN-ES\",\"companyName\":\"Banco Santander SA\",\"exchangeName\":\"Madrid\"},"
        "{\"companyName\":\"Sanofi\",\"exchangeName\":\"NASDAQ\",\"symbolName\":\"SNY\"},"
        "{\"symbolName\":\"SAN-ES\",\"companyName\":\"again\"},"
        "{\"symbolName\":\"A SYMBOL TOO LONG\",\"companyName\":\"x\"},"
        "{\"symbolName\":\"SAN\",\"companyName\":\"Banco Santander \\\"ADR\\\"\"}]";
    Found found[4];
    const int n = parseLookup(answer, std::strlen(answer), found, 4);
    check(n == 3, "three usable results");
    check(std::strcmp(found[0].symbol, "SAN-ES") == 0 && std::strcmp(found[0].company, "BANCO SANTANDER SA") == 0 &&
              std::strcmp(found[0].exchange, "MADRID") == 0,
          "the first, with its exchange");
    check(std::strcmp(found[1].symbol, "SNY") == 0 && std::strcmp(found[1].company, "SANOFI") == 0,
          "whatever order the keys come in");
    check(std::strcmp(found[2].symbol, "SAN") == 0 && std::strcmp(found[2].company, "BANCO SANTANDER ADR") == 0,
          found[2].company);
    check(parseLookup("<html>no</html>", 15, found, 4) == 0 && parseLookup("", 0, found, 4) == 0,
          "nothing in an error page");
    check(parseLookup(answer, std::strlen(answer), found, 1) == 1, "no more than asked for");
  }

  // A history is kept for half a day, and what never closes only within its UTC day.
  {
    using namespace markets;
    const int64_t noon = 1795000000 - (1795000000 % 86400) + 12 * 3600;
    Quote q;
    q.valid = true;
    q.count = 2;
    q.days[0] = static_cast<int32_t>(noon / 86400) - 1;
    q.days[1] = static_cast<int32_t>(noon / 86400);
    check(!historyFresh(q, noon, false), "never asked: not fresh");
    q.historyAt = noon;
    check(historyFresh(q, noon + 60, false) && historyFresh(q, noon + 60, true), "a minute later: kept");
    check(historyFresh(q, noon + kHistoryFreshSeconds - 1, false), "kept to the end of the half day");
    check(!historyFresh(q, noon + kHistoryFreshSeconds, false), "and not past it");
    q.historyAt = noon + 6 * 3600;
    check(historyFresh(q, noon + 13 * 3600, false), "bars from the evening hold through the night");
    check(!historyFresh(q, noon + 13 * 3600, true), "past midnight UTC the candles have turned");
    q.historyAt = noon;
    check(!historyFresh(q, noon - 60, false), "a clock gone backwards keeps nothing");
    check(!historyFresh(q, 1000, false), "nor does one that was never set");
    q.valid = false;
    check(!historyFresh(q, noon + 60, false), "nothing held, nothing kept");
  }

  return failures == 0 ? 0 : 1;
}
