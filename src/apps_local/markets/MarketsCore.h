#pragma once

// MARKETS' model: one instrument's quote and daily closes, and the arithmetic
// and number formatting the screens show. Bitcoin is read out of Coinbase's
// daily candles; the two indexes out of CNBC's daily bars, with its quote
// answer laid over them for the live price and the year's range. Freestanding
// (no Arduino), so host-tests/markets runs it against fixtures in those shapes.

#include <cstddef>
#include <cstdint>

namespace markets {

constexpr int kMaxCloses = 270;  // a year of trading days, with room

// The dashboard's three cards, in its order. Which instrument each one shows
// is chosen from the catalogue below; the names are the ones it began with.
enum class Asset : uint8_t { Bitcoin = 0, Sp500 = 1, Nasdaq = 2 };
constexpr int kAssets = 3;

struct AssetInfo {
  const char* name;     // "BITCOIN"
  const char* caption;  // "BTC / USD"
  const char* symbol;   // the source's own: Coinbase's product or CNBC's symbol
  const char* file;     // cache file stem
  bool always;          // trades around the clock, and comes from Coinbase
  bool ownable;         // something a portfolio can hold, priced in dollars
};

// Everything that can be put on a card. Crypto is Coinbase's; the rest CNBC's.
// The catalogue comes first, and after it the slots for what the owner added
// by ticker or name, which may be empty.
constexpr int kBuiltIn = 21;
constexpr int kCustom = 24;
constexpr int kInstruments = kBuiltIn + kCustom;
const AssetInfo& instrumentAt(int instrument);
// False for an empty slot.
bool present(int instrument);

// --- Instruments added by ticker or name. All of them are CNBC's. ---

constexpr size_t kSymbolChars = 12;   // with the terminator: what a quote answer is matched in
constexpr size_t kCompanyChars = 24;  // the caption under the name

// The instrument with this source symbol, or -1.
int findSymbol(const char* symbol);
// Adds a symbol and returns its instrument, or the one that already has it.
// With no empty slot it takes over the first one `used` leaves free (`used`
// has kCustom entries; null means none are spared) and sets `recycled`, so the
// caller can drop that slot's cached quote. -1 when there is no room or the
// symbol cannot be one.
int addCustom(const char* symbol, const char* company, const bool* used, bool* recycled);
// What its prices turned out to be counted in. True when that is news.
bool setCustomCurrency(int instrument, const char* currency);
void clearCustoms();
// One line a slot: symbol, company and currency between tabs.
constexpr size_t kCustomsBytes = kCustom * (kSymbolChars + kCompanyChars + 8);
size_t packCustoms(char* out, size_t cap);
void unpackCustoms(const char* in, size_t len);

// What was typed, as a search goes out: capitals, single spaces, no ends.
void tidyQuery(const char* typed, char* out, size_t cap);
// True when it could itself be a symbol: short and with no space in it.
bool tickerLike(const char* query);
void lookupUrl(const char* query, char* out, size_t cap);
struct Found {
  char symbol[kSymbolChars];
  char company[kCompanyChars];
  char exchange[12];
};
// CNBC's symbol lookup answer. Returns how many it read, `cap` at most.
int parseLookup(const char* json, size_t len, Found* out, int cap);
// What its price is counted in: "EUR" for the three European indexes, "USD" otherwise.
const char* currencyOf(int instrument);
// The euro in dollars, for a portfolio's total; -1 if it left the catalogue.
int euroInstrument();

struct Picks {
  uint8_t slot[kAssets];
};
Picks defaultPicks();
// What the cards show from now on. Out-of-range picks fall back to the default.
void setPicks(const Picks& picks);
Picks picks();
int instrumentOf(Asset asset);
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
  // What it cost 24 hours before `price`, for what trades around the clock and
  // so has no close to measure a day from. Zero when not known.
  double dayAgo = 0;
  // Daily closes, oldest first. The last is the live price while a session is
  // open, so the one before it is always the previous close.
  float closes[kMaxCloses] = {};
  int32_t days[kMaxCloses] = {};  // each close's day, as days since 1970 in exchange time
  int count = 0;
};

