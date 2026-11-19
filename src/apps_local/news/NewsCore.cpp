#include "NewsCore.h"

#include <Utf8.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace news {

const Category kDefaultSections[] = {
    {"world", "World", ""}, {"business", "Business", ""}, {"tech", "Technology", ""},
    {"ai", "AI", ""},       {"science", "Science", ""},
};
const int kDefaultSectionCount = static_cast<int>(sizeof(kDefaultSections) / sizeof(kDefaultSections[0]));

std::string storiesUrl(const std::string_view categoryUuid) {
  std::string url = "https://news.kagi.com/api/batches/latest/categories/";
  url += categoryUuid;
  url += "/stories?limit=" + std::to_string(kStoriesAsked);
  return url;
}

// --- JsonStream ---------------------------------------------------------------

bool JsonPath::is(const std::initializer_list<const char*> keys) const {
  auto want = keys.begin();
  for (int i = 0; i < depth; ++i) {
    if (inArray[i]) continue;
    if (want == keys.end() || key[i] != *want) return false;
    ++want;
  }
  return want == keys.end();
}

void JsonStream::push(const bool array) {
  if (path_.depth >= kJsonMaxDepth) {
    failed_ = true;
    return;
  }
  path_.inArray[path_.depth] = array;
  path_.index[path_.depth] = 0;
  path_.key[path_.depth].clear();
  ++path_.depth;
  expectKey_ = !array;
}

bool JsonStream::pop() {
  if (path_.depth <= 0) {
    failed_ = true;
    return false;
  }
  const bool wasObject = !path_.inArray[path_.depth - 1];
  --path_.depth;
  expectKey_ = false;
  if (wasObject && !handler_.objectEnd(path_)) {
    stopped_ = true;
    return false;
  }
  return true;
}

void JsonStream::endValue() {
  if (cap_ > 0) handler_.value(path_, text_);
  text_.clear();
  cap_ = 0;
}

void JsonStream::appendCodepoint(const uint32_t cp) {
  std::string& out = stringIsKey_ ? keyText_ : text_;
  const size_t cap = stringIsKey_ ? 48 : cap_;
  if (out.size() + 4 > cap) return;
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
}

bool JsonStream::feed(const char* data, const size_t length) {
  for (size_t i = 0; i < length; ++i) {
    if (failed_ || stopped_) return false;
    const char c = data[i];
    switch (state_) {
      case State::Scalar:
        if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\n' || c == '\r' || c == '\t') {
          endValue();
          state_ = State::Value;
          --i;  // the delimiter is structure; read it again as such
        } else if (text_.size() < cap_) {
          text_ += c;
        }
        break;
      case State::String:
        if (c == '\\') {
          state_ = State::Escape;
        } else if (c == '"') {
          if (stringIsKey_) {
            if (path_.depth > 0) path_.key[path_.depth - 1] = keyText_;
            expectKey_ = false;
          } else {
            endValue();
          }
          state_ = State::Value;
        } else if (stringIsKey_) {
          if (keyText_.size() < 48) keyText_ += c;
        } else if (text_.size() < cap_) {
          text_ += c;
        }
        break;
      case State::Escape:
        state_ = State::String;
        if (c == 'u') {
          state_ = State::Unicode;
          unicode_ = 0;
          unicodeDigits_ = 0;
        } else {
          const char plain = c == 'n' ? '\n' : (c == 't' || c == 'r' || c == 'b' || c == 'f') ? ' ' : c;
          appendCodepoint(static_cast<uint8_t>(plain));
        }
        break;
      case State::Unicode: {
        const int digit = c >= '0' && c <= '9'   ? c - '0'
                          : c >= 'a' && c <= 'f' ? c - 'a' + 10
                          : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                 : -1;
        if (digit < 0) {
          failed_ = true;
          return false;
        }
        unicode_ = unicode_ * 16 + static_cast<uint32_t>(digit);
        if (++unicodeDigits_ < 4) break;
        state_ = State::String;
        if (unicode_ >= 0xD800 && unicode_ < 0xDC00) {
          highSurrogate_ = unicode_;
        } else if (unicode_ >= 0xDC00 && unicode_ < 0xE000) {
          if (highSurrogate_ != 0) {
            appendCodepoint(0x10000 + ((highSurrogate_ - 0xD800) << 10) + (unicode_ - 0xDC00));
          }
          highSurrogate_ = 0;
        } else {
          highSurrogate_ = 0;
          appendCodepoint(unicode_);
        }
        break;
      }
      case State::Value:
      case State::AfterValue:
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ':') break;
        if (c == ',') {
          if (path_.depth > 0) {
            if (path_.inArray[path_.depth - 1]) {
              ++path_.index[path_.depth - 1];
            } else {
              expectKey_ = true;
            }
          }
        } else if (c == '{') {
          push(false);
        } else if (c == '[') {
          push(true);
        } else if (c == '}' || c == ']') {
          if (!pop()) return false;
        } else if (c == '"') {
          stringIsKey_ = expectKey_ && path_.depth > 0 && !path_.inArray[path_.depth - 1];
          if (stringIsKey_) {
            keyText_.clear();
          } else {
            cap_ = handler_.wants(path_);
            text_.clear();
          }
          state_ = State::String;
        } else {
          stringIsKey_ = false;
          cap_ = handler_.wants(path_);
          text_.clear();
          if (cap_ > 0) text_ += c;
          state_ = State::Scalar;
        }
        break;
    }
  }
  return !failed_ && !stopped_;
}

