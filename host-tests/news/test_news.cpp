// The JSON here is written by hand to the shape Kagi's API was seen to return
// on 2026-11-19 (key names and nesting read off live responses; the text is
// invented). It is NOT a capture: the build machine cannot reach the API, and
// the stories are Kagi's to publish, not this repository's.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../src/apps_local/news/NewsCore.h"

static int failures = 0;
#define CHECK(cond)                                               \
  do {                                                            \
    if (!(cond)) {                                                \
      std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                 \
    }                                                             \
  } while (0)

static const char* kCategories =
    "{\"batchId\":\"b-1\",\"createdAt\":\"2026-11-18T12:00:13.366Z\",\"hasOnThisDay\":true,\"categories\":["
    "{\"id\":\"u-world\",\"categoryId\":\"world\",\"categoryName\":\"World\",\"sourceLanguage\":\"en\","
    "\"timestamp\":1794996338,\"readCount\":3,\"clusterCount\":12},"
    "{\"id\":\"u-pizza\",\"categoryId\":\"pizza\",\"categoryName\":\"Pizza\",\"clusterCount\":2},"
    "{\"id\":\"u-ai\",\"categoryId\":\"ai\",\"categoryName\":\"AI\",\"clusterCount\":12}]}";

static const char* kStories =
    "{\"batchId\":\"b-1\",\"categoryId\":\"u-world\",\"categoryName\":\"World\",\"stories\":[\n"
    "{\"id\":\"s-1\",\"cluster_number\":1,\"unique_domains\":63,\"number_of_titles\":80,\"sourceLanguage\":\"en\","
    "\"category\":\"Diplomacy\",\"title\":\"Leaders meet \\u2014 a \\\"first\\\" summit\","
    "\"short_summary\":\"First paragraph [a.com#1][b.org#12].\\nSecond one, with Erdo\xC4\x9F"
    "an and \\ud83c\\udf10 a"
    " globe [not a cite].\",\"did_you_know\":\"Skipped.\",\"talking_points\":[\"Point one [a.com#2].\",\"Point two.\"],"
    "\"quote\":\"We will see\",\"quote_author\":\"A minister\",\"location\":\"Geneva, Switzerland\","
    "\"perspectives\":[{\"text\":\"One side says yes.\",\"sources\":[{\"name\":\"A\",\"url\":\"https://a.com/x\"}]},"
    "{\"text\":\"The other says no.\",\"sources\":[]}],\"emoji\":\"\\ud83c\\udf10\","
    "\"historical_background\":\"It began in 2019.\",\"international_reactions\":[],"
    "\"timeline\":[{\"date\":\"2024\",\"content\":\"Talks opened.\",\"date_iso\":\"2024\"},"
    "{\"date\":\"Nov. 17\",\"content\":\"They resumed.\"}],\"suggested_qna\":[{\"question\":\"Q\",\"answer\":\"A\"}],"
    "\"primary_image\":{\"url\":\"https://img.example/1.jpg\",\"caption\":\"A caption\",\"credit\":\"X\"},"
    "\"articles\":[{\"title\":\"Outlet one headline\",\"link\":\"https://a.com/1\",\"domain\":\"a.com\","
    "\"date\":\"2026-11-17T00:39:59.000Z\",\"image\":\"\",\"image_caption\":\"\"},"
    "{\"title\":\"Outlet two headline\",\"link\":\"https://b.org/2\",\"domain\":\"b.org\"}],"
    "\"domains\":[{\"name\":\"a.com\",\"favicon\":\"/f?d=a.com\",\"isPaywalled\":true}],"
    "\"main_story\":{\"title\":\"Lead\",\"url\":\"/business/"
    "x\",\"cluster_id\":\"s-lead\",\"category_id\":\"business\"},"
    "\"other_angles\":[{\"title\":\"Angle\",\"cluster_id\":\"s-angle\",\"category_id\":\"ai\"}],"
    "\"selectedLanguage\":\"en\",\"translationAvailable\":false},\n"
    "{\"id\":\"s-2\",\"unique_domains\":14,\"sourceLanguage\":\"es\",\"category\":\"Econom\xC3\xAD"
    "a\","
    "\"title\":\"Un titular\",\"short_summary\":\"Resumen.\",\"talking_points\":[],\"perspectives\":[],"
    "\"industry_impact\":[\"Sube el coste.\"],\"future_outlook\":\"Se vota el lunes.\",\"articles\":[]},\n"
    "{\"id\":\"s-3\",\"title\":\"No summary\"}\n"
    "],\"totalStories\":3,\"timestamp\":{},\"readCount\":726,\"domains\":[{\"name\":\"a.com\"}]}";

