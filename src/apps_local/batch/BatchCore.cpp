#include "BatchCore.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../hackernews/HackerNewsCore.h"

namespace batch {

namespace {

constexpr std::string_view kIssuePath = "the-batch/issue-";
constexpr const char* kMonths[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                     "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

bool isDigit(const char c) { return c >= '0' && c <= '9'; }

int monthOf(const std::string_view text) {
  if (text.size() < 3) return 0;
  for (int m = 0; m < 12; ++m) {
    bool same = true;
    for (int i = 0; i < 3; ++i) {
      char c = text[static_cast<size_t>(i)];
      if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
      same = same && c == kMonths[m][i];
    }
    if (same) return m + 1;
  }
  return 0;
}

void setDate(Issue& issue, const int day, const int month, const int year) {
  if (day < 1 || day > 31 || month < 1 || month > 12 || year < 2000 || year > 2999) return;
  std::snprintf(issue.date, sizeof(issue.date), "%02d %s %04d", day, kMonths[month - 1], year);
}

// The issue number after the last "the-batch/issue-" in `text`, or 0.
int issueNumberIn(const std::string_view text) {
  const size_t at = text.rfind(kIssuePath);
  if (at == std::string_view::npos) return 0;
  int n = 0;
  int digits = 0;
  for (size_t i = at + kIssuePath.size(); i < text.size() && isDigit(text[i]) && digits < 5; ++i, ++digits) {
    n = n * 10 + (text[i] - '0');
  }
  return digits > 0 && n < 100000 ? n : 0;
}

Issue* find(std::vector<Issue>& issues, const int number) {
  for (Issue& issue : issues) {
    if (issue.number == number) return &issue;
  }
  return nullptr;
}

// Adds an issue, or fills in what an entry already there is missing.
bool merge(std::vector<Issue>& issues, const Issue& fresh) {
  if (fresh.number <= 0) return false;
  if (Issue* held = find(issues, fresh.number)) {
    if (held->title.empty() && !fresh.title.empty()) held->title = fresh.title;
    if (held->date[0] == 0 && fresh.date[0] != 0) std::memcpy(held->date, fresh.date, sizeof(held->date));
    return false;
  }
  issues.push_back(fresh);
  return true;
}

std::string_view between(const std::string_view text, const std::string_view open, const std::string_view close) {
  const size_t a = text.find(open);
  if (a == std::string_view::npos) return {};
  const size_t from = a + open.size();
  const size_t b = text.find(close, from);
  if (b == std::string_view::npos) return {};
  return text.substr(from, b - from);
}

std::string tidy(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\t')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\t')) text.remove_suffix(1);
  std::string out(text);
  hn::decodeEntities(out);
  for (char& c : out) {
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  }
  return out;
}

// "Fri, 02 Oct 2026 12:55:55 +0000"
void dateFromRfc822(const std::string_view text, Issue& issue) {
  size_t i = 0;
  while (i < text.size() && !isDigit(text[i])) ++i;
  int day = 0;
  while (i < text.size() && isDigit(text[i])) day = day * 10 + (text[i++] - '0');
  while (i < text.size() && text[i] == ' ') ++i;
  const int month = monthOf(text.substr(i));
  i += 3;
  while (i < text.size() && text[i] == ' ') ++i;
  int year = 0;
  while (i < text.size() && isDigit(text[i])) year = year * 10 + (text[i++] - '0');
  setDate(issue, day, month, year);
}

// "Oct 02, 2026" anywhere in `text`: the last one before `before`.
void dateNear(const std::string_view text, const size_t before, Issue& issue) {
  const size_t from = before > 400 ? before - 400 : 0;
  for (size_t i = before; i-- > from;) {
    const int month = monthOf(text.substr(i));
    if (month == 0 ||
        (i > 0 && ((text[i - 1] >= 'a' && text[i - 1] <= 'z') || (text[i - 1] >= 'A' && text[i - 1] <= 'Z')))) {
      continue;
    }
    size_t j = i + 3;
    if (j >= text.size() || text[j] != ' ') continue;
    ++j;
    int day = 0;
    int digits = 0;
    while (j < text.size() && isDigit(text[j]) && digits < 2) {
      day = day * 10 + (text[j++] - '0');
      ++digits;
    }
    if (digits == 0 || j + 5 >= text.size() || text[j] != ',' || text[j + 1] != ' ') continue;
    j += 2;
    int year = 0;
    digits = 0;
    while (j < text.size() && isDigit(text[j]) && digits < 4) {
      year = year * 10 + (text[j++] - '0');
      ++digits;
    }
    if (digits != 4) continue;
    setDate(issue, day, month, year);
    return;
  }
}

int countWords(const std::string& text) {
  int words = 0;
  bool in = false;
  for (const char c : text) {
    const bool space = c == ' ' || c == '\n';
    if (!space && !in) ++words;
    in = !space;
  }
  return words;
}

// A heading line's text, or empty when the line is not one.
std::string_view atxHeading(std::string_view line) {
  size_t hashes = 0;
  while (hashes < line.size() && line[hashes] == '#') ++hashes;
  if (hashes == 0 || hashes > 6 || hashes >= line.size() || line[hashes] != ' ') return {};
  line.remove_prefix(hashes + 1);
  while (!line.empty() && (line.back() == '#' || line.back() == ' ')) line.remove_suffix(1);
  return line;
}

bool underline(const std::string_view line) {
  if (line.size() < 3) return false;
  const char c = line[0];
  if (c != '=' && c != '-') return false;
  for (const char other : line) {
    if (other != c) return false;
  }
  return true;
}

// The heading as a title: links and emphasis taken off it.
std::string headingText(const std::string_view raw) {
  const std::vector<std::string> flat = hn::paragraphsFromMarkdown(raw);
  return flat.empty() ? std::string() : flat.front();
}

constexpr int kStoryWords = 60;

}  // namespace