// --- CategoriesReader -----------------------------------------------------------

size_t CategoriesReader::wants(const JsonPath& path) {
  if (path.is({"batchId"}) || path.is({"createdAt"})) return 64;
  if (path.depth == 3 && path.key[0] == "categories") {
    const std::string& field = path.key[2];
    if (field == "id" || field == "categoryId" || field == "categoryName" || field == "sourceLanguage") return 96;
  }
  return 0;
}

void CategoriesReader::value(const JsonPath& path, std::string& text) {
  if (path.is({"batchId"})) {
    batchId = text;
  } else if (path.is({"createdAt"})) {
    createdAt = text;
  } else if (path.depth == 3) {
    const std::string& field = path.key[2];
    if (field == "id") {
      one_.uuid = text;
    } else if (field == "categoryId") {
      one_.slug = text;
    } else if (field == "categoryName") {
      one_.name = cleanText(text);
    } else if (field == "sourceLanguage") {
      language_ = text;
    }
  }
}

bool CategoriesReader::objectEnd(const JsonPath& path) {
  if (path.depth != 2 || !path.is({"categories"})) return true;
  // A category with no language given is one of Kagi's own, which are English.
  const bool english = language_.empty() || language_ == "en";
  if (english && !one_.slug.empty() && !one_.uuid.empty() && categories.size() < 400) {
    if (one_.name.empty()) one_.name = one_.slug;
    categories.push_back(one_);
  }
  one_ = Category{};
  language_.clear();
  return true;
}

std::string packCategories(const std::vector<Category>& categories) {
  std::string out;
  for (const Category& c : categories) {
    if (c.slug.empty() || c.slug.find_first_of("\t\n") != std::string::npos) continue;
    std::string name = c.name;
    for (char& ch : name) {
      if (ch == '\t' || ch == '\n') ch = ' ';
    }
    out += c.slug + "\t" + name + "\n";
  }
  return out;
}

void unpackCategories(const std::string_view text, std::vector<Category>& categories) {
  categories.clear();
  size_t at = 0;
  while (at < text.size() && categories.size() < 400) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view row = text.substr(at, end - at);
    at = end + 1;
    const size_t tab = row.find('\t');
    if (tab == std::string_view::npos || tab == 0) continue;
    Category c;
    c.slug = std::string(row.substr(0, tab));
    c.name = std::string(row.substr(tab + 1));
    if (c.name.empty()) c.name = c.slug;
    categories.push_back(std::move(c));
  }
}

