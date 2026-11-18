#include "NewsCore.h"

#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace news {

namespace {

constexpr Feed kWorld[] = {
    {"BBC", "https://feeds.bbci.co.uk/news/world/rss.xml"},
    {"GUARDIAN", "https://www.theguardian.com/world/rss"},
    {"EL PAIS", "https://feeds.elpais.com/mrss-s/pages/ep/site/elpais.com/section/internacional/portada"},
};
constexpr Feed kEconomy[] = {
    {"GUARDIAN", "https://www.theguardian.com/business/rss"},
    {"EL PAIS", "https://feeds.elpais.com/mrss-s/pages/ep/site/elpais.com/section/economia/portada"},
    {"PROJ. SYNDICATE", "https://www.project-syndicate.org/rss"},
    {"ECONOMIST", "https://www.economist.com/finance-and-economics/rss.xml"},
};
constexpr Feed kTech[] = {
    {"ARS TECHNICA", "https://feeds.arstechnica.com/arstechnica/index"},
    {"MIT TECH REV.", "https://www.technologyreview.com/feed/"},
    {"THE VERGE", "https://www.theverge.com/rss/index.xml"},
    {"REST OF WORLD", "https://restofworld.org/feed/latest/"},
};
constexpr Feed kAi[] = {
    {"S. WILLISON", "https://simonwillison.net/atom/everything/"},
    {"IMPORT AI", "https://importai.substack.com/feed"},
    {"MIT TECH REV.", "https://www.technologyreview.com/topic/artificial-intelligence/feed"},
};
constexpr Feed kLong[] = {
    {"GUARDIAN", "https://www.theguardian.com/news/series/the-long-read/rss"},
    {"AEON", "https://aeon.co/feed.rss"},
    {"NOEMA", "https://www.noemamag.com/feed/"},
    {"QUANTA", "https://www.quantamagazine.org/feed/"},
    {"CONVERSATION", "https://theconversation.com/global/articles.atom"},
};

template <size_t N>
constexpr int countOf(const Feed (&)[N]) {
  return static_cast<int>(N);
}

struct Named {
  const char* name;
  const char* utf8;
};
// The named entities feeds actually carry. Anything else is left as written.
constexpr Named kEntities[] = {
    {"amp", "&"},       {"lt", "<"},        {"gt", ">"},        {"quot", "\""},     {"apos", "'"},
    {"nbsp", " "},      {"mdash", "-"},     {"ndash", "-"},     {"hellip", "..."},  {"rsquo", "'"},
    {"lsquo", "'"},     {"rdquo", "\""},    {"ldquo", "\""},    {"eacute", "\xC3\xA9"}, {"aacute", "\xC3\xA1"},
    {"iacute", "\xC3\xAD"}, {"oacute", "\xC3\xB3"}, {"uacute", "\xC3\xBA"}, {"ntilde", "\xC3\xB1"},
    {"uuml", "\xC3\xBC"},   {"euro", "EUR"},    {"pound", "\xC2\xA3"},  {"copy", "(c)"},
};

void appendUtf8(std::string& out, const uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x110000) {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

// One pass of entity decoding. Feeds escape twice (HTML inside XML), so the
// callers that need both passes call this twice.
std::string decodeEntities(const std::string_view in) {
  std::string out;
  out.reserve(in.size());
  size_t i = 0;
  while (i < in.size()) {
    if (in[i] != '&') {
      out += in[i++];
      continue;
    }
    const size_t end = in.find(';', i);
    if (end == std::string_view::npos || end - i > 10) {
      out += in[i++];
      continue;
    }
    const std::string_view name = in.substr(i + 1, end - i - 1);
    bool done = false;
    if (name.size() > 1 && name[0] == '#') {
      const bool hex = name[1] == 'x' || name[1] == 'X';
      const std::string digits(name.substr(hex ? 2 : 1));
      char* stop = nullptr;
      const unsigned long cp = std::strtoul(digits.c_str(), &stop, hex ? 16 : 10);
      if (!digits.empty() && stop != nullptr && *stop == '\0' && cp > 0) {
        appendUtf8(out, static_cast<uint32_t>(cp));
        done = true;
      }
    } else {
      for (const Named& n : kEntities) {
        if (name == n.name) {
          out += n.utf8;
          done = true;
          break;
        }
      }
    }
    if (done) {
      i = end + 1;
    } else {
      out += in[i++];
    }
  }
  return out;
}

int monthFromName(const std::string_view s) {
  constexpr const char* kMonths[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                     "jul", "aug", "sep", "oct", "nov", "dec"};
  if (s.size() < 3) return 0;
  char low[4] = {0, 0, 0, 0};
  for (int i = 0; i < 3; ++i) low[i] = static_cast<char>(s[static_cast<size_t>(i)] | 0x20);
  for (int m = 0; m < 12; ++m) {
    if (std::strcmp(low, kMonths[m]) == 0) return m + 1;
  }
  return 0;
}

// Days from 1970-01-01 to a civil date (Howard Hinnant's algorithm).
long daysFromCivil(int y, const int m, const int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

uint32_t toEpoch(const int y, const int mo, const int d, const int h, const int mi, const int s, const int zoneMin) {
  if (y < 1990 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  const long long t = daysFromCivil(y, mo, d) * 86400LL + h * 3600LL + mi * 60LL + s - zoneMin * 60LL;
  return t > 0 && t < 0xFFFFFFFFLL ? static_cast<uint32_t>(t) : 0;
}

int number(const std::string_view s, size_t& at, const int maxDigits) {
  int value = 0;
  int digits = 0;
  while (at < s.size() && digits < maxDigits && s[at] >= '0' && s[at] <= '9') {
    value = value * 10 + (s[at] - '0');
    ++at;
    ++digits;
  }
  return digits > 0 ? value : -1;
}

// "+0100", "+01:00", "Z", "GMT" at `at`; minutes east of UTC.
int zoneAt(const std::string_view s, size_t at) {
  while (at < s.size() && s[at] == ' ') ++at;
  if (at >= s.size() || (s[at] != '+' && s[at] != '-')) return 0;
  const int sign = s[at] == '-' ? -1 : 1;
  ++at;
  const int hours = number(s, at, 2);
  if (at < s.size() && s[at] == ':') ++at;
  const int minutes = number(s, at, 2);
  if (hours < 0) return 0;
  return sign * (hours * 60 + (minutes > 0 ? minutes : 0));
}

std::string lowered(const std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
  }
  return out;
}

// The value of `name="..."` inside a tag's text, or empty.
std::string attribute(const std::string& tag, const char* name) {
  const std::string key = std::string(name) + "=";
  size_t at = 0;
  while ((at = tag.find(key, at)) != std::string::npos) {
    const bool boundary = at == 0 || tag[at - 1] == ' ' || tag[at - 1] == '\t' || tag[at - 1] == '\n';
    at += key.size();
    if (!boundary || at >= tag.size()) continue;
    const char quote = tag[at];
    if (quote != '"' && quote != '\'') continue;
    const size_t end = tag.find(quote, at + 1);
    if (end == std::string::npos) return "";
    return tag.substr(at + 1, end - at - 1);
  }
  return "";
}

void flat(std::string& s) {
  for (char& c : s) {
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  }
}

}  // namespace

const Section kSections[] = {
    {"WORLD", "WORLD", kWorld, countOf(kWorld)},     {"ECON", "ECONOMY", kEconomy, countOf(kEconomy)},
    {"TECH", "TECH", kTech, countOf(kTech)},         {"AI", "AI", kAi, countOf(kAi)},
    {"LONG", "LONG READS", kLong, countOf(kLong)},
};
const int kSectionCount = static_cast<int>(sizeof(kSections) / sizeof(kSections[0]));

uint32_t parseDate(const std::string_view text) {
  size_t at = 0;
  while (at < text.size() && (text[at] == ' ' || text[at] == '\n' || text[at] == '\t')) ++at;
  if (at >= text.size()) return 0;

  // ISO 8601: 2026-11-18T20:15:00Z
  if (text.size() - at >= 10 && text[at + 4] == '-' && text[at + 7] == '-') {
    const int y = number(text, at, 4);
    ++at;
    const int mo = number(text, at, 2);
    ++at;
    const int d = number(text, at, 2);
    int h = 0;
    int mi = 0;
    int s = 0;
    if (at < text.size() && (text[at] == 'T' || text[at] == 't' || text[at] == ' ')) {
      ++at;
      h = number(text, at, 2);
      if (at < text.size() && text[at] == ':') ++at;
      mi = number(text, at, 2);
      if (at < text.size() && text[at] == ':') {
        ++at;
        s = number(text, at, 2);
      }
      if (at < text.size() && text[at] == '.') {
        ++at;
        number(text, at, 9);
      }
    }
    if (y < 0 || mo < 0 || d < 0 || h < 0 || mi < 0 || s < 0) return 0;
    return toEpoch(y, mo, d, h, mi, s, zoneAt(text, at));
  }

  // RFC 822: [Wed, ]18 Nov 2026 20:15[:00] GMT
  const size_t comma = text.find(',', at);
  if (comma != std::string_view::npos && comma - at <= 10) at = comma + 1;
  while (at < text.size() && text[at] == ' ') ++at;
  const int d = number(text, at, 2);
  while (at < text.size() && text[at] == ' ') ++at;
  const int mo = at < text.size() ? monthFromName(text.substr(at)) : 0;
  while (at < text.size() && text[at] != ' ') ++at;
  while (at < text.size() && text[at] == ' ') ++at;
  int y = number(text, at, 4);
  if (y >= 0 && y < 100) y += y < 70 ? 2000 : 1900;
  while (at < text.size() && text[at] == ' ') ++at;
  const int h = number(text, at, 2);
  if (at < text.size() && text[at] == ':') ++at;
  const int mi = number(text, at, 2);
  int s = 0;
  if (at < text.size() && text[at] == ':') {
    ++at;
    s = number(text, at, 2);
  }
  if (d < 0 || mo == 0 || y < 0 || h < 0 || mi < 0 || s < 0) return 0;
  return toEpoch(y, mo, d, h, mi, s, zoneAt(text, at));
}

std::string plainText(const std::string_view html, const size_t cap) {
  std::string stripped;
  stripped.reserve(html.size());
  bool inTag = false;
  size_t tagStart = 0;
  for (size_t i = 0; i < html.size(); ++i) {
    const char c = html[i];
    if (c == '<') {
      inTag = true;
      tagStart = i + 1;
    } else if (c == '>' && inTag) {
      inTag = false;
      // A block tag separates words; an inline one (<b>, <a>, <em>) does not.
      size_t at = tagStart < html.size() && html[tagStart] == '/' ? tagStart + 1 : tagStart;
      const char first = at < i ? static_cast<char>(html[at] | 0x20) : '\0';
      const char second = at + 1 < i ? static_cast<char>(html[at + 1] | 0x20) : '\0';
      const bool inlineTag = (first == 'b' && second != 'r' && second != 'l') || first == 'a' || first == 'e' ||
                             first == 'i' || first == 's' || first == 'u' || first == 'c';
      if (!inlineTag) stripped += ' ';
    } else if (!inTag) {
      stripped += c;
    }
  }
  const std::string decoded = utf8FoldTypography(decodeEntities(stripped));
  std::string out;
  out.reserve(decoded.size());
  bool space = true;
  for (const char c : decoded) {
    const bool blank = c == ' ' || c == '\n' || c == '\t' || c == '\r';
    if (blank) {
      if (!space) out += ' ';
      space = true;
    } else {
      out += c;
      space = false;
    }
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  if (out.size() > cap) {
    size_t cut = cap;
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
    const size_t word = out.rfind(' ', cut);
    if (word != std::string::npos && word > cap / 2) cut = word;
    out.resize(cut);
    out += "...";
  }
  return out;
}

void ageLabel(const uint32_t when, const uint32_t now, char* out, const size_t cap) {
  if (cap == 0) return;
  out[0] = '\0';
  // A clock that was never set reads as the seventies; say nothing then.
  if (when == 0 || now < 1700000000u || when > now) return;
  const uint32_t age = now - when;
  if (age < 3600) {
    std::snprintf(out, cap, "%um", static_cast<unsigned>(age / 60 < 1 ? 1 : age / 60));
  } else if (age < 86400) {
    std::snprintf(out, cap, "%uh", static_cast<unsigned>(age / 3600));
  } else {
    std::snprintf(out, cap, "%ud", static_cast<unsigned>(age / 86400));
  }
}

// --- FeedScanner -----------------------------------------------------------

FeedScanner::FeedScanner(std::vector<Item>& out, const uint8_t feed, const int limit)
    : out_(out), feed_(feed), limit_(limit) {
  tag_.reserve(128);
  text_.reserve(512);
}

bool FeedScanner::feed(const char* data, const size_t length) {
  constexpr size_t kTagCap = 1024;
  constexpr size_t kTextCap = 4096;
  for (size_t i = 0; i < length; ++i) {
    if (full()) return false;
    const char c = data[i];
    if (inCdata_) {
      // "]]>" ends it; anything short of that was text.
      if (c == ']' && tail_ < 2) {
        ++tail_;
      } else if (c == '>' && tail_ == 2) {
        inCdata_ = false;
        tail_ = 0;
      } else {
        if (field_ != Field::None && text_.size() < kTextCap) {
          for (int k = 0; k < tail_; ++k) text_ += ']';
          text_ += c;
        }
        tail_ = c == ']' ? 2 : 0;
      }
      continue;
    }
    if (inComment_) {
      if (c == '-' && tail_ < 2) {
        ++tail_;
      } else if (c == '>' && tail_ == 2) {
        inComment_ = false;
        tail_ = 0;
      } else {
        tail_ = c == '-' ? 2 : 0;
      }
      continue;
    }
    if (inTag_) {
      if (c == '>') {
        inTag_ = false;
        onTag();
        continue;
      }
      if (tag_.size() < kTagCap) tag_ += c;
      if (tag_.size() == 8 && tag_ == "![CDATA[") {
        inTag_ = false;
        inCdata_ = true;
        fieldRaw_ = true;
        tail_ = 0;
      } else if (tag_.size() == 3 && tag_ == "!--") {
        inTag_ = false;
        inComment_ = true;
        tail_ = 0;
      }
      continue;
    }
    if (c == '<') {
      inTag_ = true;
      tag_.clear();
      continue;
    }
    if (field_ != Field::None && text_.size() < kTextCap) text_ += c;
  }
  return !full();
}

void FeedScanner::onTag() {
  if (tag_.empty() || tag_[0] == '?' || tag_[0] == '!') return;
  const bool closing = tag_[0] == '/';
  const bool selfClosing = !closing && tag_.back() == '/';
  size_t nameEnd = closing ? 1 : 0;
  while (nameEnd < tag_.size() && tag_[nameEnd] != ' ' && tag_[nameEnd] != '\t' && tag_[nameEnd] != '\n' &&
         tag_[nameEnd] != '\r' && tag_[nameEnd] != '/') {
    ++nameEnd;
  }
  const std::string name = lowered(std::string_view(tag_).substr(closing ? 1 : 0, nameEnd - (closing ? 1 : 0)));

  if (closing) {
    if (field_ != Field::None && depth_ == fieldDepth_) closeField();
    if (itemDepth_ >= 0 && depth_ == itemDepth_ && (name == "item" || name == "entry")) closeItem();
    if (depth_ > 0) --depth_;
    return;
  }

  ++depth_;
  if (itemDepth_ < 0) {
    if (name == "item" || name == "entry") {
      itemDepth_ = depth_;
      item_ = Item{};
      item_.feed = feed_;
      summaryFromContent_ = false;
    }
  } else if (field_ == Field::None && depth_ == itemDepth_ + 1) {
    Field wanted = Field::None;
    if (name == "title") {
      wanted = Field::Title;
    } else if (name == "link") {
      // Atom carries the address in an attribute; RSS carries it as text.
      const std::string href = attribute(tag_, "href");
      if (!href.empty()) {
        const std::string rel = attribute(tag_, "rel");
        if ((rel.empty() || rel == "alternate") && item_.url.empty()) item_.url = decodeEntities(href);
      } else {
        wanted = Field::Link;
      }
    } else if (name == "description" || name == "summary") {
      wanted = Field::Summary;
    } else if (name == "content" && item_.summary.empty()) {
      wanted = Field::Summary;
      summaryFromContent_ = true;
    } else if (name == "pubdate" || name == "dc:date" || name == "published" ||
               (name == "updated" && item_.when == 0)) {
      wanted = Field::Date;
    }
    if (wanted != Field::None && !selfClosing) {
      field_ = wanted;
      fieldDepth_ = depth_;
      fieldRaw_ = false;
      text_.clear();
    }
  }
  if (selfClosing && depth_ > 0) --depth_;
}

void FeedScanner::closeField() {
  // Text outside CDATA is XML-escaped once more than the HTML it carries.
  const std::string text = fieldRaw_ ? text_ : decodeEntities(text_);
  switch (field_) {
    case Field::Title:
      item_.title = plainText(text, kTitleCap);
      break;
    case Field::Link: {
      std::string url = text;
      while (!url.empty() && (url.back() == ' ' || url.back() == '\n' || url.back() == '\r' || url.back() == '\t')) {
        url.pop_back();
      }
      size_t start = 0;
      while (start < url.size() && (url[start] == ' ' || url[start] == '\n' || url[start] == '\r' || url[start] == '\t')) {
        ++start;
      }
      if (item_.url.empty()) item_.url = url.substr(start);
      break;
    }
    case Field::Summary:
      if (item_.summary.empty() || !summaryFromContent_) item_.summary = plainText(text, kSummaryCap);
      break;
    case Field::Date: {
      const uint32_t when = parseDate(text);
      if (when != 0) item_.when = when;
      break;
    }
    case Field::None:
      break;
  }
  field_ = Field::None;
  text_.clear();
}

void FeedScanner::closeItem() {
  itemDepth_ = -1;
  field_ = Field::None;
  const bool web = item_.url.compare(0, 8, "https://") == 0 || item_.url.compare(0, 7, "http://") == 0;
  if (item_.title.empty() || !web || item_.url.size() > kUrlCap) return;
  out_.push_back(std::move(item_));
  ++kept_;
}

// --- A section ---------------------------------------------------------------

void mergeSection(std::vector<Item>& items) {
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.when > b.when; });
  std::vector<Item> kept;
  kept.reserve(items.size() < static_cast<size_t>(kPerSection) ? items.size() : static_cast<size_t>(kPerSection));
  for (Item& item : items) {
    if (static_cast<int>(kept.size()) >= kPerSection) break;
    bool seen = false;
    for (const Item& k : kept) seen = seen || k.url == item.url;
    if (!seen) kept.push_back(std::move(item));
  }
  items = std::move(kept);
}

std::string packSection(const std::vector<Item>& items, const uint32_t fetched) {
  std::string out = "NEWS\t1\t" + std::to_string(fetched) + "\n";
  for (const Item& item : items) {
    std::string title = item.title;
    std::string url = item.url;
    std::string summary = item.summary;
    flat(title);
    flat(url);
    flat(summary);
    out += std::to_string(item.when) + "\t" + std::to_string(item.feed) + "\t" + title + "\t" + url + "\t" + summary +
           "\n";
  }
  return out;
}

bool unpackSection(const std::string_view text, std::vector<Item>& items, uint32_t& fetched) {
  items.clear();
  fetched = 0;
  size_t at = text.find('\n');
  if (at == std::string_view::npos || text.compare(0, 7, "NEWS\t1\t") != 0) return false;
  fetched = static_cast<uint32_t>(std::strtoul(std::string(text.substr(7, at - 7)).c_str(), nullptr, 10));
  ++at;
  items.reserve(static_cast<size_t>(kPerSection));
  while (at < text.size() && static_cast<int>(items.size()) < kPerSection) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view row = text.substr(at, end - at);
    at = end + 1;
    std::string_view part[5];
    size_t from = 0;
    int n = 0;
    while (n < 5) {
      const size_t tab = n < 4 ? row.find('\t', from) : std::string_view::npos;
      part[n++] = row.substr(from, tab == std::string_view::npos ? std::string_view::npos : tab - from);
      if (tab == std::string_view::npos) break;
      from = tab + 1;
    }
    if (n < 4 || part[2].empty() || part[3].empty()) continue;
    Item item;
    item.when = static_cast<uint32_t>(std::strtoul(std::string(part[0]).c_str(), nullptr, 10));
    item.feed = static_cast<uint8_t>(std::atoi(std::string(part[1]).c_str()));
    item.title = std::string(part[2]);
    item.url = std::string(part[3]);
    if (n == 5) item.summary = std::string(part[4]);
    items.push_back(std::move(item));
  }
  return true;
}

std::string articleFile(const std::string_view url) {
  uint32_t hash = 2166136261u;  // FNV-1a
  for (const char c : url) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  char name[16];
  std::snprintf(name, sizeof(name), "%08lx.txt", static_cast<unsigned long>(hash));
  return name;
}

}  // namespace news