int addFromFeed(const std::string_view xml, std::vector<Issue>& issues) {
  int added = 0;
  size_t at = 0;
  while (true) {
    const size_t open = xml.find("<item>", at);
    if (open == std::string_view::npos) break;
    const size_t close = xml.find("</item>", open);
    if (close == std::string_view::npos) break;
    const std::string_view item = xml.substr(open, close - open);
    at = close + 7;
    Issue issue;
    issue.number = issueNumberIn(between(item, "<link>", "</link>"));
    if (issue.number == 0) continue;
    issue.title = tidy(between(item, "<title>", "</title>"));
    dateFromRfc822(between(item, "<pubDate>", "</pubDate>"), issue);
    if (merge(issues, issue)) ++added;
  }
  return added;
}

int addFromFrontPage(const std::string_view markdown, std::vector<Issue>& issues) {
  int added = 0;
  size_t at = 0;
  while (true) {
    // "](https://www.deeplearning.ai/the-batch/issue-379...)" closes a link whose text is before it.
    const size_t path = markdown.find(kIssuePath, at);
    if (path == std::string_view::npos) break;
    at = path + kIssuePath.size();
    const size_t paren = markdown.rfind("](", path);
    if (paren == std::string_view::npos || path - paren > 80) continue;
    const size_t open = markdown.rfind('[', paren);
    if (open == std::string_view::npos || paren - open > 400) continue;
    Issue issue;
    issue.number = issueNumberIn(markdown.substr(path, 40));
    if (issue.number == 0) continue;
    std::string_view text = markdown.substr(open + 1, paren - open - 1);
    // An image inside the link is not the headline.
    if (text.find("![") != std::string_view::npos || text.find("](") != std::string_view::npos) text = {};
    if (text.size() >= 24) issue.title = tidy(text);
    dateNear(markdown, open, issue);
    if (merge(issues, issue)) ++added;
  }
  return added;
}

void sortIssues(std::vector<Issue>& issues) {
  std::sort(issues.begin(), issues.end(), [](const Issue& a, const Issue& b) { return a.number > b.number; });
}

int newestNumber(const std::vector<Issue>& issues) {
  int newest = 0;
  for (const Issue& issue : issues) newest = std::max(newest, issue.number);
  return newest;
}

std::string packIndex(const std::vector<Issue>& issues) {
  std::string out;
  out.reserve(issues.size() * 96);
  char head[32];
  for (const Issue& issue : issues) {
    std::snprintf(head, sizeof(head), "%d\t%s\t", issue.number % 100000, issue.date);
    out += head;
    out += issue.title;
    out += '\n';
  }
  return out;
}

void unpackIndex(const std::string_view text, std::vector<Issue>& issues) {
  issues.clear();
  size_t at = 0;
  while (at < text.size()) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = text.substr(at, end - at);
    at = end + 1;
    const size_t t1 = line.find('\t');
    if (t1 == std::string_view::npos) continue;
    const size_t t2 = line.find('\t', t1 + 1);
    if (t2 == std::string_view::npos) continue;
    Issue issue;
    for (size_t i = 0; i < t1 && isDigit(line[i]) && i < 5; ++i) issue.number = issue.number * 10 + (line[i] - '0');
    if (issue.number <= 0) continue;
    const std::string_view date = line.substr(t1 + 1, t2 - t1 - 1);
    if (date.size() < sizeof(issue.date)) {
      std::memcpy(issue.date, date.data(), date.size());
      issue.date[date.size()] = 0;
    }
    issue.title = std::string(line.substr(t2 + 1));
    issues.push_back(issue);
  }
}

