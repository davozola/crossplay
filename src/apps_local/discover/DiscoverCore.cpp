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
constexpr const char* kStarNames[kStarChoices] = {"10+ STARS", "50+ STARS", "200+ STARS", "1000+ STARS"};
constexpr int kStarFloors[kStarChoices] = {10, 50, 200, 1000};
constexpr const char* kAgeNames[kAges] = {"NEW THIS WEEK", "NEW THIS MONTH", "NEW THIS YEAR", "ANY AGE, ACTIVE"};

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

// A JSON string with its escapes undone, \u pairs included, as UTF-8.
void text(const Span& v, char* dest, const size_t destCap) {
  dest[0] = 0;
  if (!v.ok() || *v.p != '"') return;
  char out[512];
  const size_t cap = sizeof(out);
  size_t at = 0;
  for (const char* p = v.p + 1; p < v.end && *p != '"' && at + 5 < cap; ++p) {
    if (*p != '\\' || p + 1 >= v.end) {
      out[at++] = *p;
      continue;
    }
    ++p;
    if (*p == 'n' || *p == 'r' || *p == 't') {
      out[at++] = ' ';
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
        out[at++] = static_cast<char>(cp);
      } else if (cp < 0x800) {
        out[at++] = static_cast<char>(0xC0 | (cp >> 6));
        out[at++] = static_cast<char>(0x80 | (cp & 0x3F));
      } else if (cp < 0x10000) {
        out[at++] = static_cast<char>(0xE0 | (cp >> 12));
        out[at++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out[at++] = static_cast<char>(0x80 | (cp & 0x3F));
      } else {
        out[at++] = static_cast<char>(0xF0 | (cp >> 18));
        out[at++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out[at++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out[at++] = static_cast<char>(0x80 | (cp & 0x3F));
      }
    } else {
      out[at++] = *p;
    }
  }
  out[at] = 0;
  latinOnly(out);
  // What fits, never ending inside a two-byte letter.
  size_t n = std::strlen(out);
  if (n > destCap - 1) n = destCap - 1;
  if (n > 0 && (static_cast<unsigned char>(out[n - 1]) & 0xC0) == 0xC0) --n;
  std::memcpy(dest, out, n);
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

const char* topicName(const Topic topic) { return kTopicNames[clampIndex(topic, kTopics)]; }
const char* languageName(const Language language) { return kLanguageNames[clampIndex(language, kLanguages)]; }
const char* starsName(const Stars stars) { return kStarNames[clampIndex(stars, kStarChoices)]; }
const char* ageName(const Age age) { return kAgeNames[clampIndex(age, kAges)]; }
int starsFloor(const Stars stars) { return kStarFloors[clampIndex(stars, kStarChoices)]; }

bool packFilter(const Filter& filter, uint8_t out[4]) {
  out[0] = static_cast<uint8_t>(filter.topic);
  out[1] = static_cast<uint8_t>(filter.language);
  out[2] = static_cast<uint8_t>(filter.stars);
  out[3] = static_cast<uint8_t>(filter.age);
  return true;
}

bool unpackFilter(const uint8_t in[4], Filter& out) {
  if (in[0] >= kTopics || in[1] >= kLanguages || in[2] >= kStarChoices || in[3] >= kAges) return false;
  out.topic = static_cast<Topic>(in[0]);
  out.language = static_cast<Language>(in[1]);
  out.stars = static_cast<Stars>(in[2]);
  out.age = static_cast<Age>(in[3]);
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

void searchUrl(const Filter& filter, const int32_t today, char* out, const size_t cap) {
  std::string query;
  auto add = [&](const std::string& term) {
    if (!query.empty()) query.push_back('+');
    query += term;
  };
  const char* words = kTopicWords[clampIndex(filter.topic, kTopics)];
  if (words[0] != 0) add(std::string(words) + "+in:name,description,topics");
  const char* language = kLanguageQuery[clampIndex(filter.language, kLanguages)];
  if (language[0] != 0) add(std::string("language:") + language);
  add("stars:%3E=" + std::to_string(starsFloor(filter.stars)));
  static constexpr int kDaysBack[kAges] = {7, 30, 365, 30};
  char since[16];
  isoDate(today - kDaysBack[clampIndex(filter.age, kAges)], since, sizeof(since));
  add(std::string(filter.age == Age::AnyActive ? "pushed:%3E=" : "created:%3E=") + since);
  std::snprintf(out, cap, "https://api.github.com/search/repositories?q=%s&sort=stars&order=desc&per_page=%d",
                query.c_str(), kPerPage);
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

}  // namespace discover
