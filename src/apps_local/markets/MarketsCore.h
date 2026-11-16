#pragma once

// MARKETS' model: one instrument's quote and daily closes read out of a Yahoo
// Finance chart answer, and the arithmetic and number formatting the screens
// show. Freestanding (no Arduino), so host-tests/markets runs it against a
// fixture in the answer's shape.

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
  const char* symbol;   // Yahoo's, URL-encoded
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

// False unless the answer holds a price and at least two closes.
bool parseQuote(const char* json, size_t len, Quote& out);
void quoteUrl(Asset asset, char* out, size_t cap);

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
