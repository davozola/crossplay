#pragma once

// DISCOVER's model: a GitHub repository search built from four choices or the
// reader's own words, the answer read into a page of repositories, the ones
// kept, the ones already seen, and the repositories whose releases are
// watched. Freestanding (no Arduino), so host-tests/discover runs it against
// answers in GitHub's shape.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace discover {

// Own is the reader's words in Filter::query; the six before it are built in.
enum class Topic : uint8_t { Everything = 0, Ai, Firmware, Synths, Emulators, Gadgets, Own };
constexpr int kTopics = 6;
enum class Language : uint8_t { Any = 0, C, Cpp, Rust, Python, Go, TypeScript, Zig };
constexpr int kLanguages = 8;
enum class Stars : uint8_t { Ten = 0, Fifty, TwoHundred, Thousand, Any };
constexpr int kStarChoices = 5;
// How new: made this week, this month or this year; of any age so long as it
// was worked on this month; rising, which is made in the last two months,
// worked on this week and put in order of stars a day; or of any age at all.
// The values are what the card holds, so new ones go on the end.
enum class Age : uint8_t { Week = 0, Month, Year, AnyActive, Rising, Ever };
constexpr int kAges = 6;
constexpr size_t kQueryCap = 40;

struct Filter {
  Topic topic = Topic::Everything;
  Language language = Language::Any;
  Stars stars = Stars::Fifty;
  Age age = Age::Rising;
  // Leave out the repositories already shown on a page. Only the searches
  // bound to a date do: see skipsSeen().
  bool skipSeen = true;
  char query[kQueryCap] = "";
};
// The order the pickers list them in, which is not the order they are kept in.
Stars starsAt(int row);
Age ageAt(int row);
bool skipsSeen(const Filter& filter);
// What was typed, as a search: trimmed, one space between words, ASCII only.
void tidyQuery(const char* typed, char* out, size_t cap);
const char* topicName(Topic topic);
const char* languageName(Language language);
const char* starsName(Stars stars);
const char* ageName(Age age);
int starsFloor(Stars stars);
// Five bytes and then the query. Four bytes alone is a filter from before the
// query and the skipping, and still reads.
std::string packFilter(const Filter& filter);
bool unpackFilter(std::string_view in, Filter& out);

void civilOfDay(int32_t days, int& year, int& month, int& day);
// "2026-11-10" as days since 1970, or -1.
int32_t dayOfDate(const char* iso);
constexpr int kPerPage = 20;
// The search for a filter, most stars first. `today` is days since 1970 and
// `page` counts from one.
void searchUrl(const Filter& filter, int32_t today, int page, char* out, size_t cap);
void readmeUrl(const char* fullName, char* out, size_t cap);
// The repository's page, for a phone to open.
void pageUrl(const char* fullName, char* out, size_t cap);

struct Repo {
  char fullName[72] = "";  // "owner/name"
  char description[220] = "";
  char language[16] = "";
  char created[11] = "";  // "2026-11-10"
  char pushed[11] = "";
  char license[20] = "";
  char homepage[72] = "";
  char topics[96] = "";  // a few, comma separated
  int32_t stars = 0;
  int32_t forks = 0;
  int32_t issues = 0;
};
const char* nameOf(const Repo& repo);  // the part after the slash
// GitHub's answer to a repository search. Returns how many it read, `cap` at
// most; `total` is how many matched in all. -1 when it is not such an answer.
int parseSearch(const char* json, size_t len, Repo* out, int cap, int32_t& total);
// "1.2K", "34.5K", "812"
void formatCount(int32_t n, char* out, size_t cap);
// Keeps what the reading face can set: typography folded, anything past
// Latin-1 (emoji, other scripts) dropped.
void latinOnly(char* text);

// Stars for each day since it was made: the nearest thing to "climbing" that a
// search answer holds.
int32_t starsPerDay(const Repo& repo, int32_t today);
void rankByPace(Repo* repos, int count, int32_t today);

// The searches the reader has typed, newest first.
constexpr int kMaxTerms = 6;
struct Terms {
  char items[kMaxTerms][kQueryCap] = {};
  int count = 0;
};
void rememberTerm(Terms& terms, const char* term);
void forgetTerm(Terms& terms, int index);
std::string packTerms(const Terms& terms);
void unpackTerms(std::string_view text, Terms& out);

// The repositories already shown, as hashes of their names; the oldest make
// room.
constexpr int kMaxSeen = 600;
struct Seen {
  uint32_t names[kMaxSeen] = {};
  int count = 0;
};
uint32_t nameHash(const char* fullName);
bool hasSeen(const Seen& seen, const char* fullName);
// False when it was there already.
bool markSeen(Seen& seen, const char* fullName);
// Closes the list over the ones seen. Returns the new count.
int dropSeen(Repo* repos, int count, const Seen& seen);

// A repository whose releases are watched.
constexpr int kMaxWatched = 12;
struct Watched {
  char fullName[72] = "";
  char tag[40] = "";      // the newest version GitHub lists
  char seenTag[40] = "";  // the one the reader last opened, or the first one found
  char title[80] = "";
  char published[11] = "";
  char checked[11] = "";  // the day GitHub last answered for it
};
bool isNew(const Watched& watched);
// "owner/name" out of what was typed, a whole github.com address included.
// False when it is not one.
bool tidyFullName(const char* typed, char* out, size_t cap);
void releaseUrl(const char* fullName, char* out, size_t cap);
void tagsUrl(const char* fullName, char* out, size_t cap);
// Where a phone reads about the version: its release, or the list of them.
void releasePageUrl(const Watched& watched, char* out, size_t cap);
// GitHub's answer for a repository's latest release: the tag, the title and
// the day into `out`, the notes as they were written into `notes`.
bool parseRelease(const char* json, size_t len, Watched& out, std::string& notes);
// GitHub's answer for a repository's tags, newest first, for the ones that
// tag without releasing.
bool parseTags(const char* json, size_t len, Watched& out);
// Takes what GitHub answered. The first version found for a repository is
// where the reader starts from, not news.
void noteRelease(Watched& watched, const Watched& fetched, const char* today);
int findWatched(const Watched* list, int count, const char* fullName);
// Adds, or removes when it is there. A full list refuses. Returns the count.
int toggleWatched(Watched* list, int count, const char* fullName);
// The new ones first, then the latest published.
void sortWatched(Watched* list, int count);
std::string packWatched(const Watched* list, int count);
int unpackWatched(std::string_view text, Watched* out, int cap);

// The repositories kept, as text: a line each, fields between tabs.
constexpr int kMaxSaved = 24;
std::string packSaved(const Repo* repos, int count);
int unpackSaved(std::string_view text, Repo* out, int cap);
int findSaved(const Repo* repos, int count, const char* fullName);
// Adds or removes; a full list gives up its oldest. Returns the new count.
int toggleSaved(Repo* repos, int count, const Repo& repo);

}  // namespace discover
