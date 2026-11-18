// Fixtures here are written by hand to the RSS 2.0 and Atom specifications.
// They are NOT captures of any outlet's feed: the build machine could not reach
// one. What they pin is the scanner's reading of the formats.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/news/NewsCore.h"

static int failures = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    if (!(cond)) {                                                     \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);      \
      ++failures;                                                      \
    }                                                                  \
  } while (0)

static const char* kRss =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<rss version=\"2.0\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:media=\"http://search.yahoo.com/mrss/\">\n"
    "<channel><title>Example World</title><link>https://example.org/</link>\n"
    "<image><title>Logo</title><link>https://example.org/logo</link></image>\n"
    "<!-- a comment with <item> in it -->\n"
    "<item><title><![CDATA[Talks resume & the \xE2\x80\x9Cquiet\xE2\x80\x9D deal]]></title>\n"
    "<link>https://example.org/a?x=1&amp;y=2</link>\n"
    "<description><![CDATA[<p>First <b>paragraph</b>.</p><img src=\"x.jpg\"/> Second&nbsp;one.]]></description>\n"
    "<media:title>Not the title</media:title>\n"
    "<pubDate>Wed, 18 Nov 2026 20:15:00 GMT</pubDate></item>\n"
    "<item><title>Markets &amp; rates</title><link>\n  https://example.org/b\n</link>\n"
    "<description>&lt;p&gt;Escaped &amp;amp; nested&lt;/p&gt;</description>\n"
    "<dc:date>2026-11-18T09:00:00+01:00</dc:date></item>\n"
    "<item><title>No link</title></item>\n"
    "<item><title>Third</title><link>https://example.org/c</link><guid>g</guid></item>\n"
    "</channel></rss>\n";

static const char* kAtom =
    "<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>Example Atom</title>\n"
    "<link href=\"https://example.net/\"/>\n"
    "<entry><title type=\"html\">An &lt;em&gt;entry&lt;/em&gt;</title>\n"
    "<link rel=\"self\" href=\"https://example.net/self\"/>\n"
    "<link rel=\"alternate\" type=\"text/html\" href=\"https://example.net/one?a=1&amp;b=2\"/>\n"
    "<updated>2026-11-17T08:00:00Z</updated><published>2026-11-16T08:00:00Z</published>\n"
    "<author><name>Someone</name><title>Not this</title></author>\n"
    "<content type=\"html\">&lt;p&gt;Body text here.&lt;/p&gt;</content></entry>\n"
    "<entry><title>Two</title><link href='https://example.net/two'/><summary>Short.</summary>\n"
    "<content type=\"html\">Long body that must not replace the summary.</content>\n"
    "<updated>2026-11-18T08:00:00Z</updated></entry></feed>\n";

static void scan(const char* xml, std::vector<news::Item>& out, const size_t chunk, const int limit = news::kPerFeed) {
  news::FeedScanner scanner(out, 2, limit);
  const size_t n = std::strlen(xml);
  for (size_t at = 0; at < n; at += chunk) {
    if (!scanner.feed(xml + at, at + chunk > n ? n - at : chunk)) break;
  }
}

