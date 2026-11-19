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

constexpr AssetInfo kCatalogue[kBuiltIn] = {
    {"BITCOIN", "BTC / USD", "BTC-USD", "btc", true, true},
    {"S&P 500", "US LARGE CAPS", ".SPX", "sp500", false, true},
    {"NASDAQ", "COMPOSITE", ".IXIC", "nasdaq", false, true},
    {"ETHEREUM", "ETH / USD", "ETH-USD", "eth", true, true},
    {"SOLANA", "SOL / USD", "SOL-USD", "sol", true, true},
    {"DOW JONES", "INDUSTRIALS", ".DJI", "dji", false, true},
    {"DAX", "GERMANY 40", ".GDAXI", "dax", false, true},
    {"IBEX 35", "SPAIN", ".IBEX", "ibex", false, true},
    {"STOXX 50", "EURO ZONE", ".STOXX50E", "stoxx", false, true},
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
    {"GOLD", "USD / OUNCE", "@GC.1", "gold", false, true},
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

namespace {

// What the owner added. `info` points into the slot's own strings.
struct CustomSlot {
  char symbol[kSymbolChars] = "";
  char name[10] = "";
  char company[kCompanyChars] = "";
  char currency[4] = "";
  AssetInfo info = {"", "", "", "", false, false};
};
CustomSlot customs[kCustom];
constexpr const char* kCustomFiles[kCustom] = {"c0",  "c1",  "c2",  "c3",  "c4",  "c5",  "c6",  "c7",
                                               "c8",  "c9",  "c10", "c11", "c12", "c13", "c14", "c15",
                                               "c16", "c17", "c18", "c19", "c20", "c21", "c22", "c23"};

void refreshSlot(const int slot) {
  CustomSlot& c = customs[slot];
  std::memcpy(c.name, c.symbol, sizeof(c.name) - 1);
  c.name[sizeof(c.name) - 1] = 0;
  // Held in dollars or in euros, which the total knows how to count.
  const bool countable =
      c.currency[0] == 0 || std::strcmp(c.currency, "USD") == 0 || std::strcmp(c.currency, "EUR") == 0;
  c.info = AssetInfo{c.name, c.company[0] ? c.company : c.symbol, c.symbol, kCustomFiles[slot], false, countable};
}

bool symbolChar(const char c) {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '=' || c == '@' || c == '^';
}

// Capitals the pixel faces have, and nothing else.
void tidyInto(const char* in, char* out, const size_t cap) {
  size_t at = 0;
  bool space = false;
  for (; in != nullptr && *in != 0 && at + 1 < cap; ++in) {
    char c = *in;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    const bool keep = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '&' ||
                      c == '=' || c == '@' || c == '^' || c == '/';
    if (!keep) {
      space = at > 0;
      continue;
    }
    if (space && at + 2 < cap) out[at++] = ' ';
    space = false;
    out[at++] = c;
  }
  if (cap > 0) out[at] = 0;
}

}  // namespace

const AssetInfo& instrumentAt(const int instrument) {
  if (instrument >= kBuiltIn && instrument < kInstruments) return customs[instrument - kBuiltIn].info;
  return kCatalogue[instrument >= 0 && instrument < kBuiltIn ? instrument : 0];
}

bool present(const int instrument) {
  if (instrument < 0 || instrument >= kInstruments) return false;
  return instrument < kBuiltIn || customs[instrument - kBuiltIn].symbol[0] != 0;
}

const char* currencyOf(const int instrument) {
  if (instrument >= kBuiltIn && instrument < kInstruments) {
    const char* c = customs[instrument - kBuiltIn].currency;
    return c[0] ? c : "USD";
  }
  return instrument >= 6 && instrument <= 8 ? "EUR" : "USD";
}

int findSymbol(const char* symbol) {
  if (symbol == nullptr || *symbol == 0) return -1;
  for (int i = 0; i < kInstruments; ++i) {
    if (present(i) && std::strcmp(instrumentAt(i).symbol, symbol) == 0) return i;
  }
  return -1;
}

int addCustom(const char* symbol, const char* company, const bool* used, bool* recycled) {
  if (recycled != nullptr) *recycled = false;
  if (symbol == nullptr || *symbol == 0 || std::strlen(symbol) >= kSymbolChars) return -1;
  for (const char* c = symbol; *c != 0; ++c) {
    if (!symbolChar(*c)) return -1;
  }
  const int already = findSymbol(symbol);
  if (already >= 0) return already;
  int slot = -1;
  for (int i = 0; i < kCustom && slot < 0; ++i) {
    if (customs[i].symbol[0] == 0) slot = i;
  }
  for (int i = 0; i < kCustom && slot < 0; ++i) {
    if (used == nullptr || !used[i]) {
      slot = i;
      if (recycled != nullptr) *recycled = true;
    }
  }
  if (slot < 0) return -1;
  CustomSlot& c = customs[slot];
  std::snprintf(c.symbol, sizeof(c.symbol), "%s", symbol);
  tidyInto(company, c.company, sizeof(c.company));
  c.currency[0] = 0;
  refreshSlot(slot);
  return kBuiltIn + slot;
}

bool setCustomCurrency(const int instrument, const char* currency) {
  if (instrument < kBuiltIn || instrument >= kInstruments || currency == nullptr) return false;
  CustomSlot& c = customs[instrument - kBuiltIn];
  if (c.symbol[0] == 0 || std::strlen(currency) != 3 || std::strcmp(c.currency, currency) == 0) return false;
  std::snprintf(c.currency, sizeof(c.currency), "%s", currency);
  refreshSlot(instrument - kBuiltIn);
  return true;
}

void clearCustoms() {
  for (int i = 0; i < kCustom; ++i) {
    customs[i] = CustomSlot{};
    refreshSlot(i);
  }
}

size_t packCustoms(char* out, const size_t cap) {
  size_t at = 0;
  for (int i = 0; i < kCustom; ++i) {
    const CustomSlot& c = customs[i];
    const int n = std::snprintf(out + at, cap - at, "%s\t%s\t%s\n", c.symbol, c.company, c.currency);
    if (n < 0 || static_cast<size_t>(n) >= cap - at) return 0;
    at += static_cast<size_t>(n);
  }
  return at;
}

void unpackCustoms(const char* in, const size_t len) {
  clearCustoms();
  size_t at = 0;
  for (int slot = 0; slot < kCustom && at < len; ++slot) {
    char field[3][kCompanyChars] = {"", "", ""};
    int which = 0;
    size_t n = 0;
    for (; at < len && in[at] != '\n'; ++at) {
      if (in[at] == '\t') {
        if (which < 2) ++which;
        n = 0;
      } else if (n + 1 < sizeof(field[0])) {
        field[which][n++] = in[at];
        field[which][n] = 0;
      }
    }
    ++at;
    if (field[0][0] == 0 || std::strlen(field[0]) >= kSymbolChars) continue;
    // Straight into its own slot, so the cache files keep meaning what they did.
    CustomSlot& c = customs[slot];
    std::snprintf(c.symbol, sizeof(c.symbol), "%.*s", static_cast<int>(sizeof(c.symbol) - 1), field[0]);
    std::snprintf(c.company, sizeof(c.company), "%s", field[1]);
    if (std::strlen(field[2]) == 3) std::snprintf(c.currency, sizeof(c.currency), "%.3s", field[2]);
    refreshSlot(slot);
  }
}

void tidyQuery(const char* typed, char* out, const size_t cap) { tidyInto(typed, out, cap); }

bool tickerLike(const char* query) {
  if (query == nullptr || *query == 0 || std::strlen(query) >= kSymbolChars) return false;
  for (const char* c = query; *c != 0; ++c) {
    if (!symbolChar(*c)) return false;
  }
  return true;
}

void lookupUrl(const char* query, char* out, const size_t cap) {
  int at = std::snprintf(out, cap, "https://symlookup.cnbc.com/symservice/symlookup.do?prefix=");
  for (const char* c = query; c != nullptr && *c != 0 && at > 0 && static_cast<size_t>(at) + 4 < cap; ++c) {
    const bool plain = (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '.' || *c == '-';
    at += std::snprintf(out + at, cap - static_cast<size_t>(at), plain ? "%c" : "%%%02X",
                        plain ? *c : static_cast<unsigned char>(*c));
  }
  if (at > 0 && static_cast<size_t>(at) < cap) {
    std::snprintf(out + at, cap - static_cast<size_t>(at), "&partnerid=20064&pgok=1&pgsize=12");
  }
}

namespace {

// The string value of `key` between `from` and `to`, or false.
bool valueIn(const char* from, const char* to, const char* key, char* out, const size_t cap) {
  const size_t keyLen = std::strlen(key);
  for (const char* p = from; p + keyLen + 2 < to; ++p) {
    if (*p != '"' || std::strncmp(p + 1, key, keyLen) != 0 || p[1 + keyLen] != '"') continue;
    const char* v = p + keyLen + 2;
    while (v < to && (*v == ' ' || *v == ':')) ++v;
    if (v >= to || *v != '"') return false;
    ++v;
    size_t n = 0;
    for (; v < to && *v != '"'; ++v) {
      if (*v == '\\' && v + 1 < to) ++v;
      if (n + 1 < cap) out[n++] = *v;
    }
    out[n] = 0;
    return true;
  }
  return false;
}

}  // namespace

int parseLookup(const char* json, const size_t len, Found* out, const int cap) {
  int count = 0;
  const char* end = json + len;
  const char* at = json;
  while (count < cap && at < end) {
    // One result is one object; none of them nests another.
    const char* open = static_cast<const char*>(std::memchr(at, '{', static_cast<size_t>(end - at)));
    if (open == nullptr) break;
    const char* next = static_cast<const char*>(std::memchr(open + 1, '{', static_cast<size_t>(end - open - 1)));
    const char* close = static_cast<const char*>(std::memchr(open + 1, '}', static_cast<size_t>(end - open - 1)));
    if (close == nullptr) break;
    if (next != nullptr && next < close) {
      at = next;
      continue;
    }
    at = close + 1;
    char symbol[24];
    char company[64];
    if (!valueIn(open, close, "symbolName", symbol, sizeof(symbol)) || !tickerLike(symbol)) continue;
    bool seen = false;
    for (int i = 0; i < count; ++i) seen = seen || std::strcmp(out[i].symbol, symbol) == 0;
    if (seen) continue;
    Found& f = out[count++];
    std::snprintf(f.symbol, sizeof(f.symbol), "%.*s", static_cast<int>(sizeof(f.symbol) - 1), symbol);
    company[0] = 0;
    valueIn(open, close, "companyName", company, sizeof(company));
    tidyInto(company, f.company, sizeof(f.company));
    char exchange[32] = "";
    valueIn(open, close, "exchangeName", exchange, sizeof(exchange));
    tidyInto(exchange, f.exchange, sizeof(f.exchange));
  }
  return count;
}

int euroInstrument() { return kEuro; }

Picks defaultPicks() { return Picks{{0, 1, 2}}; }

void setPicks(const Picks& picks) {
  const Picks fallback = defaultPicks();
  for (int i = 0; i < kAssets; ++i) current.slot[i] = present(picks.slot[i]) ? picks.slot[i] : fallback.slot[i];
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

void coinbaseStatsUrlFor(const char* product, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://api.exchange.coinbase.com/products/%s/stats", product);
}

void coinbaseUrl(char* out, const size_t cap) { coinbaseUrlFor(kCatalogue[0].symbol, out, cap); }

void cnbcBarsUrlSpan(const char* symbol, const int32_t today, const int daysBack, char* out, const size_t cap) {
  int y0 = 0, m0 = 0, d0 = 0, y1 = 0, m1 = 0, d1 = 0;
  civilOfDay(today - daysBack, y0, m0, d0);
  civilOfDay(today + 2, y1, m1, d1);
  char escaped[40];
  escape(symbol, escaped, sizeof(escaped));
  std::snprintf(
      out, cap,
      "https://ts-api.cnbc.com/harmony/app/bars/%s/1D/%04d%02d%02d000000/%04d%02d%02d000000/adjusted/EST5EDT.json",
      escaped, y0 % 10000, m0 % 100, d0 % 100, y1 % 10000, m1 % 100, d1 % 100);
}

void cnbcBarsUrlFor(const char* symbol, const int32_t today, char* out, const size_t cap) {
  cnbcBarsUrlSpan(symbol, today, 125, out, cap);
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
  // Static: two kilobytes are better off the stack, and one answer is read at a time.
  static float newest[kMaxCloses];
  static int32_t newestDay[kMaxCloses];
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

bool applyCoinbaseStats(const char* json, const size_t len, Quote& out) {
  if (!out.valid) return false;
  const Span stats = flatjson::root(json, len);
  const double open = loose(member(stats, "open"));
  const double last = loose(member(stats, "last"));
  if (open <= 0) return false;
  out.dayAgo = open;
  if (last > 0) {
    out.price = last;
    out.closes[out.count - 1] = static_cast<float>(last);
  }
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
    char currency[8] = "";
    flatjson::string(member(quote, "currencyCode"), currency, sizeof(currency));
    if (std::strlen(currency) == 3) std::snprintf(q.currency, sizeof(q.currency), "%.3s", currency);
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

double previousClose(const Quote& q) {
  if (q.dayAgo > 0) return q.dayAgo;
  return q.count >= 2 ? static_cast<double>(q.closes[q.count - 2]) : q.price;
}

double closeBefore(const Quote& q, const Period period) {
  if (q.count < 2) return q.price;
  if (period == Period::Day) return previousClose(q);
  const int32_t newest = q.days[q.count - 1];
  int32_t target = newest - 365;
  if (period == Period::Week) target = newest - 7;
  if (period == Period::Month) target = newest - 30;
  if (period == Period::Ytd) {
    int year = 0, month = 0, day = 0;
    civilOfDay(newest, year, month, day);
    target = daysFromCivil(year, 1, 1) - 1;
  }
  double close = q.closes[0];
  for (int i = 0; i < q.count - 1 && q.days[i] <= target; ++i) close = q.closes[i];
  return close;
}

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

bool historyFresh(const Quote& q, const int64_t now, const bool roundTheClock) {
  if (!q.valid || q.count < 2 || q.historyAt <= 0 || now < 1700000000) return false;
  if (now < q.historyAt || now - q.historyAt >= kHistoryFreshSeconds) return false;
  if (roundTheClock && q.days[q.count - 1] != static_cast<int32_t>(now / 86400)) return false;
  return true;
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
  if (!present(instrument) || !instrumentAt(instrument).ownable) return false;
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
  // A file from when a portfolio held fewer is shorter, and still good.
  if (size < 2 || in[0] != 1 || in[1] > kMaxPositions || size < 2u + static_cast<size_t>(in[1]) * 9u) return false;
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