// --- Text ---------------------------------------------------------------------

namespace {

// U+0100..U+017F, each as the plain letter under its accent.
constexpr char kLatinExtendedA[] =
    "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiJjJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUu"
    "Uu"
    "WwYyYZzZzZzs";

// "[france24.com#1]": an opening bracket, no spaces, a '#', digits, a closing one.
size_t citationEnd(const std::string_view s, const size_t open) {
  size_t hash = std::string_view::npos;
  for (size_t i = open + 1; i < s.size() && i < open + 80; ++i) {
    const char c = s[i];
    if (c == ' ' || c == '[' || c == '\n') return 0;
    if (c == '#') {
      hash = i;
    } else if (c == ']') {
      if (hash == std::string_view::npos || hash + 1 == i) return 0;
      for (size_t d = hash + 1; d < i; ++d) {
        if (s[d] < '0' || s[d] > '9') return 0;
      }
      return i + 1;
    }
  }
  return 0;
}

void appendLine(std::string& out, const std::string& text) {
  if (text.empty()) return;
  out += text;
  out += "\n\n";
}

void appendList(std::string& out, const char* heading, const std::vector<std::string>& items) {
  bool any = false;
  for (const std::string& item : items) any = any || !item.empty();
  if (!any) return;
  out += heading;
  out += "\n\n";
  for (const std::string& item : items) {
    if (item.empty()) continue;
    out += "- ";
    out += item;
    out += "\n\n";
  }
}

std::string normalised(const std::string& title) {
  std::string out;
  for (const char c : title) {
    if (c >= 'A' && c <= 'Z') {
      out += static_cast<char>(c + 32);
    } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (static_cast<uint8_t>(c) & 0x80) != 0) {
      out += c;
    }
  }
  return out;
}

long daysFromCivil(int y, const int m, const int d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153L * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
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

void flat(std::string& s) {
  for (char& c : s) {
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  }
}

}  // namespace

std::string cleanText(const std::string_view text) {
  std::string stripped;
  stripped.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '[') {
      const size_t end = citationEnd(text, i);
      if (end != 0) {
        if (!stripped.empty() && stripped.back() == ' ') stripped.pop_back();
        i = end - 1;
        continue;
      }
    }
    stripped += text[i];
  }
  const std::string folded = utf8FoldTypography(stripped);

  std::string out;
  out.reserve(folded.size());
  bool space = true;
  int newlines = 0;
  size_t i = 0;
  while (i < folded.size()) {
    const uint8_t c = static_cast<uint8_t>(folded[i]);
    if (c == '\n' || c == '\r') {
      ++newlines;
      ++i;
      continue;
    }
    std::string glyph;
    size_t width = 1;
    if (c < 0x80) {
      glyph = static_cast<char>(c == '\t' ? ' ' : c);
    } else if ((c & 0xE0) == 0xC0 && i + 1 < folded.size()) {
      width = 2;
      const uint32_t cp = (static_cast<uint32_t>(c & 0x1F) << 6) | (static_cast<uint8_t>(folded[i + 1]) & 0x3F);
      if (cp >= 0xA0 && cp <= 0xFF) {
        glyph = folded.substr(i, 2);
        if (cp == 0xA0) glyph = " ";
      } else if (cp >= 0x100 && cp < 0x100 + sizeof(kLatinExtendedA) - 1) {
        glyph = kLatinExtendedA[cp - 0x100];
      }
    } else if ((c & 0xF0) == 0xE0) {
      width = 3;
    } else if ((c & 0xF8) == 0xF0) {
      width = 4;
    }
    i += width;
    if (glyph.empty()) continue;
    if (newlines > 0 && !out.empty()) {
      while (!out.empty() && out.back() == ' ') out.pop_back();
      out += "\n\n";
      space = true;
    }
    newlines = 0;
    if (glyph == " ") {
      if (!space) out += ' ';
      space = true;
    } else {
      out += glyph;
      space = false;
    }
  }
  while (!out.empty() && (out.back() == ' ' || out.back() == '\n')) out.pop_back();
  return out;
}

