#include "DiscoverCore.h"

#include <cstdio>
#include <cstring>

#include "../json/FlatJson.h"

namespace discover {

namespace {

using flatjson::forEach;
using flatjson::member;
using flatjson::Span;

constexpr const char* kTopicNames[kTopics] = {"EVERYTHING", "AI", "FIRMWARE", "SYNTHS", "EMULATORS", "GADGETS"};
// What each topic searches names, descriptions and topics for. GitHub takes
// five ORs in a query, so five words at most.
constexpr const char* kTopicWords[kTopics] = {
    "",
    "llm+OR+agent+OR+diffusion+OR+rag+OR+transformer",
    "firmware+OR+esp32+OR+stm32+OR+rp2040+OR+zephyr",
    "synthesizer+OR+eurorack+OR+midi+OR+synth+OR+dsp",
    "emulator+OR+emulation+OR+gameboy+OR+retro+OR+arcade",
    "eink+OR+e-ink+OR+keyboard+OR+raspberry-pi+OR+hardware",
};
constexpr const char* kLanguageNames[kLanguages] = {"ANY LANGUAGE", "C",  "C++",        "RUST",
                                                    "PYTHON",       "GO", "TYPESCRIPT", "ZIG"};
constexpr const char* kLanguageQuery[kLanguages] = {"", "c", "cpp", "rust", "python", "go", "typescript", "zig"};
constexpr const char* kStarNames[kStarChoices] = {"10+ STARS", "50+ STARS", "200+ STARS", "1000+ STARS", "ANY STARS"};
constexpr int kStarFloors[kStarChoices] = {10, 50, 200, 1000, 0};
constexpr Stars kStarOrder[kStarChoices] = {Stars::Any, Stars::Ten, Stars::Fifty, Stars::TwoHundred, Stars::Thousand};
constexpr const char* kAgeNames[kAges] = {"NEW THIS WEEK",   "NEW THIS MONTH", "NEW THIS YEAR",
                                          "ANY AGE, ACTIVE", "RISING NOW",     "ANY AGE"};
constexpr Age kAgeOrder[kAges] = {Age::Rising, Age::Week, Age::Month, Age::Year, Age::AnyActive, Age::Ever};

template <typename E>
int clampIndex(const E value, const int count) {
  const int i = static_cast<int>(value);
  return i >= 0 && i < count ? i : 0;
}

void isoDate(const int32_t day, char* out, const size_t cap) {
  int y = 0, m = 0, d = 0;
  civilOfDay(day, y, m, d);
  std::snprintf(out, cap, "%04d-%02d-%02d", y % 10000, m % 100, d % 100);
}

int hexOf(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Four hex digits at `p`, or -1.
int32_t unit(const char* p, const char* end) {
  if (end - p < 4) return -1;
  int32_t v = 0;
  for (int i = 0; i < 4; ++i) {
    const int h = hexOf(p[i]);
    if (h < 0) return -1;
    v = v * 16 + h;
  }
  return v;
}

// A JSON string with its escapes undone, \u pairs included, as UTF-8, up to
// `cap` bytes. Line ends stay when `lines` is set and are spaces otherwise.
void decode(const Span& v, std::string& out, const size_t cap, const bool lines) {
  out.clear();
  if (!v.ok() || *v.p != '"') return;
  for (const char* p = v.p + 1; p < v.end && *p != '"' && out.size() + 5 < cap; ++p) {
    if (*p != '\\' || p + 1 >= v.end) {
      out.push_back(*p);
      continue;
    }
    ++p;
    if (*p == 'n') {
      out.push_back(lines ? '\n' : ' ');
    } else if (*p == 'r' || *p == 't') {
      if (!lines || *p == 't') out.push_back(' ');
    } else if (*p == 'u') {
      int32_t cp = unit(p + 1, v.end);
      if (cp < 0) continue;
      p += 4;
      if (cp >= 0xD800 && cp < 0xDC00 && v.end - p > 6 && p[1] == '\\' && p[2] == 'u') {
        const int32_t low = unit(p + 3, v.end);
        if (low >= 0xDC00 && low < 0xE000) {
          cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          p += 6;
        }
      }
      if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
      } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
      }
    } else {
      out.push_back(*p);
    }
  }
}

// A short JSON string as what the reading face can set, cut to fit `dest`.
void text(const Span& v, char* dest, const size_t destCap) {
  dest[0] = 0;
  std::string out;
  out.reserve(512);
  decode(v, out, 512, false);
  latinOnly(out.data());
  // What fits, never ending inside a two-byte letter.
  size_t n = std::strlen(out.c_str());
  if (n > destCap - 1) n = destCap - 1;
  if (n > 0 && (static_cast<unsigned char>(out[n - 1]) & 0xC0) == 0xC0) --n;
  std::memcpy(dest, out.c_str(), n);
  dest[n] = 0;
}

int32_t integer(const Span& v) {
  double d = 0;
  return flatjson::numberD(v, d) && d > 0 && d < 2e9 ? static_cast<int32_t>(d) : 0;
}

// One field of a saved line: tabs and line ends would break it.
void field(std::string& out, const char* value) {
  for (const char* c = value; *c != 0; ++c) out.push_back(*c == '\t' || *c == '\n' || *c == '\r' ? ' ' : *c);
  out.push_back('\t');
}

}  // namespace

