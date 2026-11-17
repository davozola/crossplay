// Liner Notes: the index, the layout and the marks, on made-up albums.
#include <cstdio>
#include <cstring>

#include "LinerNotesCore.h"

using namespace linernotes;

static int failures = 0;
#define CHECK(cond)                                              \
  do {                                                           \
    if (!(cond)) {                                               \
      std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                \
    }                                                            \
  } while (0)

static int byBytes(void*, const char*, const size_t length, bool) { return static_cast<int>(length); }

int main() {
  // A two-album pack, entirely invented.
  const char* tsv =
      "LINERNOTES\t1\t2\t2026-01-01\n"
      "a1\tThe Example Band\tFirst Light\t1999\tNowhere\t9\t111\t0\t40\t0\t30\n"
      "b2\tOther People\tSecond Thoughts\t2004\t\t64\t222\t40\t10\t30\t12\n"
      "broken line\n";
  Index index;
  CHECK(index.parse(std::string(tsv)));
  CHECK(index.albums.size() == 2);
  CHECK(std::strcmp(index.str(index.albums[0].artist), "The Example Band") == 0);
  CHECK(index.albums[0].year == 1999);
  CHECK((index.albums[0].categories & kEssentialBit) != 0);
  CHECK((index.albums[0].categories & kHiddenBits) != 0);
  CHECK((index.albums[1].categories & kHiddenBits) != 0);
  CHECK(index.albums[1].offset[0] == 40 && index.albums[1].length[1] == 12);
  CHECK(std::strcmp(index.str(index.albums[1].label), "") == 0);
  CHECK(index.find("b2") == 1 && index.find("zz") == -1);
  Index bad;
  CHECK(!bad.parse(std::string("SOMETHING ELSE\n")));
  CHECK(!bad.parse(std::string("LINERNOTES\t2\t1\n")));

  // A record, wrapped at twenty bytes.
  const char* record =
      "W\tone two three four five six seven\n"
      "L\tfirst thing\n"
      "L\tsecond thing\n"
      "T\tA Song\tits note\n"
      "X\t1\tbecause it follows\n"
      "R\tA Source\thttps://example.invalid/a/very/long/address/that/does/not/fit\n";
  Document doc;
  layout(record, std::strlen(record), false, 20, byBytes, nullptr, doc);
  CHECK(doc.related.size() == 1 && doc.related[0].album == 1 && doc.related[0].note == "because it follows");
  CHECK(doc.lines.size() > 8);
  CHECK(doc.lines[0].style == Style::Heading);
  CHECK(std::strcmp(doc.text.c_str() + doc.lines[0].start, "Why it matters") == 0);
  CHECK(std::strcmp(doc.text.c_str() + doc.lines[1].start, "one two three four") == 0);
  CHECK(std::strcmp(doc.text.c_str() + doc.lines[2].start, "five six seven") == 0);
  int headings = 0;
  bool sawTrack = false;
  for (const Line& line : doc.lines) {
    const char* text = doc.text.c_str() + line.start;
    CHECK(line.length <= 20 || line.style == Style::Heading);
    CHECK(std::strlen(text) == line.length);
    if (line.style == Style::Heading) ++headings;
    if (std::strcmp(text, "- A Song. its note") == 0) sawTrack = true;
  }
  CHECK(headings == 4);  // why, listen (once for both), tracks, sources
  CHECK(sawTrack);
  layout(record, std::strlen(record), true, 20, byBytes, nullptr, doc);
  CHECK(std::strncmp(doc.text.c_str() + doc.lines[0].start, "Por qu", 6) == 0);

  // Marks and the recent list survive being written and read.
  Kept kept;
  kept.setMark("a1", Mark::Favourite);
  kept.setMark("b2", Mark::Want);
  kept.setMark("b2", Mark::None);
  kept.opened("a1");
  kept.opened("b2");
  kept.opened("a1");
  kept.spanish = 1;
  Kept back;
  back.unpack(kept.pack());
  CHECK(back.markOf("a1") == Mark::Favourite && back.markOf("b2") == Mark::None);
  CHECK(back.recent.size() == 2 && back.recent[0] == "a1" && back.recent[1] == "b2");
  CHECK(back.spanish == 1);
  for (int i = 0; i < 40; ++i) kept.opened(("x" + std::to_string(i)).c_str());
  CHECK(kept.recent.size() == kRecentMax);

  if (failures == 0) std::printf("linernotes: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