void splitIssue(const std::string_view markdown, std::vector<Section>& sections) {
  sections.clear();
  // Lines, gathered under the heading they follow.
  struct Raw {
    std::string title;
    std::string body;
  };
  std::vector<Raw> raws;
  raws.push_back(Raw{});
  size_t at = 0;
  std::string_view previous;
  size_t previousStart = 0;
  while (at <= markdown.size()) {
    size_t end = markdown.find('\n', at);
    if (end == std::string_view::npos) end = markdown.size();
    std::string_view line = markdown.substr(at, end - at);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const std::string_view heading = atxHeading(line);
    if (!heading.empty()) {
      raws.push_back(Raw{headingText(heading), {}});
    } else if (underline(line) && !previous.empty() && atxHeading(previous).empty()) {
      // A setext heading: the line above was its text, and is already in the
      // body of the section before. Take it back out.
      std::string& body = raws.back().body;
      const size_t keep = previousStart <= body.size() ? previousStart : body.size();
      body.resize(keep);
      raws.push_back(Raw{headingText(previous), {}});
    } else {
      previousStart = raws.back().body.size();
      raws.back().body.append(line);
      raws.back().body.push_back('\n');
    }
    previous = line;
    if (end == markdown.size()) break;
    at = end + 1;
  }

  for (Raw& raw : raws) {
    Section section;
    section.paragraphs = hn::paragraphsFromMarkdown(raw.body);
    // A menu is a run of short lines; a story is prose.
    std::vector<std::string> prose;
    for (std::string& p : section.paragraphs) {
      section.words += countWords(p);
      prose.push_back(std::move(p));
    }
    section.paragraphs = std::move(prose);
    if (section.words < kStoryWords) continue;
    section.title = raw.title;
    bool letter = false;
    for (size_t i = 0; i < section.paragraphs.size() && i < 3; ++i) {
      letter = letter || section.paragraphs[i].rfind("Dear friends", 0) == 0;
    }
    if (letter) section.title = "Letter from Andrew Ng";
    if (section.title.empty()) section.title = "Opening";
    sections.push_back(std::move(section));
  }
}

int minutesFor(const int words) {
  const int minutes = (words + 110) / 220;
  return minutes < 1 ? 1 : minutes;
}

int wordsIn(const std::vector<Section>& sections) {
  int words = 0;
  for (const Section& section : sections) words += section.words;
  return words;
}

void issueUrl(const int number, char* out, const size_t cap) {
  std::snprintf(out, cap, "https://r.jina.ai/https://www.deeplearning.ai/the-batch/issue-%d/", number % 100000);
}

void frontPageUrl(char* out, const size_t cap) {
  std::snprintf(out, cap, "https://r.jina.ai/https://www.deeplearning.ai/the-batch/");
}

bool isRead(const Shelf& shelf, const int issue) {
  return issue > 0 && issue < 512 && (shelf.read[issue / 8] & (1u << (issue % 8))) != 0;
}

void markRead(Shelf& shelf, const int issue) {
  if (issue > 0 && issue < 512) shelf.read[issue / 8] |= static_cast<uint8_t>(1u << (issue % 8));
}

bool isFavourite(const Shelf& shelf, const int issue, const uint8_t section) {
  for (int i = 0; i < shelf.favouriteCount; ++i) {
    if (shelf.favourites[i].issue == issue && shelf.favourites[i].section == section) return true;
  }
  return false;
}

bool hasFavourite(const Shelf& shelf, const int issue) {
  for (int i = 0; i < shelf.favouriteCount; ++i) {
    if (shelf.favourites[i].issue == issue) return true;
  }
  return false;
}

void toggleFavourite(Shelf& shelf, const int issue, const uint8_t section) {
  if (issue <= 0 || issue > 65535) return;
  for (int i = 0; i < shelf.favouriteCount; ++i) {
    if (shelf.favourites[i].issue != issue || shelf.favourites[i].section != section) continue;
    for (int k = i + 1; k < shelf.favouriteCount; ++k) shelf.favourites[k - 1] = shelf.favourites[k];
    --shelf.favouriteCount;
    return;
  }
  if (shelf.favouriteCount == kMaxFavourites) {
    for (int k = 1; k < kMaxFavourites; ++k) shelf.favourites[k - 1] = shelf.favourites[k];
    --shelf.favouriteCount;
  }
  shelf.favourites[shelf.favouriteCount++] = Favourite{static_cast<uint16_t>(issue), section};
}

size_t packShelf(const Shelf& shelf, uint8_t* out, const size_t cap) {
  if (cap < kShelfBytes) return 0;
  std::memset(out, 0, kShelfBytes);
  out[0] = 'B';
  out[1] = 1;
  std::memcpy(out + 2, shelf.read, 64);
  out[66] = static_cast<uint8_t>(shelf.favouriteCount);
  for (int i = 0; i < shelf.favouriteCount; ++i) {
    out[67 + i * 3] = static_cast<uint8_t>(shelf.favourites[i].issue & 0xFF);
    out[68 + i * 3] = static_cast<uint8_t>(shelf.favourites[i].issue >> 8);
    out[69 + i * 3] = shelf.favourites[i].section;
  }
  return kShelfBytes;
}

bool unpackShelf(const uint8_t* in, const size_t size, Shelf& out) {
  if (size != kShelfBytes || in[0] != 'B' || in[1] != 1 || in[66] > kMaxFavourites) return false;
  Shelf shelf;
  std::memcpy(shelf.read, in + 2, 64);
  shelf.favouriteCount = in[66];
  for (int i = 0; i < shelf.favouriteCount; ++i) {
    shelf.favourites[i].issue = static_cast<uint16_t>(in[67 + i * 3] | (in[68 + i * 3] << 8));
    shelf.favourites[i].section = in[69 + i * 3];
  }
  out = shelf;
  return true;
}

}  // namespace batch