std::string buildDocument(const Story& story) {
  std::string out;
  out.reserve(6000);
  appendLine(out, story.title);
  appendLine(out, story.summary);
  if (!story.quote.empty()) {
    std::string quote = "\"" + story.quote + "\"";
    if (!story.quoteAuthor.empty()) quote += " -- " + story.quoteAuthor;
    appendLine(out, quote);
  }
  appendList(out, "KEY POINTS", story.points);
  appendList(out, "PERSPECTIVES", story.perspectives);
  bool anyContext = false;
  for (const std::string& part : story.context) anyContext = anyContext || !part.empty();
  if (anyContext) {
    appendLine(out, "CONTEXT");
    for (const std::string& part : story.context) appendLine(out, part);
  }
  appendList(out, "TIMELINE", story.timeline);
  if (!story.outlook.empty()) {
    appendLine(out, "WHAT COMES NEXT");
    appendLine(out, story.outlook);
  }
  if (!story.sources.empty()) {
    appendLine(out, "ORIGINAL SOURCES");
    for (const Source& source : story.sources) {
      std::string line = "- " + source.domain;
      if (!source.title.empty()) line += ": " + source.title;
      appendLine(out, line);
    }
    if (story.outlets > static_cast<int>(story.sources.size())) {
      appendLine(out, std::to_string(story.outlets) + " outlets in all.");
    }
  }
  out += "Synthesised by Kagi News from the outlets cited. Licensed CC BY-NC 4.0, non-commercial use. news.kagi.com";
  out += "\n";
  return out;
}

uint32_t Paging::pages() const {
  if (lines <= first) return 1;
  const uint32_t each = rest < 1 ? 1 : rest;
  return 1 + (lines - first + each - 1) / each;
}

uint32_t Paging::topOf(const uint32_t page) const { return page == 0 ? 0 : first + (page - 1) * (rest < 1 ? 1 : rest); }

uint32_t Paging::pageOf(const uint32_t line) const {
  if (line < first) return 0;
  const uint32_t page = 1 + (line - first) / (rest < 1 ? 1 : rest);
  return page < pages() ? page : pages() - 1;
}

// --- StoriesReader ------------------------------------------------------------

namespace {

constexpr size_t kListCap = 8;

// The string arrays that become bulleted context, in the order they are shown.
constexpr const char* kContextLists[] = {"international_reactions", "key_players",
                                         "industry_impact",         "technical_details",
                                         "scientific_significance", "business_angle_points"};
// The single paragraphs of context, in the order they are shown.
constexpr const char* kContextTexts[] = {"historical_background", "geopolitical_context", "economic_implications",
                                         "business_angle_text"};

}  // namespace

size_t StoriesReader::wants(const JsonPath& path) {
  if (path.depth < 3 || path.key[0] != "stories") return 0;
  const std::string& field = path.key[2];
  if (path.depth == 3) {
    if (field == "id" || field == "unique_domains") return 64;
    if (field == "title") return 300;
    if (field == "short_summary") return 6000;
    if (field == "quote") return 500;
    if (field == "quote_author") return 160;
    if (field == "future_outlook") return 2500;
    for (const char* name : kContextTexts) {
      if (field == name) return 2500;
    }
    return 0;
  }
  if (path.depth == 4 && path.inArray[3]) {
    if (path.index[3] >= static_cast<int>(kListCap)) return 0;
    if (field == "talking_points") return 1200;
    for (const char* name : kContextLists) {
      if (field == name) return 1200;
    }
    return 0;
  }
  if (path.depth == 4 && field == "main_story") return path.key[3] == "cluster_id" ? 64 : 0;
  if (path.depth == 5 && path.inArray[3]) {
    const std::string& member = path.key[4];
    if (field == "other_angles") return member == "cluster_id" && path.index[3] < 24 ? 64 : 0;
    if (path.index[3] >= static_cast<int>(kListCap)) return 0;
    if (field == "perspectives") return member == "text" ? 1200 : 0;
    if (field == "timeline") return member == "date" ? 48 : member == "content" ? 700 : 0;
    if (field == "articles") {
      if (path.index[3] >= kSourcesShown) return 0;
      return member == "title" ? 220 : member == "domain" ? 96 : 0;
    }
  }
  return 0;
}

