#include <cstdio>
#include <cstring>
#include <string>

#include "DiscoverCore.h"

using namespace discover;

namespace {
int failures = 0;
int checks = 0;
void check(const bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL: %s\n", what);
  }
}
}  // namespace

int main() {
  {  // the search
    char url[400];
    Filter f;
    f.age = Age::Week;
    // 17 November 2026 is day 20774.
    searchUrl(f, 20774, 1, url, sizeof(url));
    check(std::strcmp(url,
                      "https://api.github.com/search/repositories?q=stars:%3E=50+created:%3E=2026-11-10"
                      "&sort=stars&order=desc&per_page=20") == 0,
          url);
    f.topic = Topic::Synths;
    f.language = Language::Cpp;
    f.stars = Stars::Thousand;
    f.age = Age::AnyActive;
    searchUrl(f, 20774, 1, url, sizeof(url));
    check(std::strcmp(url,
                      "https://api.github.com/search/repositories?q=synthesizer+OR+eurorack+OR+midi+OR+synth+OR+dsp"
                      "+in:name,description,topics+language:cpp+stars:%3E=1000+pushed:%3E=2026-10-18"
                      "&sort=stars&order=desc&per_page=20") == 0,
          url);
    for (int t = 0; t < kTopics; ++t) {
      f.topic = static_cast<Topic>(t);
      f.language = Language::TypeScript;
      searchUrl(f, 20774, 1, url, sizeof(url));
      check(std::strlen(url) < sizeof(url) - 1 && std::strstr(url, "per_page=20") != nullptr, "every search fits");
      int ors = 0;
      for (const char* p = url; (p = std::strstr(p, "+OR+")) != nullptr; ++p) ++ors;
      check(ors <= 5, "no more ORs than GitHub takes");
    }
    int y = 0, m = 0, d = 0;
    civilOfDay(20774, y, m, d);
    check(y == 2026 && m == 11 && d == 17, "a day as a date");
    civilOfDay(0, y, m, d);
    check(y == 1970 && m == 1 && d == 1, "the first day");
    readmeUrl("davozola/crossplay", url, sizeof(url));
    check(std::strcmp(url, "https://raw.githubusercontent.com/davozola/crossplay/HEAD/README.md") == 0, url);

    std::string bytes = packFilter(f);
    Filter back;
    check(unpackFilter(bytes, back) && back.topic == f.topic && back.age == Age::AnyActive, "the filter round trips");
    bytes[1] = static_cast<char>(200);
    check(!unpackFilter(bytes, back), "a language that is not one");
    const char old[4] = {2, 1, 1, 0};
    check(unpackFilter(std::string_view(old, 4), back) && back.topic == Topic::Firmware && back.age == Age::Week &&
              back.skipSeen && back.query[0] == 0,
          "a filter from before the query still reads");
    check(!unpackFilter(std::string_view(old, 3), back), "three bytes are no filter");

    // rising, the reader's own words, and the pages after the first
    Filter rising;
    check(rising.age == Age::Rising && skipsSeen(rising), "rising is where it starts, and skips what was seen");
    searchUrl(rising, 20774, 1, url, sizeof(url));
    check(std::strcmp(url,
                      "https://api.github.com/search/repositories?q=stars:%3E=50+created:%3E=2026-09-18"
                      "+pushed:%3E=2026-11-10&sort=stars&order=desc&per_page=20") == 0,
          url);
    searchUrl(rising, 20774, 3, url, sizeof(url));
    check(std::strstr(url, "per_page=20&page=3") != nullptr, url);
    Filter own;
    own.topic = Topic::Own;
    own.stars = Stars::Any;
    own.age = Age::Ever;
    tidyQuery("  e-ink   display c++ user:ada \xC3\xA9 ", own.query, sizeof(own.query));
    check(std::strcmp(own.query, "e-ink display c++ user:ada") == 0, own.query);
    searchUrl(own, 20774, 1, url, sizeof(url));
    check(std::strcmp(url,
                      "https://api.github.com/search/repositories?q=e-ink+display+c%2B%2B+user%3Aada+stars:%3E=0"
                      "&sort=stars&order=desc&per_page=20") == 0,
          url);
    check(!skipsSeen(own), "a search of any age shows everything every time");
    Filter read;
    check(unpackFilter(packFilter(own), read) && read.topic == Topic::Own && read.stars == Stars::Any &&
              read.age == Age::Ever && std::strcmp(read.query, own.query) == 0,
          "the reader's words round trip");
    own.query[0] = 0;
    check(unpackFilter(packFilter(own), read) && read.topic == Topic::Everything, "no words is everything");
    char longQuery[200];
    std::memset(longQuery, 'a', sizeof(longQuery) - 1);
    longQuery[sizeof(longQuery) - 1] = 0;
    tidyQuery(longQuery, own.query, sizeof(own.query));
    check(std::strlen(own.query) == kQueryCap - 1, "a long query is cut to fit");
    bool everyStar[kStarChoices] = {};
    bool everyAge[kAges] = {};
    for (int i = 0; i < kStarChoices; ++i) everyStar[static_cast<int>(starsAt(i))] = true;
    for (int i = 0; i < kAges; ++i) everyAge[static_cast<int>(ageAt(i))] = true;
    bool all = true;
    for (const bool b : everyStar) all = all && b;
    for (const bool b : everyAge) all = all && b;
    check(all && ageAt(0) == Age::Rising && starsAt(0) == Stars::Any, "the pickers list every choice once");
    pageUrl("ada/tinysynth", url, sizeof(url));
    check(std::strcmp(url, "https://github.com/ada/tinysynth") == 0, url);
    check(dayOfDate("2026-11-17") == 20774 && dayOfDate("1970-01-01") == 0 && dayOfDate("soon") == -1,
          "a date as a day");
    check(std::strcmp(topicName(Topic::Firmware), "FIRMWARE") == 0 &&
              std::strcmp(languageName(Language::Cpp), "C++") == 0 &&
              std::strcmp(starsName(Stars::Ten), "10+ STARS") == 0 && starsFloor(Stars::TwoHundred) == 200 &&
              std::strcmp(ageName(Age::Month), "NEW THIS MONTH") == 0,
          "names");
  }

  {  // the answer
    const std::string json =
        "{\"total_count\": 1234, \"incomplete_results\": false, \"items\": ["
        "{\"id\": 1, \"name\": \"tinysynth\", \"full_name\": \"ada/tinysynth\", \"owner\": {\"login\": \"ada\"},"
        " \"description\": \"A tiny \\u201cwavetable\\u201d synth \\ud83c\\udfb9 for the RP2040 \\u2014 caf\\u00e9 "
        "ready\","
        " \"created_at\": \"2026-11-12T08:00:00Z\", \"pushed_at\": \"2026-11-16T21:10:00Z\","
        " \"homepage\": \"https://ada.example/tinysynth\", \"stargazers_count\": 1840, \"language\": \"C++\","
        " \"forks_count\": 73, \"open_issues_count\": 12, \"license\": {\"key\": \"mit\", \"spdx_id\": \"MIT\"},"
        " \"topics\": [\"synthesizer\", \"rp2040\", \"audio\"]},"
        "{\"id\": 2, \"full_name\": \"bob/nothing\", \"description\": null, \"created_at\": \"2026-11-11T00:00:00Z\","
        " \"pushed_at\": \"2026-11-11T00:00:00Z\", \"homepage\": null, \"stargazers_count\": 61, \"language\": null,"
        " \"forks_count\": 0, \"open_issues_count\": 0, \"license\": null, \"topics\": []},"
        "{\"id\": 3, \"full_name\": \"no-slash\"}"
        "]}";
    Repo repos[8];
    int32_t total = 0;
    const int n = parseSearch(json.data(), json.size(), repos, 8, total);
    check(n == 2 && total == 1234, "two repositories of many");
    check(std::strcmp(repos[0].fullName, "ada/tinysynth") == 0 && std::strcmp(nameOf(repos[0]), "tinysynth") == 0,
          "names");
    check(std::strcmp(repos[0].description, "A tiny \"wavetable\" synth for the RP2040 - caf\xC3\xA9 ready") == 0,
          repos[0].description);
    check(repos[0].stars == 1840 && repos[0].forks == 73 && repos[0].issues == 12, "numbers");
    check(std::strcmp(repos[0].language, "C++") == 0 && std::strcmp(repos[0].license, "MIT") == 0, "language, licence");
    check(std::strcmp(repos[0].created, "2026-11-12") == 0 && std::strcmp(repos[0].pushed, "2026-11-16") == 0,
          "dates cut to the day");
    check(std::strcmp(repos[0].topics, "synthesizer, rp2040, audio") == 0, repos[0].topics);
    check(repos[1].description[0] == 0 && repos[1].language[0] == 0 && repos[1].license[0] == 0 && repos[1].stars == 61,
          "nulls are empty");
    check(parseSearch(json.data(), json.size(), repos, 1, total) == 1, "no more than asked for");
    const char* limited = "{\"message\": \"API rate limit exceeded\"}";
    check(parseSearch(limited, std::strlen(limited), repos, 8, total) == -1, "an error is not an answer");

    char count[16];
    formatCount(812, count, sizeof(count));
    check(std::strcmp(count, "812") == 0, count);
    formatCount(1840, count, sizeof(count));
    check(std::strcmp(count, "1.8K") == 0, count);
    formatCount(234567, count, sizeof(count));
    check(std::strcmp(count, "234K") == 0, count);

    // keeping
    Repo saved[kMaxSaved];
    int kept = toggleSaved(saved, 0, repos[0]);
    kept = toggleSaved(saved, kept, repos[1]);
    check(kept == 2 && findSaved(saved, kept, "bob/nothing") == 1 && findSaved(saved, kept, "x/y") == -1, "kept");
    const std::string packed = packSaved(saved, kept);
    Repo back[kMaxSaved];
    const int read = unpackSaved(packed, back, kMaxSaved);
    check(read == 2 && std::strcmp(back[0].fullName, "ada/tinysynth") == 0 && back[0].stars == 1840 &&
              back[0].issues == 12 && std::strcmp(back[0].topics, "synthesizer, rp2040, audio") == 0 &&
              std::strcmp(back[0].description, repos[0].description) == 0 && back[1].description[0] == 0 &&
              back[1].stars == 61,
          "the kept list round trips");
    kept = toggleSaved(saved, kept, repos[0]);
    check(kept == 1 && std::strcmp(saved[0].fullName, "bob/nothing") == 0, "a second toggle lets go");
    for (int i = 0; i < kMaxSaved + 3; ++i) {
      Repo r;
      std::snprintf(r.fullName, sizeof(r.fullName), "o/r%d", i);
      kept = toggleSaved(saved, kept, r);
    }
    check(kept == kMaxSaved && findSaved(saved, kept, "bob/nothing") == -1 && findSaved(saved, kept, "o/r26") >= 0,
          "the oldest make room");
    check(unpackSaved("garbage with no tabs\n", back, kMaxSaved) == 0, "a broken line is no repository");
  }

  {  // what the face can set
    char a[64] = "  \xF0\x9F\x9A\x80 Rocket  \xE4\xB8\xAD\xE6\x96\x87  fast\xE2\x80\xA6 ";
    latinOnly(a);
    check(std::strcmp(a, "Rocket fast...") == 0, a);
    char b[32] = "ni\xC3\xB1o\tline\nbreak";
    latinOnly(b);
    check(std::strcmp(b, "ni\xC3\xB1o line break") == 0, b);
  }

  if (failures == 0) {
    {  // pace, what was seen, the reader's searches
      Repo list[4];
      std::snprintf(list[0].fullName, sizeof(list[0].fullName), "old/giant");
      std::snprintf(list[0].created, sizeof(list[0].created), "2026-09-20");
      list[0].stars = 5000;
      std::snprintf(list[1].fullName, sizeof(list[1].fullName), "new/Spark");
      std::snprintf(list[1].created, sizeof(list[1].created), "2026-11-15");
      list[1].stars = 900;
      std::snprintf(list[2].fullName, sizeof(list[2].fullName), "mid/steady");
      std::snprintf(list[2].created, sizeof(list[2].created), "2026-11-07");
      list[2].stars = 1000;
      std::snprintf(list[3].fullName, sizeof(list[3].fullName), "today/born");
      std::snprintf(list[3].created, sizeof(list[3].created), "2026-11-17");
      list[3].stars = 60;
      check(starsPerDay(list[0], 20774) == 5000 / 58 && starsPerDay(list[1], 20774) == 450 &&
                starsPerDay(list[3], 20774) == 60,
            "stars a day");
      rankByPace(list, 4, 20774);
      check(std::strcmp(list[0].fullName, "new/Spark") == 0 && std::strcmp(list[1].fullName, "mid/steady") == 0 &&
                std::strcmp(list[2].fullName, "old/giant") == 0 && std::strcmp(list[3].fullName, "today/born") == 0,
            "the fastest first");

      static Seen seen;
      check(markSeen(seen, "new/spark") && !markSeen(seen, "NEW/Spark") && hasSeen(seen, "New/Spark") &&
                !hasSeen(seen, "old/giant") && seen.count == 1,
            "seen, whatever the case");
      check(dropSeen(list, 4, seen) == 3 && std::strcmp(list[0].fullName, "mid/steady") == 0 &&
                std::strcmp(list[2].fullName, "today/born") == 0,
            "the seen ones close up");
      for (int i = 0; i < kMaxSeen + 5; ++i) {
        char name[24];
        std::snprintf(name, sizeof(name), "o/r%d", i);
        markSeen(seen, name);
      }
      check(seen.count == kMaxSeen && !hasSeen(seen, "new/spark") && !hasSeen(seen, "o/r3") && hasSeen(seen, "o/r604"),
            "the oldest seen make room");

      Terms terms;
      rememberTerm(terms, "plex");
      rememberTerm(terms, " e-ink  display ");
      rememberTerm(terms, "PLEX");
      rememberTerm(terms, "   ");
      check(terms.count == 2 && std::strcmp(terms.items[0], "PLEX") == 0 &&
                std::strcmp(terms.items[1], "e-ink display") == 0,
            "a search typed again comes to the front");
      for (int i = 0; i < kMaxTerms + 2; ++i) {
        char term[16];
        std::snprintf(term, sizeof(term), "term%d", i);
        rememberTerm(terms, term);
      }
      check(terms.count == kMaxTerms && std::strcmp(terms.items[0], "term7") == 0 &&
                std::strcmp(terms.items[kMaxTerms - 1], "term2") == 0,
            "the oldest searches go");
      Terms back;
      unpackTerms(packTerms(terms), back);
      check(back.count == kMaxTerms && std::strcmp(back.items[3], terms.items[3]) == 0, "the searches round trip");
      forgetTerm(back, 0);
      forgetTerm(back, 40);
      check(back.count == kMaxTerms - 1 && std::strcmp(back.items[0], "term6") == 0, "a search forgotten");
    }

    {  // releases
      char name[72];
      check(tidyFullName(" https://github.com/Ada/TinySynth/releases/tag/v2 ", name, sizeof(name)) &&
                std::strcmp(name, "Ada/TinySynth") == 0,
            name);
      check(tidyFullName("ada/tinysynth.git", name, sizeof(name)) && std::strcmp(name, "ada/tinysynth") == 0, name);
      check(!tidyFullName("tinysynth", name, sizeof(name)) && !tidyFullName("ada/", name, sizeof(name)) &&
                !tidyFullName("/x", name, sizeof(name)) && !tidyFullName("a da/x?y", name, sizeof(name)),
            "what is not a repository");

      const std::string json =
          "{\"url\": \"https://api.github.com/repos/ada/tinysynth/releases/1\", \"author\": {\"login\": \"ada\"},"
          " \"tag_name\": \"v2.1.0\", \"name\": \"Wavefolder \\u2014 and a fix\", \"draft\": false,"
          " \"published_at\": \"2026-11-14T09:30:00Z\","
          " \"body\": \"## What's new\\r\\n\\r\\n- A wavefolder\\n- Fixes \\\"stuck notes\\\"\\n\"}";
      Watched fetched;
      std::string notes;
      check(parseRelease(json.data(), json.size(), fetched, notes), "a release");
      check(std::strcmp(fetched.tag, "v2.1.0") == 0 && std::strcmp(fetched.title, "Wavefolder - and a fix") == 0 &&
                std::strcmp(fetched.published, "2026-11-14") == 0,
            fetched.title);
      check(notes == "## What's new\n\n- A wavefolder\n- Fixes \"stuck notes\"\n", notes.c_str());
      const char* missing = "{\"message\": \"Not Found\", \"status\": \"404\"}";
      check(!parseRelease(missing, std::strlen(missing), fetched, notes) && std::strcmp(fetched.tag, "v2.1.0") == 0,
            "no release leaves what was there");
      const char* same =
          "{\"tag_name\": \"v3\", \"name\": \"v3\", \"published_at\": \"2026-11-16T00:00:00Z\", \"body\": null}";
      Watched plain;
      check(parseRelease(same, std::strlen(same), plain, notes) && plain.title[0] == 0 && notes.empty(),
            "a title that is only the tag is no title");
      const char* tags = "[{\"name\": \"1.6.5\", \"commit\": {\"sha\": \"abc\"}}, {\"name\": \"1.6.4\"}]";
      Watched tagged;
      check(parseTags(tags, std::strlen(tags), tagged) && std::strcmp(tagged.tag, "1.6.5") == 0 &&
                tagged.published[0] == 0,
            "the newest tag");
      check(!parseTags("[]", 2, tagged) && !parseTags(missing, std::strlen(missing), tagged), "no tags");

      Watched list[kMaxWatched];
      int count = toggleWatched(list, 0, "ada/tinysynth");
      count = toggleWatched(list, count, "bob/board");
      count = toggleWatched(list, count, "cy/tagsonly");
      check(count == 3 && findWatched(list, count, "BOB/board") == 1 && !isNew(list[0]), "watched");
      noteRelease(list[0], fetched, "2026-11-17");
      check(!isNew(list[0]) && std::strcmp(list[0].seenTag, "v2.1.0") == 0 &&
                std::strcmp(list[0].checked, "2026-11-17") == 0,
            "the first version found is where the reader starts");
      noteRelease(list[0], plain, "2026-11-18");
      check(isNew(list[0]) && std::strcmp(list[0].tag, "v3") == 0 && std::strcmp(list[0].seenTag, "v2.1.0") == 0,
            "a later one is news");
      Watched older;
      std::snprintf(older.tag, sizeof(older.tag), "0.9");
      std::snprintf(older.published, sizeof(older.published), "2026-11-17");
      noteRelease(list[1], older, "2026-11-18");
      noteRelease(list[2], tagged, "2026-11-18");
      sortWatched(list, count);
      check(std::strcmp(list[0].fullName, "ada/tinysynth") == 0 && std::strcmp(list[1].fullName, "bob/board") == 0 &&
                std::strcmp(list[2].fullName, "cy/tagsonly") == 0,
            "news first, then the latest");
      char url[160];
      releasePageUrl(list[0], url, sizeof(url));
      check(std::strcmp(url, "https://github.com/ada/tinysynth/releases/tag/v3") == 0, url);
      releasePageUrl(list[2], url, sizeof(url));
      check(std::strcmp(url, "https://github.com/cy/tagsonly/tags") == 0, url);
      releaseUrl("ada/tinysynth", url, sizeof(url));
      check(std::strcmp(url, "https://api.github.com/repos/ada/tinysynth/releases/latest") == 0, url);
      tagsUrl("ada/tinysynth", url, sizeof(url));
      check(std::strcmp(url, "https://api.github.com/repos/ada/tinysynth/tags?per_page=1") == 0, url);

      Watched back[kMaxWatched];
      const int read = unpackWatched(packWatched(list, count) + "not a repository\n\n", back, kMaxWatched);
      check(read == 3 && isNew(back[0]) && std::strcmp(back[0].seenTag, "v2.1.0") == 0 &&
                std::strcmp(back[0].published, "2026-11-16") == 0 && std::strcmp(back[2].tag, "1.6.5") == 0 &&
                std::strcmp(back[1].checked, "2026-11-18") == 0,
            "the watched list round trips");
      count = toggleWatched(list, count, "Ada/TinySynth");
      check(count == 2 && findWatched(list, count, "ada/tinysynth") == -1, "a second toggle stops watching");
      for (int i = 0; i < kMaxWatched + 3; ++i) {
        std::snprintf(name, sizeof(name), "o/r%d", i);
        count = toggleWatched(list, count, name);
      }
      check(count == kMaxWatched && findWatched(list, count, "o/r11") == -1, "a full list takes no more");
    }

    std::printf("discover: all tests passed (%d checks)\n", checks);
    return 0;
  }
  return 1;
}
