#pragma once

// DISCOVER's model: a GitHub repository search built from four choices, the
// answer read into a page of repositories, and the ones kept. Freestanding (no
// Arduino), so host-tests/discover runs it against an answer in GitHub's shape.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace discover {

enum class Topic : uint8_t { Everything = 0, Ai, Firmware, Synths, Emulators, Gadgets };
constexpr int kTopics = 6;
enum class Language : uint8_t { Any = 0, C, Cpp, Rust, Python, Go, TypeScript, Zig };
constexpr int kLanguages = 8;
enum class Stars : uint8_t { Ten = 0, Fifty, TwoHundred, Thousand };
constexpr int kStarChoices = 4;
// How new: made this week, this month or this year, or of any age so long as
// it was worked on this month.
enum class Age : uint8_t { Week = 0, Month, Year, AnyActive };
constexpr int kAges = 4;

struct Filter {
  Topic topic = Topic::Everything;
  Language language = Language::Any;
  Stars stars = Stars::Fifty;
  Age age = Age::Week;
};
const char* topicName(Topic topic);
const char* languageName(Language language);
const char* starsName(Stars stars);
const char* ageName(Age age);
int starsFloor(Stars stars);
bool packFilter(const Filter& filter, uint8_t out[4]);
bool unpackFilter(const uint8_t in[4], Filter& out);

void civilOfDay(int32_t days, int& year, int& month, int& day);
constexpr int kPerPage = 20;
// The search for a filter, most stars first. `today` is days since 1970.
void searchUrl(const Filter& filter, int32_t today, char* out, size_t cap);
void readmeUrl(const char* fullName, char* out, size_t cap);

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

// The repositories kept, as text: a line each, fields between tabs.
constexpr int kMaxSaved = 24;
std::string packSaved(const Repo* repos, int count);
int unpackSaved(std::string_view text, Repo* out, int cap);
int findSaved(const Repo* repos, int count, const char* fullName);
// Adds or removes; a full list gives up its oldest. Returns the new count.
int toggleSaved(Repo* repos, int count, const Repo& repo);

}  // namespace discover