int main() {
  CHECK(news::parseDate("Wed, 18 Nov 2026 20:15:00 GMT") == 1795032900u);
  CHECK(news::parseDate("18 Nov 2026 21:15:00 +0100") == 1795032900u);
  CHECK(news::parseDate("2026-11-18T20:15:00Z") == 1795032900u);
  CHECK(news::parseDate("2026-11-18T21:15:00.123+01:00") == 1795032900u);
  CHECK(news::parseDate("2026-11-18T15:15:00-05:00") == 1795032900u);
  CHECK(news::parseDate("yesterday") == 0);
  CHECK(news::parseDate("") == 0);

  CHECK(news::plainText("<p>One</p>\n<p>two &amp; three&nbsp;four</p>", 100) == "One two & three four");
  CHECK(news::plainText("caf&eacute; &#233; &#xE9;", 100) == "caf\xC3\xA9 \xC3\xA9 \xC3\xA9");
  CHECK(news::plainText("alpha beta gamma delta", 12) == "alpha beta...");

  char age[8];
  news::ageLabel(1795032900u - 120, 1795032900u, age, sizeof(age));
  CHECK(std::strcmp(age, "2m") == 0);
  news::ageLabel(1795032900u - 7200, 1795032900u, age, sizeof(age));
  CHECK(std::strcmp(age, "2h") == 0);
  news::ageLabel(1795032900u - 3 * 86400, 1795032900u, age, sizeof(age));
  CHECK(std::strcmp(age, "3d") == 0);
  news::ageLabel(1795032900u, 1000, age, sizeof(age));
  CHECK(age[0] == '\0');

  // The same answer however the socket cuts the document.
  for (const size_t chunk : {size_t{1}, size_t{7}, size_t{64}, size_t{100000}}) {
    std::vector<news::Item> rss;
    scan(kRss, rss, chunk);
    CHECK(rss.size() == 3);
    if (rss.size() == 3) {
      CHECK(rss[0].title == "Talks resume & the \"quiet\" deal");
      CHECK(rss[0].url == "https://example.org/a?x=1&y=2");
      CHECK(rss[0].summary == "First paragraph. Second one.");
      CHECK(rss[0].when == 1795032900u);
      CHECK(rss[0].feed == 2);
      CHECK(rss[1].title == "Markets & rates");
      CHECK(rss[1].url == "https://example.org/b");
      CHECK(rss[1].summary == "Escaped & nested");
      CHECK(rss[1].when == news::parseDate("2026-11-18T08:00:00Z"));
      CHECK(rss[2].title == "Third");
      CHECK(rss[2].when == 0);
    }
    std::vector<news::Item> atom;
    scan(kAtom, atom, chunk);
    CHECK(atom.size() == 2);
    if (atom.size() == 2) {
      CHECK(atom[0].title == "An entry");
      CHECK(atom[0].url == "https://example.net/one?a=1&b=2");
      CHECK(atom[0].when == news::parseDate("2026-11-16T08:00:00Z"));
      CHECK(atom[0].summary == "Body text here.");
      CHECK(atom[1].url == "https://example.net/two");
      CHECK(atom[1].summary == "Short.");
    }
  }

  // The limit stops the transfer.
  {
    std::vector<news::Item> one;
    news::FeedScanner scanner(one, 0, 1);
    CHECK(!scanner.feed(kRss, std::strlen(kRss)));
    CHECK(scanner.full());
    CHECK(one.size() == 1);
  }

  // Merging: newest first, undated last, one row per address.
  {
    std::vector<news::Item> all;
    scan(kRss, all, 4096);
    scan(kAtom, all, 4096);
    scan(kAtom, all, 4096);
    news::mergeSection(all);
    CHECK(all.size() == 5);
    CHECK(all.front().url == "https://example.org/a?x=1&y=2");
    CHECK(all.back().when == 0);

    const std::string packed = news::packSection(all, 1795040000u);
    std::vector<news::Item> back;
    uint32_t fetched = 0;
    CHECK(news::unpackSection(packed, back, fetched));
    CHECK(fetched == 1795040000u);
    CHECK(back.size() == all.size());
    for (size_t i = 0; i < back.size() && i < all.size(); ++i) {
      CHECK(back[i].title == all[i].title);
      CHECK(back[i].url == all[i].url);
      CHECK(back[i].summary == all[i].summary);
      CHECK(back[i].when == all[i].when);
      CHECK(back[i].feed == all[i].feed);
    }
    CHECK(!news::unpackSection("garbage", back, fetched));
  }

  CHECK(news::articleFile("https://example.org/a") == news::articleFile("https://example.org/a"));
  CHECK(news::articleFile("https://example.org/a") != news::articleFile("https://example.org/b"));
  CHECK(news::articleFile("") == "811c9dc5.txt");

  CHECK(news::kSectionCount == 5);
  for (int s = 0; s < news::kSectionCount; ++s) {
    CHECK(news::kSections[s].feedCount > 0);
    for (int f = 0; f < news::kSections[s].feedCount; ++f) {
      CHECK(std::strncmp(news::kSections[s].feeds[f].url, "https://", 8) == 0);
    }
  }

  if (failures == 0) std::printf("news: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
