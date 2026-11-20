#pragma once

// WIKIPEDIA over the network, the part with no device in it. Freestanding
// C++17, tested in host-tests/wikionline.
//
// The app asks es.wikipedia.org three things: what matches a query, a random
// article, and one article's text. The text comes from the TextExtracts API
// (prop=extracts): the article already stripped of tables, images, references
// and links, as a small set of tags inside one JSON string. ExtractReader
// reads that answer a few bytes at a time and writes the well-formed XHTML the
// book engine lays out (h2 h3 h4 p ul ol li b i sub sup), so an article never
// sits in memory whole.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wikies {

struct Hit {
  uint32_t id = 0;
  std::string title;
};

// ------------------------------------------------------------------ urls

// Titles that start with the query first, then full-text matches.
std::string searchUrl(const std::string& query);
std::string randomUrl();
std::string articleUrl(uint32_t pageId);
// The same article asked for by name, for the lists that only carry titles.
std::string articleUrlByTitle(const std::string& title);
// The most viewed pages of a day, or of a whole month with day 0.
std::string topUrl(int year, int month, int day);
// Twenty articles of a category of the wiki ("Wikipedia:Artículos
// destacados"), in title order from `startAt` on.
std::string categoryUrl(const std::string& category, const std::string& startAt);

// ---------------------------------------------------------------- answers

// The hits of a search or random answer, in the order above, without repeats,
// at most `max` appended. Returns how many.
int parseHits(const char* json, size_t len, int max, std::vector<Hit>& out);

// The articles of a most-viewed answer, in rank order, without the pages that
// are not articles (the main page, the search page, files...). They carry a
// title and no id. The answer may be cut short: what is whole is read.
int parseTop(const char* json, size_t len, int max, std::vector<Hit>& out);

// Case and accents folded, for matching what was typed against saved titles.
std::string foldTitle(const std::string& title);
// Whether every word typed appears in the title, folded.
bool titleMatches(const std::string& title, const std::string& query);

// ---------------------------------------------------------------- article

class Sink {
 public:
  virtual ~Sink() = default;
  virtual bool write(const char* data, size_t len) = 0;
};

class ExtractReader {
 public:
  explicit ExtractReader(Sink& sink);

  // A last paragraph, in italics, written after an article that has text: who
  // the words belong to.
  void setCredit(const std::string& credit) { credit_ = credit; }
  void feed(const char* data, size_t len);
  // Closes the document. True when an article with some text was written.
  bool finish();

  const std::string& title() const { return title_; }
  uint32_t pageId() const { return pageId_; }
  // The answer said there is no such page.
  bool missing() const { return missing_; }
  // The h2 headings written, in order; heading N carries id="sN" (1-based).
  const std::vector<std::string>& headings() const { return headings_; }
  size_t bytesOut() const { return bytesOut_; }
  bool writeFailed() const { return writeFailed_; }

 private:
  enum class El : uint8_t { P, Ul, Ol, Li, B, I, Sub, Sup };
  struct Open {
    El el;
    bool emitted;
  };
  struct PendingHead {
    int level;
    std::string text;
  };
  static constexpr int kMaxDepth = 24;

  // json
  void jsonChar(char c);
  void stringChar(char c);
  void stringEnd();
  void emitCodepoint(uint32_t cp);
  void valueByte(char c);

  // html
  void htmlByte(char c);
  void handleTag();
  void openTag(const char* name);
  void closeTag(const char* name);
  void endHeading();
  void entityDone(bool terminated);
  void textByte(char c);
  void textCodepoint(uint32_t cp);

  // tree
  void push(El el);
  void popThrough(int index);
  void closeAll();
  void closeInline();
  void closeParagraph();
  int find(El el) const;
  bool inContainer() const;
  void emitOpens();
  void flushHeads();

  // out
  void out(const char* s);
  void out(const char* s, size_t n);
  void outEscaped(char c);
  void flushOut();

  Sink& sink_;

  // json state
  bool inString_ = false;
  bool escape_ = false;
  int unicodeLeft_ = 0;
  uint32_t unicode_ = 0;
  uint32_t highSurrogate_ = 0;
  bool expectValue_ = false;
  bool stringIsValue_ = false;
  std::string key_;      // the string being read when it may be a key
  std::string lastKey_;  // the key the next value belongs to
  std::string number_;
  enum class Target : uint8_t { None, Extract, Title } target_ = Target::None;

  // html state
  enum class Html : uint8_t { Text, Tag, Comment, Entity } html_ = Html::Text;
  std::string tag_;
  std::string entity_;
  int commentDashes_ = 0;
  char skipName_[12] = {0};  // an element dropped with everything in it
  int skipDepth_ = 0;
  bool inHeading_ = false;
  int headLevel_ = 0;
  std::string head_;
  std::vector<PendingHead> pendingHeads_;
  bool skipSection_ = false;  // under a heading whose section is left out
  Open stack_[kMaxDepth];
  int depth_ = 0;
  bool lastWasSpace_ = true;

  // results
  std::string credit_;
  std::string title_;
  uint32_t pageId_ = 0;
  bool missing_ = false;
  std::vector<std::string> headings_;
  size_t bytesOut_ = 0;
  bool started_ = false;
  bool anyText_ = false;
  bool writeFailed_ = false;
  char buf_[1024];
  size_t bufLen_ = 0;
};

}  // namespace wikies