struct Collect final : news::StoriesReader::Sink {
  std::vector<news::Story> stories;
  size_t stopAfter = 99;
  bool story(news::Story& s) override {
    stories.push_back(s);
    return stories.size() < stopAfter;
  }
};

static void scan(const char* json, news::JsonHandler& handler, const size_t chunk) {
  news::JsonStream stream(handler);
  const size_t n = std::strlen(json);
  for (size_t at = 0; at < n; at += chunk) {
    if (!stream.feed(json + at, at + chunk > n ? n - at : chunk)) break;
  }
  CHECK(!stream.failed());
}

int main() {
  CHECK(news::kSectionCount == 4);
  CHECK(news::storiesUrl("abc") == "https://news.kagi.com/api/batches/latest/categories/abc/stories?limit=10");

  CHECK(news::cleanText("A fact [france24.com#1][bbc.co.uk#22]. Next [sic] one.") == "A fact. Next [sic] one.");
  CHECK(news::cleanText("  two   spaces\nand a line  ") == "two spaces\n\nand a line");
  CHECK(news::cleanText("Erdo\xC4\x9F"
                        "an, \xC5\xA0"
                        "ef\xC4\x8D"
                        "ovi\xC4\x8D, caf\xC3\xA9 \xF0\x9F\x8C\x90!") == "Erdogan, Sefcovic, caf\xC3\xA9 !");

  CHECK(news::parseDate("2026-11-18T20:15:00Z") == 1795032900u);
  CHECK(news::parseDate("2026-11-18T12:00:13.366Z") == 1795003213u);
  CHECK(news::parseDate("yesterday") == 0);
  char label[16];
  news::editionLabel(1795003213u, label, sizeof(label));
  CHECK(std::strcmp(label, "18 NOV") == 0);
  news::editionLabel(951782400u, label, sizeof(label));  // 2000-02-29
  CHECK(std::strcmp(label, "29 FEB") == 0);
  news::editionLabel(0, label, sizeof(label));
  CHECK(label[0] == '\0');

  // The same answer however the socket cuts the document.
  for (const size_t chunk : {size_t{1}, size_t{5}, size_t{64}, size_t{100000}}) {
    news::CategoriesReader categories;
    scan(kCategories, categories, chunk);
    CHECK(categories.batchId == "b-1");
    CHECK(news::parseDate(categories.createdAt) == 1795003213u);
    CHECK(categories.categories.size() == 2);
    if (categories.categories.size() == 2) {
      CHECK(categories.categories[0].slug == "world" && categories.categories[0].uuid == "u-world");
      CHECK(categories.categories[1].slug == "ai" && categories.categories[1].uuid == "u-ai");
    }

    Collect got;
    news::StoriesReader reader(got);
    scan(kStories, reader, chunk);
    CHECK(reader.seen() == 3);
    CHECK(got.stories.size() == 2);  // the one with no summary is not a story
    if (got.stories.size() != 2) continue;
    const news::Story& a = got.stories[0];
    CHECK(a.id == "s-1");
    CHECK(a.title == "Leaders meet -- a \"first\" summit");
    CHECK(a.summary == "First paragraph.\n\nSecond one, with Erdogan and a globe [not a cite].");
    CHECK(a.category == "Diplomacy" && a.location == "Geneva, Switzerland");
    CHECK(a.outlets == 63 && a.language == "en");
    CHECK(a.points.size() == 2 && a.points[0] == "Point one.");
    CHECK(a.perspectives.size() == 2 && a.perspectives[1] == "The other says no.");
    CHECK(a.timeline.size() == 2 && a.timeline[0] == "2024: Talks opened." &&
          a.timeline[1] == "Nov. 17: They resumed.");
    CHECK(a.sources.size() == 2 && a.sources[1].domain == "b.org" && a.sources[0].title == "Outlet one headline");
    CHECK(a.mainId == "s-lead");
    CHECK(a.angleIds.size() == 1 && a.angleIds[0] == "s-angle");
    CHECK(a.quote == "We will see" && a.quoteAuthor == "A minister");

    const std::string doc = news::buildDocument(a);
    CHECK(doc.find("Leaders meet") == 0);
    CHECK(doc.find("DIPLOMACY  63 OUTLETS  Geneva, Switzerland") != std::string::npos);
    CHECK(doc.find("KEY POINTS\n\n- Point one.\n\n- Point two.") != std::string::npos);
    CHECK(doc.find("PERSPECTIVES\n\n- One side says yes.") != std::string::npos);
    CHECK(doc.find("CONTEXT\n\nIt began in 2019.") != std::string::npos);
    CHECK(doc.find("TIMELINE\n\n- 2024: Talks opened.") != std::string::npos);
    CHECK(doc.find("ORIGINAL SOURCES\n\n- a.com: Outlet one headline") != std::string::npos);
    CHECK(doc.find("63 outlets in all.") != std::string::npos);
    CHECK(doc.find("Kagi News") != std::string::npos && doc.find("CC BY-NC 4.0") != std::string::npos);
    // Nothing that was empty gets a heading, and nothing that was skipped leaks in.
    CHECK(doc.find("WHAT COMES NEXT") == std::string::npos);
    CHECK(doc.find("Skipped") == std::string::npos && doc.find("caption") == std::string::npos);
    CHECK(doc.find("#") == std::string::npos);

    const news::Story& b = got.stories[1];
    CHECK(b.language == "es");
    CHECK(news::kickerOf(b) ==
          "ECONOM\xC3\x8D"
          "A  14 MEDIOS");
    const std::string es = news::buildDocument(b);
    CHECK(es.find("CONTEXTO\n\n- Sube el coste.") != std::string::npos);
    CHECK(es.find("QU\xC3\x89 VIENE AHORA\n\nSe vota el lunes.") != std::string::npos);
    CHECK(es.find("CLAVES") == std::string::npos && es.find("PERSPECTIVAS") == std::string::npos);
    CHECK(es.find("FUENTES") == std::string::npos && es.find("CRONOLOG") == std::string::npos);
    CHECK(es.find("Licencia CC BY-NC 4.0") != std::string::npos);
  }

  // The sink can stop the transfer after the story it wanted.
  {
    Collect one;
    one.stopAfter = 1;
    news::StoriesReader reader(one);
    news::JsonStream stream(reader);
    CHECK(!stream.feed(kStories, std::strlen(kStories)));
    CHECK(stream.stopped() && !stream.failed());
    CHECK(one.stories.size() == 1);
  }

  // Not JSON at all (an error page) fails rather than inventing stories.
  {
    Collect none;
    news::StoriesReader reader(none);
    news::JsonStream stream(reader);
    const char* html = "<html><body>502 Bad Gateway</body></html>";
    stream.feed(html, std::strlen(html));
    CHECK(none.stories.empty());
  }

  // One event is told once: by its own id, by the lead it points at, by a
  // shared angle, or by the same headline.
  {
    news::Deduper once;
    news::Story lead;
    lead.id = "L";
    lead.title = "Chipmaker posts record quarter";
    lead.angleIds = {"A1", "A2"};
    CHECK(once.accept(lead));
    CHECK(!once.accept(lead));
    news::Story angle;
    angle.id = "A1";
    angle.title = "Another headline";
    angle.mainId = "L";
    CHECK(!once.accept(angle));
    news::Story sibling;
    sibling.id = "Z";
    sibling.title = "Third headline";
    sibling.angleIds = {"A2"};
    CHECK(!once.accept(sibling));
    news::Story same;
    same.id = "Q";
    same.title = "chipmaker posts RECORD quarter!";
    CHECK(!once.accept(same));
    news::Story other;
    other.id = "N";
    other.title = "Something else entirely";
    CHECK(once.accept(other));
  }

  {
    std::vector<news::Item> items(2);
    items[0].id = "s-1";
    items[0].title = "A title";
    items[0].kicker = "DIPLOMACY  63 OUTLETS";
    items[0].summary = "Lead\ttext\nhere";
    items[1].id = "s-2";
    items[1].title = "Another";
    const std::string packed = news::packSection(items, 1795003213u);
    std::vector<news::Item> back;
    uint32_t edition = 0;
    CHECK(news::unpackSection(packed, back, edition));
    CHECK(edition == 1795003213u && back.size() == 2);
    if (back.size() == 2) {
      CHECK(back[0].id == "s-1" && back[0].title == "A title" && back[0].kicker == "DIPLOMACY  63 OUTLETS");
      CHECK(back[0].summary == "Lead text here");
      CHECK(back[1].id == "s-2" && back[1].kicker.empty() && back[1].summary.empty());
    }
    CHECK(!news::unpackSection("garbage", back, edition));
    CHECK(!news::unpackSection("NEWS\t1\t5\nold format\n", back, edition));
  }

  CHECK(news::articleFile("s-1") != news::articleFile("s-2"));
  CHECK(news::articleFile("") == "811c9dc5.txt");

  if (failures == 0) std::printf("news: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
