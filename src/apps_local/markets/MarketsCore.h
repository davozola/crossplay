#pragma once

// MARKETS' model: one instrument's quote and daily closes, and the arithmetic
// and number formatting the screens show. Bitcoin is read out of Coinbase's
// daily candles; the two indexes out of CNBC's daily bars, with its quote
// answer laid over them for the live price and the year's range. Freestanding
// (no Arduino), so host-tests/markets runs it against fixtures in those shapes.

#include <cstddef>
#include <cstdint>

namespace markets {

constexpr int kMaxCloses = 96;  // three months of trading days, with room

// The three the dashboard shows, in its order.
enum class Asset : uint8_t { Bitcoin = 0, Sp500 = 1, Nasdaq = 2 };
constexpr int kAssets = 3;

struct AssetInfo {
  const char* name;     // "BITCOIN"
  const char* caption;  // "BTC / USD"
  const char* symbol;   // the source's own: Coinbase's product or CNBC's symbol
  const char* file;     // cache file stem
  bool always;          // trades around the clock
};
const AssetInfo& infoOf(Asset asset);

struct Quote {
  bool valid = false;
  char currency[4] = "";
  char zone[6] = "";  // the exchange's zone abbreviation, "EST"
  double price = 0;
  double dayHigh = 0;
  double dayLow = 0;
  double yearHigh = 0;
  double yearLow = 0;
  double volume = 0;
  int64_t time = 0;       // of `price`, Unix seconds
  int32_t gmtOffset = 0;  // the exchange's, seconds
  int64_t sessionStart = 0;
  int64_t sessionEnd = 0;
  // Daily closes, oldest first. The last is the live price while a session is
  // open, so the one before it is always the previous close.
  float closes[kMaxCloses] = {};
  int32_t days[kMaxCloses] = {};  // each close's day, as days since 1970 in exchange time
  int count = 0;
};

// Coinbase's candles for a product: an array of [time, low, high, open, close,
// volume], newest first. False unless it holds at least two.
bool parseCoinbase(const char* json, size_t len, Quote& out);
// CNBC's daily bars for one symbol, oldest first. False unless it holds two.
bool parseCnbcBars(const char* json, size_t len, Quote& out);
// Lays CNBC's quote for `asset` over bars already read: the live price as the
// last close, the day's and the year's range. False when the answer has no
// such symbol, which leaves `q` as it was.
bool applyCnbcQuote(const char* json, size_t len, Asset asset, Quote& q);

void coinbaseUrl(char* out, size_t cap);
// Bars for the four months up to `today`, a day count since 1970.
void cnbcBarsUrl(Asset asset, int32_t today, char* out, size_t cap);
void cnbcQuoteUrl(char* out, size_t cap);
int32_t daysFromCivil(int year, int month, int day);

double previousClose(const Quote& q);
double dayChange(const Quote& q);
double dayChangePercent(const Quote& q);
// Percent change from the close `sessions` trading days back to the price;
// clamped to the history held. False when there is none to compare with.
bool changeOver(const Quote& q, int sessions, double& percent);
// The lowest and highest close among the last `sessions` (all of them for 0).
void closeRange(const Quote& q, int sessions, float& lo, float& hi);
// True while the price is from inside the exchange's regular session.
bool sessionOpen(const Quote& q);

// "104,250" above ten thousand, "6,734.11" below: the digits worth showing.
void formatPrice(double value, char* out, size_t cap);
// The dashboard's shorter form: whole units from a thousand up ("6,460"), so
// three cards of very different prices set in one size.
void formatPriceShort(double value, char* out, size_t cap);
// "+1,190" / "-12.40", same rule, always signed.
void formatChange(double value, char* out, size_t cap);
// "+1.24%"
void formatPercent(double percent, char* out, size_t cap);
// "3.31B", "842M", "12.4K"
void formatVolume(double value, char* out, size_t cap);

struct Civil {
  int year, month, day, hour, minute;
};
Civil civilOf(int64_t unixSeconds, int32_t gmtOffset);
void civilOfDay(int32_t days, int& year, int& month, int& day);
const char* monthName(int month);  // "JAN", 1-based

}  // namespace markets
