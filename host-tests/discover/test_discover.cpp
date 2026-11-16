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
    // 17 November 2026 is day 20774.
    searchUrl(f, 20774, url, sizeof(url));
    check(std::strcmp(url,
                      "https://api.github.com/search/repositories?q=stars:%3E=50+created:%3E=2026-11-10"
                      "&sort=stars&order=desc&per_page=20") == 0,
          url);
    f.topic = Topic::Synths;
    f.language = Language::Cpp;
    f.stars = Stars::Thousand;
    f.age = Age::AnyActive;
    searchUrl(f, 20774, url, sizeof(url));
    check(std::strcmp(url,
                      "https://api.github.com/search/repositories?q=synthesizer+OR+eurorack+OR+midi+OR+synth+OR+dsp"
                      "+in:name,description,topics+language:cpp+stars:%3E=1000+pushed:%3E=2026-10-18"
                      "&sort=stars&order=desc&per_page=20") == 0,
          url);
    for (int t = 0; t < kTopics; ++t) {
      f.topic = static_cast<Topic>(t);
      f.language = Language::TypeScript;
      searchUrl(f, 20774, url, sizeof(url));
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

    uint8_t bytes[4];
    Filter back;
    packFilter(f, bytes);
    check(unpackFilter(bytes, back) && back.topic == f.topic && back.age == Age::AnyActive, "the filter round trips");
    bytes[1] = 200;
    check(!unpackFilter(bytes, back), "a language that is not one");
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
    std::printf("discover: all tests passed (%d checks)\n", checks);
    return 0;
  }
  return 1;
}