// Coinbase's candles for a product: an array of [time, low, high, open, close,
// volume], newest first. False unless it holds at least two.
bool parseCoinbase(const char* json, size_t len, Quote& out);
// Coinbase's rolling 24-hour figures, laid over a quote parseCoinbase() made:
// the price a day ago, and the last trade as the price now.
bool applyCoinbaseStats(const char* json, size_t len, Quote& out);
void coinbaseStatsUrlFor(const char* product, char* out, size_t cap);
// CNBC's daily bars for one symbol, oldest first. False unless it holds two.
bool parseCnbcBars(const char* json, size_t len, Quote& out);
// Lays CNBC's quote for `asset` over bars already read: the live price as the
// last close, the day's and the year's range. False when the answer has no
// such symbol, which leaves `q` as it was.
bool applyCnbcQuote(const char* json, size_t len, Asset asset, Quote& q);

void coinbaseUrl(char* out, size_t cap);  // Bitcoin's
void coinbaseUrlFor(const char* product, char* out, size_t cap);
void cnbcBarsUrlFor(const char* symbol, int32_t today, char* out, size_t cap);
// The same over `daysBack` days: a year's worth for the long changes.
void cnbcBarsUrlSpan(const char* symbol, int32_t today, int daysBack, char* out, size_t cap);
constexpr int kYearOfDays = 380;
// One quote answer for several symbols at once.
void cnbcQuoteUrlFor(const char* const* symbols, int count, char* out, size_t cap);
bool applyCnbcQuoteFor(const char* json, size_t len, const char* symbol, Quote& q);
// Bars for the four months up to `today`, a day count since 1970.
void cnbcBarsUrl(Asset asset, int32_t today, char* out, size_t cap);
void cnbcQuoteUrl(char* out, size_t cap);
int32_t daysFromCivil(int year, int month, int day);

double previousClose(const Quote& q);
double dayChange(const Quote& q);
double dayChangePercent(const Quote& q);
// The spans a portfolio's change is counted over, in calendar time.
enum class Period : uint8_t { Day = 0, Week = 1, Month = 2, Ytd = 3, Year = 4 };
constexpr int kPeriods = 5;
// The close a period is measured from: yesterday's, the one a week, a month or
// a year before the newest, or the last of the year before. The oldest held
// when the history does not reach that far.
double closeBefore(const Quote& q, Period period);
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

// --- The portfolio: how much of each thing, and what that comes to. ---

constexpr int kMaxPositions = 24;

struct Position {
  uint8_t instrument;
  double amount;
};

struct Portfolio {
  Position positions[kMaxPositions];
  int count = 0;
};

// Adds a holding or changes the amount of one already held; an amount of zero
// or less removes it. False when it is full or the instrument cannot be held.
bool setPosition(Portfolio& portfolio, int instrument, double amount);
int positionOf(const Portfolio& portfolio, int instrument);  // -1 when not held

// One holding's price now and at the previous close; invalid when no quote
// for it has been fetched yet.
struct Priced {
  bool valid = false;
  double price = 0;
  double previous = 0;
};

struct Totals {
  double value = 0;     // now, in dollars
  double previous = 0;  // the same holdings at their previous closes
  int priced = 0;       // how many holdings had a quote
};
Totals totalsOf(const Portfolio& portfolio, const Priced* priced);
double percentOf(double now, double before);  // 0 when there is no before

constexpr size_t kPortfolioBytes = 2 + kMaxPositions * 9;
size_t packPortfolio(const Portfolio& portfolio, uint8_t* out, size_t cap);
bool unpackPortfolio(const uint8_t* in, size_t size, Portfolio& portfolio);

// An amount being typed on the keypad: digits and one point.
struct AmountEntry {
  char text[12] = "";
};
// '0'..'9', '.' or '<' to rub one out. False for a key that does nothing.
bool entryKey(AmountEntry& entry, char key);
double entryValue(const AmountEntry& entry);
void entryFrom(double amount, AmountEntry& entry);
// "0.25", "12", "1,250.5": as many decimals as the amount has, to six.
void formatAmount(double amount, char* out, size_t cap);

}  // namespace markets