const char* topicName(const Topic topic) {
  return topic == Topic::Own ? "YOUR WORDS" : kTopicNames[clampIndex(topic, kTopics)];
}
const char* languageName(const Language language) { return kLanguageNames[clampIndex(language, kLanguages)]; }
const char* starsName(const Stars stars) { return kStarNames[clampIndex(stars, kStarChoices)]; }
const char* ageName(const Age age) { return kAgeNames[clampIndex(age, kAges)]; }
int starsFloor(const Stars stars) { return kStarFloors[clampIndex(stars, kStarChoices)]; }

Stars starsAt(const int row) { return kStarOrder[row >= 0 && row < kStarChoices ? row : 0]; }
Age ageAt(const int row) { return kAgeOrder[row >= 0 && row < kAges ? row : 0]; }

bool skipsSeen(const Filter& filter) {
  return filter.skipSeen && filter.age != Age::AnyActive && filter.age != Age::Ever;
}

void tidyQuery(const char* typed, char* out, const size_t cap) {
  size_t at = 0;
  bool space = true;
  for (const char* c = typed; *c != 0 && at + 1 < cap; ++c) {
    const unsigned char u = static_cast<unsigned char>(*c);
    if (u <= ' ' || u >= 0x7F) {
      if (!space) out[at++] = ' ';
      space = true;
      continue;
    }
    out[at++] = *c;
    space = false;
  }
  while (at > 0 && out[at - 1] == ' ') --at;
  out[at] = 0;
}

std::string packFilter(const Filter& filter) {
  std::string out;
  out.push_back(static_cast<char>(filter.topic));
  out.push_back(static_cast<char>(filter.language));
  out.push_back(static_cast<char>(filter.stars));
  out.push_back(static_cast<char>(filter.age));
  out.push_back(filter.skipSeen ? 1 : 0);
  out += filter.query;
  return out;
}

bool unpackFilter(const std::string_view in, Filter& out) {
  if (in.size() < 4) return false;
  const uint8_t topic = static_cast<uint8_t>(in[0]);
  const uint8_t language = static_cast<uint8_t>(in[1]);
  const uint8_t stars = static_cast<uint8_t>(in[2]);
  const uint8_t age = static_cast<uint8_t>(in[3]);
  if (topic > static_cast<uint8_t>(Topic::Own) || language >= kLanguages || stars >= kStarChoices || age >= kAges) {
    return false;
  }
  Filter read;
  read.topic = static_cast<Topic>(topic);
  read.language = static_cast<Language>(language);
  read.stars = static_cast<Stars>(stars);
  read.age = static_cast<Age>(age);
  if (in.size() >= 5) {
    read.skipSeen = in[4] != 0;
    tidyQuery(std::string(in.substr(5)).c_str(), read.query, sizeof(read.query));
  }
  // Words of the reader's own with no words is a search for everything.
  if (read.topic == Topic::Own && read.query[0] == 0) read.topic = Topic::Everything;
  out = read;
  return true;
}