void StoriesReader::value(const JsonPath& path, std::string& text) {
  const std::string& field = path.key[2];
  if (path.depth == 3) {
    if (field == "id") {
      story_.id = text;
    } else if (field == "unique_domains") {
      story_.outlets = std::atoi(text.c_str());
    } else if (field == "title") {
      story_.title = cleanText(text);
    } else if (field == "short_summary") {
      story_.summary = cleanText(text);
    } else if (field == "quote") {
      story_.quote = cleanText(text);
    } else if (field == "quote_author") {
      story_.quoteAuthor = cleanText(text);
    } else if (field == "future_outlook") {
      story_.outlook = cleanText(text);
    } else {
      // The paragraphs of context keep their place whatever order they arrive in.
      for (size_t i = 0; i < sizeof(kContextTexts) / sizeof(kContextTexts[0]); ++i) {
        if (field != kContextTexts[i]) continue;
        if (story_.context.size() < 10) story_.context.resize(10);
        story_.context[i] = cleanText(text);
      }
    }
    return;
  }
  if (path.depth == 4 && path.inArray[3]) {
    if (field == "talking_points") {
      story_.points.push_back(cleanText(text));
      return;
    }
    for (size_t i = 0; i < sizeof(kContextLists) / sizeof(kContextLists[0]); ++i) {
      if (field != kContextLists[i]) continue;
      if (story_.context.size() < 10) story_.context.resize(10);
      std::string& slot = story_.context[4 + i];
      const std::string item = cleanText(text);
      if (item.empty()) return;
      if (!slot.empty()) slot += "\n\n";
      slot += "- " + item;
    }
    return;
  }
  if (path.depth == 4 && field == "main_story") {
    story_.mainId = text;
    return;
  }
  if (path.depth == 5) {
    const std::string& member = path.key[4];
    if (field == "other_angles") {
      story_.angleIds.push_back(text);
    } else if (field == "perspectives") {
      story_.perspectives.push_back(cleanText(text));
    } else if (field == "timeline") {
      if (member == "date") {
        timelineDate_ = cleanText(text);
      } else {
        const std::string what = cleanText(text);
        story_.timeline.push_back(timelineDate_.empty() ? what : timelineDate_ + ": " + what);
        timelineDate_.clear();
      }
    } else if (field == "articles") {
      const size_t at = static_cast<size_t>(path.index[3]);
      if (story_.sources.size() <= at) story_.sources.resize(at + 1);
      if (member == "title") {
        story_.sources[at].title = cleanText(text);
      } else {
        story_.sources[at].domain = cleanText(text);
      }
    }
  }
}

bool StoriesReader::objectEnd(const JsonPath& path) {
  if (path.depth != 2 || path.key[0] != "stories" || !path.inArray[1]) return true;
  Story done = std::move(story_);
  story_ = Story{};
  timelineDate_.clear();
  ++seen_;
  if (done.id.empty() || done.title.empty() || done.summary.empty()) return true;
  return sink_.story(done);
}

// --- Deduper ------------------------------------------------------------------

bool Deduper::known(const std::string& id) const {
  if (id.empty()) return false;
  for (const std::string& seen : ids_) {
    if (seen == id) return true;
  }
  return false;
}

