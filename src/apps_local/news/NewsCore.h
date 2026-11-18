#pragma once

// NEWS: headlines read straight off each outlet's own feed, with no server of
// ours in between. Freestanding on purpose: no Arduino, no renderer, no SD
// card, so host-tests/news/ can drive all of it on a laptop.
//
// A feed is RSS 2.0 or Atom and can be hundreds of kilobytes, most of it full
// article bodies this screen never shows. So it is scanned as it arrives off
// the socket and never assembled: only a title, a link, a date and the start
// of a summary are kept for each item, and the transfer is cut once enough
// items are in hand.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace news {

struct Feed {
  const char* name;  // as the row shows it
  const char* url;
};

struct Section {
  const char* name;   // on the tab
  const char* title;  // on the band
  const Feed* feeds;
  int feedCount;
};

extern const Section kSections[];
extern const int kSectionCount;

struct Item {
  std::string title;
  std::string url;
  std::string summary;
  uint32_t when = 0;  // seconds since 1970, 0 when the feed gave no date
  uint8_t feed = 0;   // index into its section's feeds
};

// Items kept from one feed, and from a section once its feeds are merged.
constexpr int kPerFeed = 10;
constexpr int kPerSection = 40;
constexpr size_t kTitleCap = 300;
constexpr size_t kSummaryCap = 600;
constexpr size_t kUrlCap = 400;

// "Wed, 18 Nov 2026 20:15:00 GMT", "2026-11-18T20:15:00+01:00" and their
// common variations. 0 for anything else.
uint32_t parseDate(std::string_view text);

// Markup and entities out, whitespace collapsed, folded to what the reading
// face can draw. For display only.
std::string plainText(std::string_view html, size_t cap);

// "12m", "5h", "3d", or "" when either time is unknown.
void ageLabel(uint32_t when, uint32_t now, char* out, size_t cap);

class FeedScanner {
 public:
  FeedScanner(std::vector<Item>& out, uint8_t feed, int limit = kPerFeed);

  // Feed the response in whatever chunks arrive. Returns false once it wants
  // no more, which is the caller's signal to stop the transfer; full() says
  // whether that was the limit being reached rather than a refusal.
  bool feed(const char* data, size_t length);
  bool full() const { return kept_ >= limit_; }
  int kept() const { return kept_; }

 private:
  enum class Field : uint8_t { None, Title, Link, Summary, Date };

  void onTag();
  void onText(char c);
  void closeField();
  void closeItem();

  std::vector<Item>& out_;
  uint8_t feed_;
  int limit_;
  int kept_ = 0;

  // '<' seen, gathering up to '>'. Bounded: a tag longer than this is one whose
  // attributes this scanner has no use for.
  bool inTag_ = false;
  bool inCdata_ = false;
  bool inComment_ = false;
  std::string tag_;
  int tail_ = 0;  // how much of "]]>" or "-->" has matched

  int depth_ = 0;
  int itemDepth_ = -1;  // depth of the open <item>/<entry>, -1 outside one
  Field field_ = Field::None;
  int fieldDepth_ = 0;
  bool fieldRaw_ = false;  // the text came in CDATA, so it is not XML-escaped
  std::string text_;
  Item item_;
  bool summaryFromContent_ = false;
};

// Newest first, one row per address, at most kPerSection.
void mergeSection(std::vector<Item>& items);

// A section as it is kept on the card between visits.
std::string packSection(const std::vector<Item>& items, uint32_t fetched);
bool unpackSection(std::string_view text, std::vector<Item>& items, uint32_t& fetched);

// The name an opened article is kept under on the card: eight hex digits of
// its address and ".txt". Two addresses sharing one is possible and harmless:
// the later article replaces the earlier.
std::string articleFile(std::string_view url);

}  // namespace news
