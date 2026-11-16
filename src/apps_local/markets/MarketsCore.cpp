#include "MarketsCore.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../json/FlatJson.h"

namespace markets {

namespace {

using flatjson::element;
using flatjson::forEach;
using flatjson::member;
using flatjson::numberD;
using flatjson::Span;

double numberOr(const Span& v, const double fallback) {
  double d = 0;
  return numberD(v, d) ? d : fallback;
}

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

const AssetInfo& infoOf(const Asset asset) {
  static const AssetInfo kInfo[kAssets] = {
      {"BITCOIN", "BTC / USD", "BTC-USD", "btc", true},
      {"S&P 500", "US LARGE CAPS", "%5EGSPC", "sp500", false},
      {"NASDAQ", "COMPOSITE", "%5EIXIC", "nasdaq", false},
  };
  const int i = static_cast<int>(asset);
  return kInfo[i >= 0 && i < kAssets ? i : 0];
}

void quoteUrl(const Asset asset, char* out, const size_t cap) {
  std::snprintf(out, cap,
                "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=3mo&interval=1d&includePrePost=false",
                infoOf(asset).symbol);
}

bool parseQuote(const char* json, const size_t len, Quote& out) {
  out = Quote{};
  const Span root = flatjson::root(json, len);
  const Span result = element(member(member(root, "chart"), "result"), 0);
  if (!result.ok()) return false;
  const Span meta = member(result, "meta");
  if (!meta.ok() || !numberD(member(meta, "regularMarketPrice"), out.price)) return false;
  flatjson::string(member(meta, "currency"), out.currency, sizeof(out.currency));
  flatjson::string(member(meta, "timezone"), out.zone, sizeof(out.zone));
  out.dayHigh = numberOr(member(meta, "regularMarketDayHigh"), 0);
  out.dayLow = numberOr(member(meta, "regularMarketDayLow"), 0);
  out.yearHigh = numberOr(member(meta, "fiftyTwoWeekHigh"), 0);
  out.yearLow = numberOr(member(meta, "fiftyTwoWeekLow"), 0);
  out.volume = numberOr(member(meta, "regularMarketVolume"), 0);
  out.time = static_cast<int64_t>(numberOr(member(meta, "regularMarketTime"), 0));
  out.gmtOffset = static_cast<int32_t>(numberOr(member(meta, "gmtoffset"), 0));
  const Span regular = member(member(meta, "currentTradingPeriod"), "regular");
  out.sessionStart = static_cast<int64_t>(numberOr(member(regular, "start"), 0));
  out.sessionEnd = static_cast<int64_t>(numberOr(member(regular, "end"), 0));

  // Closes and their stamps run in step, and either may hold a null (a
  // holiday the exchange still listed); only whole pairs are kept. When the
  // answer is longer than the buffer, it is the newest that stay.
  const Span stamps = member(result, "timestamp");
  const Span closes = member(element(member(member(result, "indicators"), "quote"), 0), "close");
  int total = 0;
  forEach(closes, 4096, [&](int, const Span&) { ++total; });
  const int skip = total > kMaxCloses ? total - kMaxCloses : 0;
  // The stamps wait in `days` (zero for a missing one) until their closes are
  // read, then each kept pair is packed down over them.
  forEach(stamps, 4096, [&](const int i, const Span& el) {
    double t = 0;
    if (i >= skip && i - skip < kMaxCloses && numberD(el, t)) out.days[i - skip] = static_cast<int32_t>(t);
  });
  const int32_t offset = out.gmtOffset;
  forEach(closes, 4096, [&](const int i, const Span& el) {
    double c = 0;
    if (i < skip || i - skip >= kMaxCloses) return;
    const int32_t stamp = out.days[i - skip];
    if (stamp == 0 || !numberD(el, c) || c <= 0) return;
    out.closes[out.count] = static_cast<float>(c);
    out.days[out.count] = static_cast<int32_t>(floorDiv(static_cast<int64_t>(stamp) + offset, 86400));
    ++out.count;
  });
  if (out.count < 2) return false;
  out.valid = true;
  return true;
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

}  // namespace markets