bool Deduper::accept(const Story& story) {
  if (known(story.id) || known(story.mainId)) return false;
  for (const std::string& angle : story.angleIds) {
    if (known(angle)) return false;
  }
  const std::string title = normalised(story.title);
  for (const std::string& seen : titles_) {
    if (seen == title) return false;
  }
  ids_.push_back(story.id);
  if (!story.mainId.empty()) ids_.push_back(story.mainId);
  for (const std::string& angle : story.angleIds) ids_.push_back(angle);
  titles_.push_back(title);
  return true;
}

// --- The card -----------------------------------------------------------------

std::string packSection(const std::vector<Item>& items, const uint32_t edition) {
  std::string out = "NEWS\t2\t" + std::to_string(edition) + "\n";
  for (const Item& item : items) {
    std::string title = item.title;
    std::string summary = item.summary;
    flat(title);
    flat(summary);
    // The empty column was a kicker; it stays so older favourites still read.
    out += item.id + "\t" + title + "\t\t" + summary + "\n";
  }
  return out;
}

bool unpackSection(const std::string_view text, std::vector<Item>& items, uint32_t& edition) {
  items.clear();
  edition = 0;
  size_t at = text.find('\n');
  if (at == std::string_view::npos || text.compare(0, 7, "NEWS\t2\t") != 0) return false;
  edition = static_cast<uint32_t>(std::strtoul(std::string(text.substr(7, at - 7)).c_str(), nullptr, 10));
  ++at;
  while (at < text.size() && items.size() < 64) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view row = text.substr(at, end - at);
    at = end + 1;
    std::string_view part[4];
    size_t from = 0;
    int n = 0;
    while (n < 4) {
      const size_t tab = n < 3 ? row.find('\t', from) : std::string_view::npos;
      part[n++] = row.substr(from, tab == std::string_view::npos ? std::string_view::npos : tab - from);
      if (tab == std::string_view::npos) break;
      from = tab + 1;
    }
    if (n < 2 || part[0].empty() || part[1].empty()) continue;
    Item item;
    item.id = std::string(part[0]);
    item.title = std::string(part[1]);
    if (n > 3) item.summary = std::string(part[3]);
    items.push_back(std::move(item));
  }
  return true;
}

std::string articleFile(const std::string_view id) {
  uint32_t hash = 2166136261u;  // FNV-1a
  for (const char c : id) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  char name[16];
  std::snprintf(name, sizeof(name), "%08lx.txt", static_cast<unsigned long>(hash));
  return name;
}

uint32_t parseDate(const std::string_view text) {
  size_t at = 0;
  if (text.size() < 10 || text[4] != '-' || text[7] != '-') return 0;
  const int y = number(text, at, 4);
  ++at;
  const int mo = number(text, at, 2);
  ++at;
  const int d = number(text, at, 2);
  int h = 0;
  int mi = 0;
  int s = 0;
  if (at < text.size() && (text[at] == 'T' || text[at] == ' ')) {
    ++at;
    h = number(text, at, 2);
    if (at < text.size() && text[at] == ':') ++at;
    mi = number(text, at, 2);
    if (at < text.size() && text[at] == ':') {
      ++at;
      s = number(text, at, 2);
    }
  }
  if (y < 1990 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || mi < 0 || s < 0) return 0;
  const long long t = daysFromCivil(y, mo, d) * 86400LL + h * 3600LL + mi * 60LL + s;
  return t > 0 && t < 0xFFFFFFFFLL ? static_cast<uint32_t>(t) : 0;
}

void editionLabel(const uint32_t when, char* out, const size_t cap) {
  if (cap == 0) return;
  out[0] = '\0';
  if (when == 0) return;
  // Civil date from days since 1970 (Howard Hinnant's algorithm, inverted).
  const long z = static_cast<long>(when / 86400u) + 719468;
  const long era = z / 146097;
  const long doe = z - era * 146097;
  const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long mp = (5 * doy + 2) / 153;
  const int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
  constexpr const char* kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  std::snprintf(out, cap, "%d %s", day, kMonths[month - 1]);
}

}  // namespace news