// Howard Hinnant's civil_from_days.
void civilOfDay(const int32_t days, int& year, int& month, int& day) {
  const int32_t z = days + 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t y = static_cast<int32_t>(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  year = y + (month <= 2 ? 1 : 0);
}

int32_t dayOfDate(const char* iso) {
  int y = 0, m = 0, d = 0;
  if (std::sscanf(iso, "%4d-%2d-%2d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 || d > 31) return -1;
  // Howard Hinnant's days_from_civil.
  y -= m <= 2 ? 1 : 0;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
  const uint32_t doy = (153u * static_cast<uint32_t>(m > 2 ? m - 3 : m + 9) + 2) / 5 + static_cast<uint32_t>(d) - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

void searchUrl(const Filter& filter, const int32_t today, const int page, char* out, const size_t cap) {
  std::string query;
  auto add = [&](const std::string& term) {
    if (!query.empty()) query.push_back('+');
    query += term;
  };
  if (filter.topic == Topic::Own) {
    // The reader's words, as an address can carry them.
    std::string typed;
    for (const char* c = filter.query; *c != 0; ++c) {
      const bool plain = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
                         *c == '-' || *c == '_' || *c == '.';
      if (plain) {
        typed.push_back(*c);
      } else if (*c == ' ') {
        typed.push_back('+');
      } else {
        char hex[4];
        std::snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned>(static_cast<unsigned char>(*c)));
        typed += hex;
      }
    }
    if (!typed.empty()) add(typed);
  }
  const char* words = filter.topic == Topic::Own ? "" : kTopicWords[clampIndex(filter.topic, kTopics)];
  if (words[0] != 0) add(std::string(words) + "+in:name,description,topics");
  const char* language = kLanguageQuery[clampIndex(filter.language, kLanguages)];
  if (language[0] != 0) add(std::string("language:") + language);
  add("stars:%3E=" + std::to_string(starsFloor(filter.stars)));
  static constexpr int kDaysBack[kAges] = {7, 30, 365, 30, 60, 0};
  char since[16];
  if (filter.age != Age::Ever) {
    isoDate(today - kDaysBack[clampIndex(filter.age, kAges)], since, sizeof(since));
    add(std::string(filter.age == Age::AnyActive ? "pushed:%3E=" : "created:%3E=") + since);
  }
  if (filter.age == Age::Rising) {
    isoDate(today - 7, since, sizeof(since));
    add(std::string("pushed:%3E=") + since);
  }
  char pageTerm[16] = "";
  if (page > 1) std::snprintf(pageTerm, sizeof(pageTerm), "&page=%d", page % 100);
  std::snprintf(out, cap, "https://api.github.com/search/repositories?q=%s&sort=stars&order=desc&per_page=%d%s",
                query.c_str(), kPerPage, pageTerm);
}

void pageUrl(const char* fullName, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://github.com/%s", fullName);
}

void readmeUrl(const char* fullName, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://raw.githubusercontent.com/%s/HEAD/README.md", fullName);
}

const char* nameOf(const Repo& repo) {
  const char* slash = std::strchr(repo.fullName, '/');
  return slash != nullptr ? slash + 1 : repo.fullName;
}

int parseSearch(const char* json, const size_t len, Repo* out, const int cap, int32_t& total) {
  const Span root = flatjson::root(json, len);
  const Span items = member(root, "items");
  if (!items.ok()) return -1;
  total = integer(member(root, "total_count"));
  int count = 0;
  forEach(items, cap, [&](int, const Span& item) {
    if (count >= cap) return;
    Repo repo;
    text(member(item, "full_name"), repo.fullName, sizeof(repo.fullName));
    if (repo.fullName[0] == 0 || std::strchr(repo.fullName, '/') == nullptr) return;
    text(member(item, "description"), repo.description, sizeof(repo.description));
    text(member(item, "language"), repo.language, sizeof(repo.language));
    text(member(item, "created_at"), repo.created, sizeof(repo.created));
    text(member(item, "pushed_at"), repo.pushed, sizeof(repo.pushed));
    text(member(member(item, "license"), "spdx_id"), repo.license, sizeof(repo.license));
    if (std::strcmp(repo.license, "NOASSERTION") == 0) repo.license[0] = 0;
    text(member(item, "homepage"), repo.homepage, sizeof(repo.homepage));
    repo.stars = integer(member(item, "stargazers_count"));
    repo.forks = integer(member(item, "forks_count"));
    repo.issues = integer(member(item, "open_issues_count"));
    size_t used = 0;
    forEach(member(item, "topics"), 6, [&](int, const Span& topic) {
      char one[32];
      text(topic, one, sizeof(one));
      const size_t n = std::strlen(one);
      if (n == 0 || used + n + 3 >= sizeof(repo.topics)) return;
      if (used > 0) {
        repo.topics[used++] = ',';
        repo.topics[used++] = ' ';
      }
      std::memcpy(repo.topics + used, one, n + 1);
      used += n;
    });
    out[count++] = repo;
  });
  return count;
}

void formatCount(const int32_t n, char* out, const size_t cap) {
  if (n >= 100000) {
    std::snprintf(out, cap, "%dK", static_cast<int>(n / 1000) % 100000);
  } else if (n >= 1000) {
    std::snprintf(out, cap, "%d.%dK", static_cast<int>(n / 1000), static_cast<int>((n % 1000) / 100));
  } else {
    std::snprintf(out, cap, "%d", static_cast<int>(n < 0 ? 0 : n));
  }
}

void latinOnly(char* text) {
  const unsigned char* in = reinterpret_cast<const unsigned char*>(text);
  char* out = text;
  while (*in != 0) {
    const unsigned char c = *in;
    if (c < 0x80) {
      *out++ = c == '\n' || c == '\r' || c == '\t' ? ' ' : static_cast<char>(c);
      ++in;
      continue;
    }
    int length = c >= 0xF0 ? 4 : (c >= 0xE0 ? 3 : (c >= 0xC0 ? 2 : 1));
    uint32_t cp = length == 4 ? c & 0x07 : (length == 3 ? c & 0x0F : (length == 2 ? c & 0x1F : 0xFFFD));
    int have = 1;
    while (have < length && (in[have] & 0xC0) == 0x80) cp = (cp << 6) | (in[have++] & 0x3F);
    if (have < length) cp = 0xFFFD;
    in += have;
    // The quotes and dashes people type without meaning to.
    const char* plain = nullptr;
    if (cp == 0x2018 || cp == 0x2019) plain = "'";
    if (cp == 0x201C || cp == 0x201D) plain = "\"";
    if (cp == 0x2013 || cp == 0x2014) plain = "-";
    if (cp == 0x2026) plain = "...";
    if (cp == 0x00A0) plain = " ";
    if (plain != nullptr) {
      // Never longer than what it replaces, so this stays in place.
      for (; *plain != 0; ++plain) *out++ = *plain;
    } else if (cp >= 0xA1 && cp <= 0xFF) {
      *out++ = static_cast<char>(0xC0 | (cp >> 6));
      *out++ = static_cast<char>(0x80 | (cp & 0x3F));
    }
  }
  *out = 0;
  // What dropping a symbol leaves: doubled and leading spaces.
  char* w = text;
  bool space = true;
  for (const char* r = text; *r != 0; ++r) {
    if (*r == ' ' && space) continue;
    space = *r == ' ';
    *w++ = *r;
  }
  while (w > text && w[-1] == ' ') --w;
  *w = 0;
}

std::string packSaved(const Repo* repos, const int count) {
  std::string out;
  char numbers[48];
  for (int i = 0; i < count; ++i) {
    const Repo& r = repos[i];
    field(out, r.fullName);
    field(out, r.description);
    field(out, r.language);
    field(out, r.created);
    field(out, r.pushed);
    field(out, r.license);
    field(out, r.homepage);
    field(out, r.topics);
    std::snprintf(numbers, sizeof(numbers), "%d\t%d\t%d\n", static_cast<int>(r.stars), static_cast<int>(r.forks),
                  static_cast<int>(r.issues));
    out += numbers;
  }
  return out;
}

int unpackSaved(const std::string_view all, Repo* out, const int cap) {
  int count = 0;
  size_t at = 0;
  while (at < all.size() && count < cap) {
    size_t end = all.find('\n', at);
    if (end == std::string_view::npos) end = all.size();
    std::string_view line = all.substr(at, end - at);
    at = end + 1;
    Repo repo;
    char* const texts[8] = {repo.fullName, repo.description, repo.language, repo.created,
                            repo.pushed,   repo.license,     repo.homepage, repo.topics};
    const size_t caps[8] = {sizeof(repo.fullName), sizeof(repo.description), sizeof(repo.language),
                            sizeof(repo.created),  sizeof(repo.pushed),      sizeof(repo.license),
                            sizeof(repo.homepage), sizeof(repo.topics)};
    int32_t* const numbers[3] = {&repo.stars, &repo.forks, &repo.issues};
    bool whole = true;
    for (int f = 0; f < 11; ++f) {
      size_t tab = line.find('\t');
      if (tab == std::string_view::npos) {
        if (f < 10) whole = false;
        tab = line.size();
      }
      const std::string_view value = line.substr(0, tab);
      if (f < 8) {
        const size_t n = value.size() < caps[f] - 1 ? value.size() : caps[f] - 1;
        std::memcpy(texts[f], value.data(), n);
        texts[f][n] = 0;
      } else {
        int32_t v = 0;
        for (const char c : value) {
          if (c < '0' || c > '9' || v > 100000000) break;
          v = v * 10 + (c - '0');
        }
        *numbers[f - 8] = v;
      }
      line.remove_prefix(tab < line.size() ? tab + 1 : line.size());
    }
    if (whole && std::strchr(repo.fullName, '/') != nullptr) out[count++] = repo;
  }
  return count;
}

int findSaved(const Repo* repos, const int count, const char* fullName) {
  for (int i = 0; i < count; ++i) {
    if (std::strcmp(repos[i].fullName, fullName) == 0) return i;
  }
  return -1;
}

int toggleSaved(Repo* repos, int count, const Repo& repo) {
  const int at = findSaved(repos, count, repo.fullName);
  if (at >= 0) {
    for (int i = at + 1; i < count; ++i) repos[i - 1] = repos[i];
    return count - 1;
  }
  if (count == kMaxSaved) {
    for (int i = 1; i < count; ++i) repos[i - 1] = repos[i];
    --count;
  }
  repos[count] = repo;
  return count + 1;
}

int32_t starsPerDay(const Repo& repo, const int32_t today) {
  const int32_t made = dayOfDate(repo.created);
  const int32_t days = made < 0 || today <= made ? 1 : today - made;
  return repo.stars / days;
}

void rankByPace(Repo* repos, const int count, const int32_t today) {
  // An insertion sort: thirty at most, and equal paces keep GitHub's order.
  for (int i = 1; i < count; ++i) {
    const Repo moving = repos[i];
    const int32_t pace = starsPerDay(moving, today);
    int at = i;
    while (at > 0 && starsPerDay(repos[at - 1], today) < pace) {
      repos[at] = repos[at - 1];
      --at;
    }
    repos[at] = moving;
  }
}

namespace {

bool sameWords(const char* a, const char* b) {
  for (;; ++a, ++b) {
    const char x = *a >= 'A' && *a <= 'Z' ? static_cast<char>(*a + 32) : *a;
    const char y = *b >= 'A' && *b <= 'Z' ? static_cast<char>(*b + 32) : *b;
    if (x != y) return false;
    if (x == 0) return true;
  }
}

void copyText(char* dest, const size_t cap, const std::string_view value) {
  const size_t n = value.size() < cap - 1 ? value.size() : cap - 1;
  std::memcpy(dest, value.data(), n);
  dest[n] = 0;
}

}  // namespace

void rememberTerm(Terms& terms, const char* term) {
  char tidy[kQueryCap];
  tidyQuery(term, tidy, sizeof(tidy));
  if (tidy[0] == 0) return;
  int at = terms.count;
  for (int i = 0; i < terms.count; ++i) {
    if (sameWords(terms.items[i], tidy)) at = i;
  }
  if (at == terms.count && terms.count == kMaxTerms) at = kMaxTerms - 1;
  if (at == terms.count) ++terms.count;
  for (int i = at; i > 0; --i) std::memcpy(terms.items[i], terms.items[i - 1], kQueryCap);
  std::memcpy(terms.items[0], tidy, kQueryCap);
}

void forgetTerm(Terms& terms, const int index) {
  if (index < 0 || index >= terms.count) return;
  for (int i = index + 1; i < terms.count; ++i) std::memcpy(terms.items[i - 1], terms.items[i], kQueryCap);
  --terms.count;
  terms.items[terms.count][0] = 0;
}

std::string packTerms(const Terms& terms) {
  std::string out;
  for (int i = 0; i < terms.count; ++i) {
    out += terms.items[i];
    out.push_back('\n');
  }
  return out;
}

void unpackTerms(const std::string_view text, Terms& out) {
  out = Terms{};
  size_t at = 0;
  while (at < text.size() && out.count < kMaxTerms) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    tidyQuery(std::string(text.substr(at, end - at)).c_str(), out.items[out.count], kQueryCap);
    if (out.items[out.count][0] != 0) ++out.count;
    at = end + 1;
  }
}

uint32_t nameHash(const char* fullName) {
  // FNV-1a over the name in lower case: GitHub's names do not tell case apart.
  uint32_t h = 2166136261u;
  for (const char* c = fullName; *c != 0; ++c) {
    const char lower = *c >= 'A' && *c <= 'Z' ? static_cast<char>(*c + 32) : *c;
    h = (h ^ static_cast<unsigned char>(lower)) * 16777619u;
  }
  return h;
}

bool hasSeen(const Seen& seen, const char* fullName) {
  const uint32_t h = nameHash(fullName);
  for (int i = 0; i < seen.count; ++i) {
    if (seen.names[i] == h) return true;
  }
  return false;
}

bool markSeen(Seen& seen, const char* fullName) {
  if (hasSeen(seen, fullName)) return false;
  if (seen.count == kMaxSeen) {
    std::memmove(seen.names, seen.names + 1, sizeof(uint32_t) * (kMaxSeen - 1));
    --seen.count;
  }
  seen.names[seen.count++] = nameHash(fullName);
  return true;
}

int dropSeen(Repo* repos, const int count, const Seen& seen) {
  int kept = 0;
  for (int i = 0; i < count; ++i) {
    if (hasSeen(seen, repos[i].fullName)) continue;
    if (kept != i) repos[kept] = repos[i];
    ++kept;
  }
  return kept;
}

bool isNew(const Watched& watched) {
  return watched.tag[0] != 0 && watched.seenTag[0] != 0 && std::strcmp(watched.tag, watched.seenTag) != 0;
}

bool tidyFullName(const char* typed, char* out, const size_t cap) {
  out[0] = 0;
  std::string text;
  for (const char* c = typed; *c != 0; ++c) {
    if (static_cast<unsigned char>(*c) > ' ') text.push_back(*c);
  }
  const size_t host = text.find("github.com/");
  if (host != std::string::npos) text.erase(0, host + 11);
  while (!text.empty() && text.back() == '/') text.pop_back();
  if (text.size() > 4 && text.compare(text.size() - 4, 4, ".git") == 0) text.erase(text.size() - 4);
  const size_t slash = text.find('/');
  if (slash == std::string::npos || slash == 0 || slash + 1 >= text.size()) return false;
  // A longer address names something inside the repository; the first two parts are it.
  const size_t second = text.find('/', slash + 1);
  if (second != std::string::npos) text.erase(second);
  for (const char c : text) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
                    c == '_' || c == '.' || c == '/';
    if (!ok) return false;
  }
  if (text.size() + 1 > cap) return false;
  std::memcpy(out, text.c_str(), text.size() + 1);
  return true;
}

void releaseUrl(const char* fullName, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://api.github.com/repos/%s/releases/latest", fullName);
}

void tagsUrl(const char* fullName, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://api.github.com/repos/%s/tags?per_page=1", fullName);
}

void releasePageUrl(const Watched& watched, char* out, const size_t cap) {
  // A version with no day came from the tags, and has no release page.
  if (watched.tag[0] != 0 && watched.published[0] != 0) {
    std::snprintf(out, cap, "https://github.com/%s/releases/tag/%s", watched.fullName, watched.tag);
  } else {
    std::snprintf(out, cap, "https://github.com/%s/%s", watched.fullName, watched.tag[0] != 0 ? "tags" : "releases");
  }
}

bool parseRelease(const char* json, const size_t len, Watched& out, std::string& notes) {
  notes.clear();
  const Span root = flatjson::root(json, len);
  if (!root.ok()) return false;
  Watched read;
  text(member(root, "tag_name"), read.tag, sizeof(read.tag));
  if (read.tag[0] == 0) return false;
  text(member(root, "name"), read.title, sizeof(read.title));
  if (std::strcmp(read.title, read.tag) == 0) read.title[0] = 0;
  text(member(root, "published_at"), read.published, sizeof(read.published));
  decode(member(root, "body"), notes, 24u * 1024u, true);
  std::memcpy(out.tag, read.tag, sizeof(out.tag));
  std::memcpy(out.title, read.title, sizeof(out.title));
  std::memcpy(out.published, read.published, sizeof(out.published));
  return true;
}

bool parseTags(const char* json, const size_t len, Watched& out) {
  Span list;
  list.p = flatjson::skipSpace(json, json + len);
  list.end = json + len;
  if (list.p >= list.end || *list.p != '[') return false;
  char tag[sizeof(out.tag)] = "";
  text(member(flatjson::element(list, 0), "name"), tag, sizeof(tag));
  if (tag[0] == 0) return false;
  std::memcpy(out.tag, tag, sizeof(out.tag));
  out.title[0] = 0;
  out.published[0] = 0;
  return true;
}

void noteRelease(Watched& watched, const Watched& fetched, const char* today) {
  std::memcpy(watched.tag, fetched.tag, sizeof(watched.tag));
  std::memcpy(watched.title, fetched.title, sizeof(watched.title));
  std::memcpy(watched.published, fetched.published, sizeof(watched.published));
  if (watched.seenTag[0] == 0) std::memcpy(watched.seenTag, fetched.tag, sizeof(watched.seenTag));
  copyText(watched.checked, sizeof(watched.checked), today);
}

int findWatched(const Watched* list, const int count, const char* fullName) {
  for (int i = 0; i < count; ++i) {
    if (sameWords(list[i].fullName, fullName)) return i;
  }
  return -1;
}

int toggleWatched(Watched* list, int count, const char* fullName) {
  const int at = findWatched(list, count, fullName);
  if (at >= 0) {
    for (int i = at + 1; i < count; ++i) list[i - 1] = list[i];
    return count - 1;
  }
  if (count >= kMaxWatched) return count;
  list[count] = Watched{};
  copyText(list[count].fullName, sizeof(list[count].fullName), fullName);
  return count + 1;
}

void sortWatched(Watched* list, const int count) {
  auto before = [](const Watched& a, const Watched& b) {
    if (isNew(a) != isNew(b)) return isNew(a);
    return std::strcmp(a.published, b.published) > 0;
  };
  for (int i = 1; i < count; ++i) {
    const Watched moving = list[i];
    int at = i;
    while (at > 0 && before(moving, list[at - 1])) {
      list[at] = list[at - 1];
      --at;
    }
    list[at] = moving;
  }
}

std::string packWatched(const Watched* list, const int count) {
  std::string out;
  for (int i = 0; i < count; ++i) {
    const Watched& w = list[i];
    field(out, w.fullName);
    field(out, w.tag);
    field(out, w.seenTag);
    field(out, w.title);
    field(out, w.published);
    out += w.checked;
    out.push_back('\n');
  }
  return out;
}

int unpackWatched(const std::string_view all, Watched* out, const int cap) {
  int count = 0;
  size_t at = 0;
  while (at < all.size() && count < cap) {
    size_t end = all.find('\n', at);
    if (end == std::string_view::npos) end = all.size();
    std::string_view line = all.substr(at, end - at);
    at = end + 1;
    Watched w;
    char* const texts[6] = {w.fullName, w.tag, w.seenTag, w.title, w.published, w.checked};
    const size_t caps[6] = {sizeof(w.fullName), sizeof(w.tag),       sizeof(w.seenTag),
                            sizeof(w.title),    sizeof(w.published), sizeof(w.checked)};
    for (int f = 0; f < 6; ++f) {
      size_t tab = line.find('\t');
      if (tab == std::string_view::npos) tab = line.size();
      copyText(texts[f], caps[f], line.substr(0, tab));
      line.remove_prefix(tab < line.size() ? tab + 1 : line.size());
    }
    char tidy[sizeof(w.fullName)];
    if (tidyFullName(w.fullName, tidy, sizeof(tidy)) && findWatched(out, count, tidy) < 0) out[count++] = w;
  }
  return count;
}

}  // namespace discover
