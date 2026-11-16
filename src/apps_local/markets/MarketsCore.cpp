#include "MarketsCore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../json/FlatJson.h"

namespace markets {

namespace {

using flatjson::element;
using flatjson::forEach;
using flatjson::member;
using flatjson::numberD;
using flatjson::Span;

// Floor division, so times before 1970 and negative offsets land on the right day.
int64_t floorDiv(const int64_t a, const int64_t b) { return a / b - ((a % b != 0 && (a < 0) != (b < 0)) ? 1 : 0); }

// Groups the integer part in threes: 104250 -> "104,250".
void grouped(const double magnitude, const int decimals, char* out, const size_t cap) {
  char raw[40];
  std::snprintf(raw, sizeof(raw), "%.*f", decimals, magnitude);
  const char* dot = std::strchr(raw, '.');
  const int whole = static_cast<int>(dot ? dot - raw : static_cast<long>(std::strlen(raw)));
  size_t at = 0;
  for (int i = 0; i < whole && at + 1 < cap; ++i) {
    if (i > 0 && (whole - i) % 3 == 0 && at + 2 < cap) out[at++] = ',';
    out[at++] = raw[i];
  }
  for (const char* p = dot; p && *p && at + 1 < cap; ++p) out[at++] = *p;
  out[at] = 0;
}

int decimalsFor(const double magnitude) { return magnitude >= 10000 ? 0 : 2; }

}  // namespace

namespace {

constexpr AssetInfo kCatalogue[kInstruments] = {
    {"BITCOIN", "BTC / USD", "BTC-USD", "btc", true, true},
    {"S&P 500", "US LARGE CAPS", ".SPX", "sp500", false, false},
    {"NASDAQ", "COMPOSITE", ".IXIC", "nasdaq", false, false},
    {"ETHEREUM", "ETH / USD", "ETH-USD", "eth", true, true},
    {"SOLANA", "SOL / USD", "SOL-USD", "sol", true, true},
    {"DOW JONES", "INDUSTRIALS", ".DJI", "dji", false, false},
    {"DAX", "GERMANY 40", ".GDAXI", "dax", false, false},
    {"IBEX 35", "SPAIN", ".IBEX", "ibex", false, false},
    {"STOXX 50", "EURO ZONE", ".STOXX50E", "stoxx", false, false},
    {"APPLE", "AAPL", "AAPL", "aapl", false, true},
    {"MICROSOFT", "MSFT", "MSFT", "msft", false, true},
    {"NVIDIA", "NVDA", "NVDA", "nvda", false, true},
    {"AMAZON", "AMZN", "AMZN", "amzn", false, true},
    {"ALPHABET", "GOOGL", "GOOGL", "googl", false, true},
    {"META", "META", "META", "meta", false, true},
    {"TESLA", "TSLA", "TSLA", "tsla", false, true},
    {"SPY", "S&P 500 ETF", "SPY", "spy", false, true},
    {"QQQ", "NASDAQ 100 ETF", "QQQ", "qqq", false, true},
    {"GLD", "GOLD ETF", "GLD", "gld", false, true},
    {"GOLD", "USD / OUNCE", "@GC.1", "gold", false, false},
    {"EUR / USD", "EURO IN DOLLARS", "EUR=", "eurusd", false, false},
};
constexpr int kEuro = 20;

Picks current = {{0, 1, 2}};

// A symbol as it goes in a URL: CNBC's use '@', '=' and '&'.
void escape(const char* symbol, char* out, const size_t cap) {
  size_t at = 0;
  for (const char* p = symbol; *p && at + 4 < cap; ++p) {
    const char c = *p;
    const bool plain =
        (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_';
    if (plain) {
      out[at++] = c;
    } else {
      static const char kHex[] = "0123456789ABCDEF";
      out[at++] = '%';
      out[at++] = kHex[(static_cast<unsigned char>(c) >> 4) & 15];
      out[at++] = kHex[static_cast<unsigned char>(c) & 15];
    }
  }
  out[at] = 0;
}

}  // namespace

const AssetInfo& instrumentAt(const int instrument) {
  return kCatalogue[instrument >= 0 && instrument < kInstruments ? instrument : 0];
}

const char* currencyOf(const int instrument) { return instrument >= 6 && instrument <= 8 ? "EUR" : "USD"; }

int euroInstrument() { return kEuro; }

Picks defaultPicks() { return Picks{{0, 1, 2}}; }

void setPicks(const Picks& picks) {
  const Picks fallback = defaultPicks();
  for (int i = 0; i < kAssets; ++i) current.slot[i] = picks.slot[i] < kInstruments ? picks.slot[i] : fallback.slot[i];
}

Picks picks() { return current; }

int instrumentOf(const Asset asset) {
  const int i = static_cast<int>(asset);
  return current.slot[i >= 0 && i < kAssets ? i : 0];
}

const AssetInfo& infoOf(const Asset asset) { return instrumentAt(instrumentOf(asset)); }

void coinbaseUrlFor(const char* product, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://api.exchange.coinbase.com/products/%s/candles?granularity=86400", product);
}

void coinbaseUrl(char* out, const size_t cap) { coinbaseUrlFor(kCatalogue[0].symbol, out, cap); }

void cnbcBarsUrlFor(const char* symbol, const int32_t today, char* out, const size_t cap) {
  int y0 = 0, m0 = 0, d0 = 0, y1 = 0, m1 = 0, d1 = 0;
  civilOfDay(today - 125, y0, m0, d0);
  civilOfDay(today + 2, y1, m1, d1);
  char escaped[40];
  escape(symbol, escaped, sizeof(escaped));
  std::snprintf(
      out, cap,
      "https://ts-api.cnbc.com/harmony/app/bars/%s/1D/%04d%02d%02d000000/%04d%02d%02d000000/adjusted/EST5EDT.json",
      escaped, y0 % 10000, m0 % 100, d0 % 100, y1 % 10000, m1 % 100, d1 % 100);
}

void cnbcBarsUrl(const Asset asset, const int32_t today, char* out, const size_t cap) {
  cnbcBarsUrlFor(infoOf(asset).symbol, today, out, cap);
}

void cnbcQuoteUrlFor(const char* const* symbols, const int count, char* out, const size_t cap) {
  int at = std::snprintf(out, cap, "https://quote.cnbc.com/quote-html-webservice/restQuote/symbolType/symbol?symbols=");
  for (int i = 0; i < count && at > 0 && static_cast<size_t>(at) + 48 < cap; ++i) {
    char escaped[40];
    escape(symbols[i], escaped, sizeof(escaped));
    at += std::snprintf(out + at, cap - static_cast<size_t>(at), "%s%s", i > 0 ? "%7C" : "", escaped);
  }
  if (at > 0 && static_cast<size_t>(at) < cap) {
    std::snprintf(out + at, cap - static_cast<size_t>(at),
                  "&requestMethod=itv&noform=1&partnerId=2&fund=1&exthrs=1&output=json");
  }
}

void cnbcQuoteUrl(char* out, const size_t cap) {
  const char* const symbols[2] = {kCatalogue[1].symbol, kCatalogue[2].symbol};
  cnbcQuoteUrlFor(symbols, 2, out, cap);
}

int32_t daysFromCivil(int year, const int month, const int day) {
  // Howard Hinnant's days_from_civil.
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const int yoe = year - era * 400;
  const int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

namespace {

// A number that may arrive quoted and grouped: "7,561.67". Zero when it is not one.
double loose(const Span& v) {
  double d = 0;
  if (numberD(v, d)) return d;
  char text[32];
  flatjson::string(v, text, sizeof(text));
  char plain[32];
  size_t at = 0;
  for (const char* c = text; *c && at + 1 < sizeof(plain); ++c) {
    if (*c != ',') plain[at++] = *c;
  }
  plain[at] = 0;
  char* end = nullptr;
  d = std::strtod(plain, &end);
  return end != plain ? d : 0;
}

int digitsOf(const char* p, const int n) {
  int v = 0;
  for (int i = 0; i < n; ++i) {
    if (p[i] < '0' || p[i] > '9') return -1;
    v = v * 10 + (p[i] - '0');
  }
  return v;
}

Span arrayOf(const char* json, const size_t len) {
  Span s;
  s.p = flatjson::skipSpace(json, json + len);
  s.end = json + len;
  if (s.p >= s.end || *s.p != '[') s.p = nullptr;
  return s;
}

}  // namespace

bool parseCoinbase(const char* json, const size_t len, Quote& out) {
  out = Quote{};
  const Span candles = arrayOf(json, len);
  if (!candles.ok()) return false;
  // Newest first. The first kMaxCloses are kept, the rest only widen the range.
  float newest[kMaxCloses];
  int32_t newestDay[kMaxCloses];
  int kept = 0;
  double high = 0, low = 0;
  forEach(candles, 400, [&](const int i, const Span& candle) {
    double t = 0, lo = 0, hi = 0, close = 0, volume = 0;
    if (!numberD(element(candle, 0), t) || !numberD(element(candle, 1), lo) || !numberD(element(candle, 2), hi) ||
        !numberD(element(candle, 4), close) || close <= 0) {
      return;
    }
    numberD(element(candle, 5), volume);
    if (kept == 0) {
      out.price = close;
      out.dayHigh = hi;
      out.dayLow = lo;
      out.volume = volume;
      out.time = static_cast<int64_t>(t);
    }
    if (high == 0 || hi > high) high = hi;
    if (low == 0 || lo < low) low = lo;
    if (kept < kMaxCloses && i < 400) {
      newest[kept] = static_cast<float>(close);
      newestDay[kept] = static_cast<int32_t>(floorDiv(static_cast<int64_t>(t), 86400));
      ++kept;
    }
  });
  if (kept < 2) return false;
  for (int i = 0; i < kept; ++i) {
    out.closes[i] = newest[kept - 1 - i];
    out.days[i] = newestDay[kept - 1 - i];
  }
  out.count = kept;
  out.yearHigh = high;
  out.yearLow = low;
  std::snprintf(out.currency, sizeof(out.currency), "USD");
  std::snprintf(out.zone, sizeof(out.zone), "UTC");
  out.valid = true;
  return true;
}

bool parseCnbcBars(const char* json, const size_t len, Quote& out) {
  out = Quote{};
  const Span bars = member(member(flatjson::root(json, len), "barData"), "priceBars");
  if (!bars.ok()) return false;
  int total = 0;
  forEach(bars, 4096, [&](int, const Span&) { ++total; });
  const int skip = total > kMaxCloses ? total - kMaxCloses : 0;
  forEach(bars, 4096, [&](const int i, const Span& bar) {
    if (i < skip || out.count >= kMaxCloses) return;
    char stamp[16];
    flatjson::string(member(bar, "tradeTime"), stamp, sizeof(stamp));
    const int year = digitsOf(stamp, 4);
    const int month = digitsOf(stamp + 4, 2);
    const int day = digitsOf(stamp + 6, 2);
    const double close = loose(member(bar, "close"));
    if (std::strlen(stamp) < 8 || year < 0 || month < 1 || month > 12 || day < 1 || close <= 0) return;
    out.closes[out.count] = static_cast<float>(close);
    out.days[out.count] = daysFromCivil(year, month, day);
    ++out.count;
    out.price = close;
    out.dayHigh = loose(member(bar, "high"));
    out.dayLow = loose(member(bar, "low"));
    out.volume = loose(member(bar, "volume"));
  });
  if (out.count < 2) return false;
  // Until a quote says otherwise: the year's range is the range held, and the
  // price is the last close, stamped at that day's closing bell in New York.
  float lo = 0, hi = 0;
  closeRange(out, 0, lo, hi);
  out.yearHigh = hi;
  out.yearLow = lo;
  out.gmtOffset = -18000;
  out.time = static_cast<int64_t>(out.days[out.count - 1]) * 86400 + 16 * 3600 - out.gmtOffset;
  std::snprintf(out.currency, sizeof(out.currency), "USD");
  std::snprintf(out.zone, sizeof(out.zone), "EST");
  out.valid = true;
  return true;
}

bool applyCnbcQuote(const char* json, const size_t len, const Asset asset, Quote& q) {
  return applyCnbcQuoteFor(json, len, infoOf(asset).symbol, q);
}

bool applyCnbcQuoteFor(const char* json, const size_t len, const char* want, Quote& q) {
  if (!q.valid || q.count < 1) return false;
  const Span list = member(member(flatjson::root(json, len), "FormattedQuoteResult"), "FormattedQuote");
  if (!list.ok()) return false;
  bool found = false;
  forEach(list, 16, [&](int, const Span& quote) {
    char symbol[12];
    flatjson::string(member(quote, "symbol"), symbol, sizeof(symbol));
    if (found || std::strcmp(symbol, want) != 0) return;
    const double last = loose(member(quote, "last"));
    if (last <= 0) return;
    found = true;
    char when[12];
    flatjson::string(member(quote, "last_time"), when, sizeof(when));
    const int year = digitsOf(when, 4);
    const int month = digitsOf(when + 5, 2);
    const int day = digitsOf(when + 8, 2);
    if (year > 0 && month >= 1 && month <= 12 && day >= 1) {
      const int32_t today = daysFromCivil(year, month, day);
      // A quote older than the bars adds nothing but its year's range.
      const bool stale = today < q.days[q.count - 1];
      if (today > q.days[q.count - 1]) {
        // A session the bars do not hold yet: it becomes the newest close.
        if (q.count == kMaxCloses) {
          for (int i = 1; i < kMaxCloses; ++i) {
            q.closes[i - 1] = q.closes[i];
            q.days[i - 1] = q.days[i];
          }
          --q.count;
        }
        q.days[q.count++] = today;
        q.volume = 0;
      }
      if (!stale) {
        q.closes[q.count - 1] = static_cast<float>(last);
        q.time = static_cast<int64_t>(today) * 86400 + 16 * 3600 - q.gmtOffset;
        q.price = last;
      }
    } else {
      q.closes[q.count - 1] = static_cast<float>(last);
      q.price = last;
    }
    const double high = loose(member(quote, "high"));
    const double low = loose(member(quote, "low"));
    if (high > 0 && low > 0 && q.price == last) {
      q.dayHigh = high;
      q.dayLow = low;
    }
    const double yearHigh = loose(member(quote, "yrhiprice"));
    const double yearLow = loose(member(quote, "yrloprice"));
    if (yearHigh > 0 && yearLow > 0) {
      q.yearHigh = yearHigh;
      q.yearLow = yearLow;
    }
  });
  return found;
}

double previousClose(const Quote& q) { return q.count >= 2 ? static_cast<double>(q.closes[q.count - 2]) : q.price; }

double dayChange(const Quote& q) { return q.price - previousClose(q); }

double dayChangePercent(const Quote& q) {
  const double prev = previousClose(q);
  return prev > 0 ? (q.price - prev) / prev * 100.0 : 0;
}

bool changeOver(const Quote& q, const int sessions, double& percent) {
  if (q.count < 2 || sessions < 1) return false;
  int at = q.count - 1 - sessions;
  if (at < 0) at = 0;
  const double then = static_cast<double>(q.closes[at]);
  if (then <= 0) return false;
  percent = (q.price - then) / then * 100.0;
  return true;
}

void closeRange(const Quote& q, const int sessions, float& lo, float& hi) {
  int from = sessions > 0 ? q.count - sessions : 0;
  if (from < 0) from = 0;
  lo = hi = q.count > 0 ? q.closes[from] : 0;
  for (int i = from; i < q.count; ++i) {
    if (q.closes[i] < lo) lo = q.closes[i];
    if (q.closes[i] > hi) hi = q.closes[i];
  }
}

bool sessionOpen(const Quote& q) {
  return q.sessionEnd > q.sessionStart && q.time >= q.sessionStart && q.time < q.sessionEnd - 60;
}

void formatPrice(const double value, char* out, const size_t cap) {
  const double m = std::fabs(value);
  grouped(m, decimalsFor(m), out, cap);
}

void formatPriceShort(const double value, char* out, const size_t cap) {
  const double m = std::fabs(value);
  grouped(m, m >= 1000 ? 0 : 2, out, cap);
}

void formatChange(const double value, char* out, const size_t cap) {
  if (cap < 2) return;
  const double m = std::fabs(value);
  // The sign follows the rounded figure, so a change that shows as 0.00 is not
  // dressed as a loss.
  const int decimals = m >= 1000 ? 0 : 2;
  char body[32];
  grouped(m, decimals, body, sizeof(body));
  bool zero = true;
  for (const char* p = body; *p; ++p) {
    if (*p >= '1' && *p <= '9') zero = false;
  }
  if (zero) {
    std::snprintf(out, cap, "%s", body);
  } else {
    out[0] = value < 0 ? '-' : '+';
    std::snprintf(out + 1, cap - 1, "%s", body);
  }
}

void formatPercent(const double percent, char* out, const size_t cap) {
  const double rounded = std::round(percent * 100.0) / 100.0;
  if (rounded == 0) {
    std::snprintf(out, cap, "0.00%%");
  } else {
    std::snprintf(out, cap, "%+.2f%%", rounded);
  }
}

void formatVolume(const double value, char* out, const size_t cap) {
  const double m = std::fabs(value);
  const char* unit = "";
  double scaled = m;
  if (m >= 1e12) {
    scaled = m / 1e12;
    unit = "T";
  } else if (m >= 1e9) {
    scaled = m / 1e9;
    unit = "B";
  } else if (m >= 1e6) {
    scaled = m / 1e6;
    unit = "M";
  } else if (m >= 1e3) {
    scaled = m / 1e3;
    unit = "K";
  }
  if (!unit[0]) {
    std::snprintf(out, cap, "%.0f", scaled);
  } else if (scaled >= 100) {
    std::snprintf(out, cap, "%.0f%s", scaled, unit);
  } else if (scaled >= 10) {
    std::snprintf(out, cap, "%.1f%s", scaled, unit);
  } else {
    std::snprintf(out, cap, "%.2f%s", scaled, unit);
  }
}

void civilOfDay(const int32_t days, int& year, int& month, int& day) {
  // Howard Hinnant's days-to-civil.
  const int64_t z = static_cast<int64_t>(days) + 719468;
  const int64_t era = floorDiv(z, 146097);
  const int64_t doe = z - era * 146097;
  const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const int64_t mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
}

Civil civilOf(const int64_t unixSeconds, const int32_t gmtOffset) {
  const int64_t local = unixSeconds + gmtOffset;
  const int64_t days = floorDiv(local, 86400);
  const int64_t rest = local - days * 86400;
  Civil c{};
  civilOfDay(static_cast<int32_t>(days), c.year, c.month, c.day);
  c.hour = static_cast<int>(rest / 3600);
  c.minute = static_cast<int>((rest % 3600) / 60);
  return c;
}

const char* monthName(const int month) {
  static const char* const kNames[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                         "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return month >= 1 && month <= 12 ? kNames[month - 1] : "";
}

bool setPosition(Portfolio& portfolio, const int instrument, const double amount) {
  if (instrument < 0 || instrument >= kInstruments || !kCatalogue[instrument].ownable) return false;
  const int at = positionOf(portfolio, instrument);
  if (!(amount > 0)) {
    if (at < 0) return false;
    for (int i = at + 1; i < portfolio.count; ++i) portfolio.positions[i - 1] = portfolio.positions[i];
    --portfolio.count;
    return true;
  }
  if (at >= 0) {
    portfolio.positions[at].amount = amount;
    return true;
  }
  if (portfolio.count >= kMaxPositions) return false;
  portfolio.positions[portfolio.count].instrument = static_cast<uint8_t>(instrument);
  portfolio.positions[portfolio.count].amount = amount;
  ++portfolio.count;
  return true;
}

int positionOf(const Portfolio& portfolio, const int instrument) {
  for (int i = 0; i < portfolio.count; ++i) {
    if (portfolio.positions[i].instrument == instrument) return i;
  }
  return -1;
}

Totals totalsOf(const Portfolio& portfolio, const Priced* priced) {
  Totals t;
  for (int i = 0; i < portfolio.count; ++i) {
    if (!priced[i].valid) continue;
    t.value += portfolio.positions[i].amount * priced[i].price;
    t.previous += portfolio.positions[i].amount * priced[i].previous;
    ++t.priced;
  }
  return t;
}

double percentOf(const double now, const double before) { return before > 0 ? (now - before) / before * 100.0 : 0; }

size_t packPortfolio(const Portfolio& portfolio, uint8_t* out, const size_t cap) {
  if (cap < kPortfolioBytes) return 0;
  std::memset(out, 0, kPortfolioBytes);
  out[0] = 1;
  out[1] = static_cast<uint8_t>(portfolio.count);
  for (int i = 0; i < portfolio.count; ++i) {
    uint8_t* o = out + 2 + i * 9;
    o[0] = portfolio.positions[i].instrument;
    std::memcpy(o + 1, &portfolio.positions[i].amount, 8);
  }
  return kPortfolioBytes;
}

bool unpackPortfolio(const uint8_t* in, const size_t size, Portfolio& portfolio) {
  portfolio.count = 0;
  if (size != kPortfolioBytes || in[0] != 1 || in[1] > kMaxPositions) return false;
  Portfolio fresh;
  for (int i = 0; i < in[1]; ++i) {
    const uint8_t* o = in + 2 + i * 9;
    double amount = 0;
    std::memcpy(&amount, o + 1, 8);
    // Anything that is not a plain positive amount of something holdable is
    // not a file this wrote.
    if (!(amount > 0) || amount > 1e12 || !setPosition(fresh, o[0], amount)) return false;
  }
  if (fresh.count != in[1]) return false;
  portfolio = fresh;
  return true;
}

bool entryKey(AmountEntry& entry, const char key) {
  const size_t n = std::strlen(entry.text);
  if (key == '<') {
    if (n == 0) return false;
    entry.text[n - 1] = 0;
    return true;
  }
  const bool hasPoint = std::strchr(entry.text, '.') != nullptr;
  if (key == '.') {
    if (hasPoint || n + 2 >= sizeof(entry.text)) return false;
    if (n == 0) {
      std::snprintf(entry.text, sizeof(entry.text), "0.");
      return true;
    }
  } else if (key < '0' || key > '9') {
    return false;
  } else if (hasPoint && n - static_cast<size_t>(std::strchr(entry.text, '.') - entry.text) > 6) {
    return false;  // six decimals is as fine as an amount is kept
  }
  if (n + 1 >= sizeof(entry.text)) return false;
  // A leading zero is replaced, not followed.
  if (n == 1 && entry.text[0] == '0' && key != '.') {
    entry.text[0] = key;
    return true;
  }
  entry.text[n] = key;
  entry.text[n + 1] = 0;
  return true;
}

double entryValue(const AmountEntry& entry) {
  double whole = 0;
  double frac = 0;
  double scale = 1;
  bool after = false;
  for (const char* p = entry.text; *p; ++p) {
    if (*p == '.') {
      after = true;
    } else if (after) {
      scale /= 10;
      frac += (*p - '0') * scale;
    } else {
      whole = whole * 10 + (*p - '0');
    }
  }
  return whole + frac;
}

void entryFrom(const double amount, AmountEntry& entry) {
  char plain[32];
  std::snprintf(plain, sizeof(plain), "%.6f", amount > 0 ? amount : 0);
  size_t n = std::strlen(plain);
  while (n > 0 && plain[n - 1] == '0') plain[--n] = 0;
  if (n > 0 && plain[n - 1] == '.') plain[--n] = 0;
  if (n >= sizeof(entry.text) || amount <= 0) {
    entry.text[0] = 0;
    return;
  }
  std::memcpy(entry.text, plain, n + 1);
}

void formatAmount(const double amount, char* out, const size_t cap) {
  char plain[32];
  std::snprintf(plain, sizeof(plain), "%.6f", amount);
  size_t n = std::strlen(plain);
  while (n > 0 && plain[n - 1] == '0') plain[--n] = 0;
  if (n > 0 && plain[n - 1] == '.') plain[--n] = 0;
  const char* dot = std::strchr(plain, '.');
  const int whole = static_cast<int>(dot ? dot - plain : static_cast<long>(n));
  size_t at = 0;
  for (int i = 0; i < whole && at + 1 < cap; ++i) {
    if (i > 0 && (whole - i) % 3 == 0 && at + 2 < cap) out[at++] = ',';
    out[at++] = plain[i];
  }
  for (const char* q = dot; q && *q && at + 1 < cap; ++q) out[at++] = *q;
  out[at] = 0;
}

}  // namespace markets
