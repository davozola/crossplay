#pragma once

// NEWS: one daily edition of Kagi News, four sections of at most eight
// stories, kept on the card.
//
// Freestanding on purpose: no Arduino, no renderer, no SD card, so
// host-tests/news/ can drive all of it on a laptop.
//
// The data is Kagi's public API (news.kagi.com/api, no key), licensed CC BY-NC
// 4.0: free with attribution, for non-commercial use. Every story this app
// shows ends with that attribution, and nothing here may be used commercially.
//
// A category's stories come as a few hundred kilobytes of JSON, three quarters
// of it image and favicon addresses. So it is scanned as it arrives off the
// socket and never assembled: one story's text is in hand at a time.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace news {

// A section is one Kagi category the reader chose. Eight at most, of eight
// stories each: an edition stays a thing that is read, not scrolled.
constexpr int kMaxSections = 8;
constexpr int kStoriesPerSection = 8;
// Asked of the API per category; more than is kept, so that a story dropped as
// another section's duplicate has a replacement.
constexpr int kStoriesAsked = 12;
constexpr int kSourcesShown = 6;

struct Category {
  std::string slug;  // Kagi's categoryId: "world"
  std::string name;  // "World"
  std::string uuid;  // this batch's id for it; empty when read off the card
};

// What a card that has never chosen starts with.
extern const Category kDefaultSections[];
extern const int kDefaultSectionCount;

constexpr const char* kApiCategories = "https://news.kagi.com/api/batches/latest/categories";
// The stories of one category, by the per-batch UUID the list above gives it.
std::string storiesUrl(std::string_view categoryUuid);

// --- A JSON document read as it arrives ---------------------------------------

constexpr int kJsonMaxDepth = 12;

// Where the scanner is: the key (or array position) at each level.
struct JsonPath {
  int depth = 0;
  std::string key[kJsonMaxDepth];  // the member name; empty inside an array
  bool inArray[kJsonMaxDepth] = {};
  int index[kJsonMaxDepth] = {};  // position within the enclosing array

  // True when the path, without array positions, is exactly these keys.
  bool is(std::initializer_list<const char*> keys) const;
};

class JsonHandler {
 public:
  virtual ~JsonHandler() = default;
  // Asked as a string or number starts: how many bytes of it to keep. 0 skips
  // it, which is what makes a megabyte of image addresses free.
  virtual size_t wants(const JsonPath& path) = 0;
  virtual void value(const JsonPath& path, std::string& text) = 0;
  // An object closed. Returning false stops the scan.
  virtual bool objectEnd(const JsonPath& path) = 0;
};

class JsonStream {
 public:
  explicit JsonStream(JsonHandler& handler) : handler_(handler) {}
  // False once the handler has had enough or the document is not JSON.
  bool feed(const char* data, size_t length);
  bool failed() const { return failed_; }
  bool stopped() const { return stopped_; }

 private:
  enum class State : uint8_t { Value, String, Escape, Unicode, Scalar, AfterValue };

  void push(bool array);
  bool pop();
  void endValue();
  void appendCodepoint(uint32_t cp);

  JsonHandler& handler_;
  JsonPath path_;
  State state_ = State::Value;
  bool stringIsKey_ = false;
  bool expectKey_ = false;
  bool failed_ = false;
  bool stopped_ = false;
  size_t cap_ = 0;
  std::string text_;
  std::string keyText_;
  uint32_t unicode_ = 0;
  int unicodeDigits_ = 0;
  uint32_t highSurrogate_ = 0;
};

// --- The category list --------------------------------------------------------

// Every English-language category in the batch. Kagi has a couple of hundred,
// most of them in other languages or about one place.
class CategoriesReader final : public JsonHandler {
 public:
  size_t wants(const JsonPath& path) override;
  void value(const JsonPath& path, std::string& text) override;
  bool objectEnd(const JsonPath& path) override;

  std::string batchId;
  std::string createdAt;
  std::vector<Category> categories;

 private:
  Category one_;
  std::string language_;
};

// The category list, and the reader's choice from it, as kept on the card.
std::string packCategories(const std::vector<Category>& categories);
void unpackCategories(std::string_view text, std::vector<Category>& categories);

// --- A story ------------------------------------------------------------------

struct Source {
  std::string title;
  std::string domain;
};

struct Story {
  std::string id;
  std::string title;
  std::string summary;
  int outlets = 0;
  std::string quote;
  std::string quoteAuthor;
  std::vector<std::string> points;
  std::vector<std::string> perspectives;
  // Context, each already a paragraph: background, geopolitics, economics, the
  // business angle, impact, technical detail, significance.
  std::vector<std::string> context;
  std::vector<std::string> timeline;  // "date: what happened"
  std::string outlook;
  std::vector<Source> sources;
  // The same event told in another category: the lead story's id, and the
  // other angles' ids.
  std::string mainId;
  std::vector<std::string> angleIds;
};

class StoriesReader final : public JsonHandler {
 public:
  class Sink {
   public:
    virtual ~Sink() = default;
    // False to stop the transfer.
    virtual bool story(Story& story) = 0;
  };
  explicit StoriesReader(Sink& sink) : sink_(sink) {}

  size_t wants(const JsonPath& path) override;
  void value(const JsonPath& path, std::string& text) override;
  bool objectEnd(const JsonPath& path) override;
  int seen() const { return seen_; }

 private:
  Sink& sink_;
  Story story_;
  std::string timelineDate_;
  int seen_ = 0;
};

// Kagi's text as this panel can set it: the [outlet#3] citation marks out,
// typographic characters folded, Latin Extended letters reduced to the letter
// under the accent, anything else the reading face has no glyph for dropped.
std::string cleanText(std::string_view text);

// The whole story as the reader pages it. The headline is the first paragraph;
// then the summary, a quote, key points, perspectives, context, timeline, what
// is next, the original sources, and the attribution the licence asks for. A
// part with nothing in it is left out, heading and all.
std::string buildDocument(const Story& story);

// A story's pages when the first one gives some of its lines to the headline.
struct Paging {
  uint32_t lines = 0;  // of body text
  uint32_t first = 1;  // lines the first page holds
  uint32_t rest = 1;   // lines every later page holds
  uint32_t pages() const;
  uint32_t topOf(uint32_t page) const;
  uint32_t pageOf(uint32_t line) const;
};

// Whether a story is told once across the whole edition.
class Deduper {
 public:
  // True the first time an event is offered; remembers it and its other angles.
  bool accept(const Story& story);

 private:
  bool known(const std::string& id) const;
  std::vector<std::string> ids_;
  std::vector<std::string> titles_;
};

// --- What is kept on the card ---------------------------------------------------

struct Item {
  std::string title;
  std::string id;       // Kagi's cluster id; names the story's file
  std::string summary;  // the opening of the summary, for the lead
};

constexpr size_t kLeadCap = 320;

std::string packSection(const std::vector<Item>& items, uint32_t edition);
bool unpackSection(std::string_view text, std::vector<Item>& items, uint32_t& edition);

// The name a story is kept under on the card: eight hex digits of its id and
// ".txt". A section's list is kept the same way, from its slug.
std::string articleFile(std::string_view id);

// "2026-11-18T12:00:13.366Z" and the like, as seconds since 1970; 0 otherwise.
uint32_t parseDate(std::string_view text);
// "18 NOV" for an edition's date; "" for 0.
void editionLabel(uint32_t when, char* out, size_t cap);

}  // namespace news
