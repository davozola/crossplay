#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "BatchCore.h"

using namespace batch;

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
const Issue* find(const std::vector<Issue>& issues, const int n) {
  for (const Issue& i : issues) {
    if (i.number == n) return &i;
  }
  return nullptr;
}
}  // namespace

int main() {
  std::vector<Issue> issues;
  {  // the feed as it is published
    std::ifstream in("feed.xml");
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string xml = ss.str();
    const int added = addFromFeed(xml, issues);
    check(added == 9, "every item with an issue number");
    const Issue* i373 = find(issues, 373);
    check(i373 != nullptr && std::strcmp(i373->date, "02 OCT 2026") == 0, "a date");
    check(i373 != nullptr && i373->title.rfind("The Next Top Open Model", 0) == 0, "a title");
    check(find(issues, 1) != nullptr && find(issues, 9999) == nullptr, "from the first");
    check(addFromFeed(xml, issues) == 0, "reading it twice adds nothing");
    bool entities = false;
    for (const Issue& i : issues) entities = entities || i.title.find("&amp;") != std::string::npos;
    check(!entities, "entities are decoded");
    check(newestNumber(issues) == 373, "the newest");
  }

  {  // the front page, for what the feed has not caught up with
    const std::string page =
        "Title: The Batch\n\nMarkdown Content:\n"
        "[![Image 1: cover](https://x/y.png)](https://www.deeplearning.ai/the-batch/issue-379/)\n\n"
        "Nov 13, 2026\n\n"
        "[GPT-6 Cyber Versus Linux, The Problem With LLM-Powered Search, LLMs Edit Model Weights]"
        "(https://www.deeplearning.ai/the-batch/issue-379/)\n\n"
        "The Batch AI News and Insights: something.\n\n"
        "Nov 06, 2026\n\n"
        "[Gemini 4 Carbon Heads Toward Wider Availability, What an IPO Means for Developers]"
        "(https://www.deeplearning.ai/the-batch/issue-378/)\n\n"
        "[Read more](https://www.deeplearning.ai/the-batch/issue-373/)\n"
        "[A tag](https://www.deeplearning.ai/the-batch/tag/research/)\n";
    const int added = addFromFrontPage(page, issues);
    check(added == 2, "two it did not have");
    const Issue* i379 = find(issues, 379);
    check(i379 != nullptr && i379->title.rfind("GPT-6 Cyber Versus Linux", 0) == 0, "the headline, not the image");
    check(i379 != nullptr && std::strcmp(i379->date, "13 NOV 2026") == 0, "its date");
    const Issue* i378 = find(issues, 378);
    check(i378 != nullptr && std::strcmp(i378->date, "06 NOV 2026") == 0, "the next one's own date");
    check(find(issues, 373)->title.rfind("The Next Top Open Model", 0) == 0, "a short link does not rename an issue");
    sortIssues(issues);
    check(issues.front().number == 379 && issues.back().number == 1, "newest first");

    const std::string packed = packIndex(issues);
    std::vector<Issue> back;
    unpackIndex(packed, back);
    check(back.size() == issues.size() && back.front().number == 379 && back.front().title == issues.front().title &&
              std::strcmp(back[1].date, "06 NOV 2026") == 0,
          "the index round trips");
  }

  {  // one issue, cut at its headings
    std::string words;
    for (int i = 0; i < 80; ++i) words += "word ";
    const std::string issue =
        "[Skip](https://x/)\n\n* [Courses](https://x/c)\n* [The Batch](https://x/b)\n\n"
        "GLM-5.3's Score is Good News\n============================\n\n"
        "Dear friends,\n\n" +
        words + "\n\n" + words +
        "\n\nKeep building!\n\nAndrew\n\n"
        "A MESSAGE FROM DEEPLEARNING.AI\n------------------------------\n\n[![promo](https://x/p.png)](https://x/p)\n\n"
        "Join our course today.\n\n"
        "News\n====\n\n"
        "![Image 3: chart](https://x/c.png)\n\n"
        "## An Unexpected **Open Weights** Leader\n\n" +
        words + "\n\n**What's new:** " + words +
        "\n\n"
        "### Voice, Video, and [Reasoning](https://x/r) in One\n\n" +
        words + words +
        "\n\n"
        "Subscribe\n---------\n\nGet the latest.\n";
    std::vector<Section> sections;
    splitIssue(issue, sections);
    check(sections.size() == 3, "the letter and two stories; menus and promotions dropped");
    if (sections.size() == 3) {
      check(sections[0].title == "Letter from Andrew Ng", "the letter is named");
      check(sections[0].paragraphs.front().rfind("Dear friends", 0) == 0, "and begins where it begins");
      check(sections[1].title == "An Unexpected Open Weights Leader", sections[1].title.c_str());
      check(sections[2].title == "Voice, Video, and Reasoning in One", sections[2].title.c_str());
      check(sections[1].words >= 160 && sections[1].words < 175, "words counted");
      check(sections[1].paragraphs.size() == 2, "paragraphs kept apart");
    }
    check(wordsIn(sections) > 480, "the issue's length");
    check(minutesFor(0) == 1 && minutesFor(220) == 1 && minutesFor(440) == 2 && minutesFor(4000) == 18, "minutes");

    std::vector<Section> none;
    splitIssue("", none);
    check(none.empty(), "nothing in nothing");
    splitIssue(words + words, none);
    check(none.size() == 1 && none[0].title == "Opening", "prose with no heading is one piece");
  }

  {  // urls
    char url[128];
    issueUrl(379, url, sizeof(url));
    check(std::strcmp(url, "https://r.jina.ai/https://www.deeplearning.ai/the-batch/issue-379/") == 0, url);
    frontPageUrl(url, sizeof(url));
    check(std::strcmp(url, "https://r.jina.ai/https://www.deeplearning.ai/the-batch/") == 0, url);
  }

  {  // read marks and favourites
    Shelf shelf;
    check(!isRead(shelf, 379), "unread to begin with");
    markRead(shelf, 379);
    markRead(shelf, 9000);
    check(isRead(shelf, 379) && !isRead(shelf, 378) && !isRead(shelf, 9000), "read");
    toggleFavourite(shelf, 379, kWholeIssue);
    toggleFavourite(shelf, 373, 2);
    check(isFavourite(shelf, 379, kWholeIssue) && isFavourite(shelf, 373, 2) && !isFavourite(shelf, 373, 1),
          "an issue and a story");
    check(hasFavourite(shelf, 373) && !hasFavourite(shelf, 372), "an issue with a kept story");
    toggleFavourite(shelf, 379, kWholeIssue);
    check(!isFavourite(shelf, 379, kWholeIssue) && shelf.favouriteCount == 1, "a second toggle removes");
    for (int i = 0; i < kMaxFavourites + 5; ++i) toggleFavourite(shelf, 100 + i, kWholeIssue);
    check(shelf.favouriteCount == kMaxFavourites && !isFavourite(shelf, 373, 2) &&
              isFavourite(shelf, 100 + kMaxFavourites + 4, kWholeIssue),
          "the oldest make room");
    uint8_t bytes[kShelfBytes];
    check(packShelf(shelf, bytes, sizeof(bytes)) == kShelfBytes && packShelf(shelf, bytes, 4) == 0, "pack size");
    Shelf back;
    check(unpackShelf(bytes, sizeof(bytes), back) && isRead(back, 379) && back.favouriteCount == kMaxFavourites &&
              isFavourite(back, 120, kWholeIssue),
          "round trip");
    bytes[66] = 200;
    check(!unpackShelf(bytes, sizeof(bytes), back), "a count past the room is not read");
  }

  if (failures == 0) {
    std::printf("batch: all tests passed (%d checks)\n", checks);
    return 0;
  }
  return 1;
}
