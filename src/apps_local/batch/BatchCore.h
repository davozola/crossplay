#pragma once

// THE BATCH's model: the list of issues, read out of a feed someone else keeps
// and out of the newsletter's own front page, and one issue cut into the
// letter and the stories it carries. Freestanding (no Arduino), so
// host-tests/batch runs it against text in those shapes.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace batch {

struct Issue {
  int number = 0;
  char date[12] = "";  // "02 OCT 2026", or empty when nothing said
  std::string title;
};

// The archive as a third-party RSS feed lists it: one <item> an issue, its
// link ending in issue-N. Adds what `issues` lacks; returns how many it added.
int addFromFeed(std::string_view xml, std::vector<Issue>& issues);
// The newsletter's front page as the extractor returns it: markdown links to
// issue-N with the headline as their text and a date near them.
int addFromFrontPage(std::string_view markdown, std::vector<Issue>& issues);
// Newest first.
void sortIssues(std::vector<Issue>& issues);
int newestNumber(const std::vector<Issue>& issues);

// One line an issue: number, date and title between tabs.
std::string packIndex(const std::vector<Issue>& issues);
void unpackIndex(std::string_view text, std::vector<Issue>& issues);

// A piece of an issue that reads on its own: the letter, or one story.
struct Section {
  std::string title;
  std::vector<std::string> paragraphs;
  int words = 0;
};
// Cuts the extractor's markdown at its headings. What is too short to be a
// story (menus, a share row, a promotion's caption) is left out.
void splitIssue(std::string_view markdown, std::vector<Section>& sections);
// Minutes at 220 words a minute, never less than one.
int minutesFor(int words);
int wordsIn(const std::vector<Section>& sections);

// The URLs. `out` gets the extractor's address for an issue or the front page.
void issueUrl(int number, char* out, size_t cap);
void frontPageUrl(char* out, size_t cap);
constexpr const char* kFeedUrl = "https://raw.githubusercontent.com/Olshansk/rss-feeds/main/feeds/feed_the_batch.xml";

// What has been read and what is kept. A favourite is an issue, or one section of it.
constexpr int kMaxFavourites = 48;
constexpr uint8_t kWholeIssue = 255;
struct Favourite {
  uint16_t issue = 0;
  uint8_t section = kWholeIssue;
};
struct Shelf {
  uint8_t read[64] = {};  // a bit an issue number, to 511
  Favourite favourites[kMaxFavourites];
  int favouriteCount = 0;
};
bool isRead(const Shelf& shelf, int issue);
void markRead(Shelf& shelf, int issue);
bool isFavourite(const Shelf& shelf, int issue, uint8_t section);
// True when the issue or any section of it is kept.
bool hasFavourite(const Shelf& shelf, int issue);
// Adds or removes. A full shelf gives up its oldest.
void toggleFavourite(Shelf& shelf, int issue, uint8_t section);
constexpr size_t kShelfBytes = 2 + 64 + 1 + kMaxFavourites * 3;
size_t packShelf(const Shelf& shelf, uint8_t* out, size_t cap);
bool unpackShelf(const uint8_t* in, size_t size, Shelf& out);

}  // namespace batch
