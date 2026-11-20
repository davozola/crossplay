#pragma once

// What NEWS draws. It does not look like the rest of the fork on purpose: no
// black band, no boxed rows, no pill buttons. It is set like a page of a paper
// -- a serif masthead over a double rule, headlines in bold serif divided by
// hairlines, and a story that runs from edge to edge with its controls in one
// quiet line -- because the one job of this app is to be read.
//
// A target draws from three faces, so each screen is given two: `page` carries
// the display serif, the reading serif at the size the reader chose, and a
// small serif; `heads` carries the bold serif at two sizes and the small
// reading serif.

#include "../ui/ToyboxScreen.h"
#include "../ui/ToyboxWrappedText.h"

namespace newsui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionMenu = 340,
  ActionBack = 341,
  ActionFavourite = 342,
  ActionTextSize = 343,
  ActionRefresh = 344,
  ActionChoose = 345,
  ActionFavourites = 346,
  ActionPrev = 347,
  ActionNext = 348,
  ActionDone = 349,
  ActionOpen0 = 360,     // + index in the section, up to kMaxRows
  ActionSection0 = 380,  // + index among the chosen sections
  ActionToggle0 = 400,   // + row on the chooser's page
};

constexpr int kMaxRows = 16;
constexpr int kChooserRows = 11;

struct Ink {
  fui::DrawTarget& page;
  fui::DrawTarget& heads;
  const fui::ThemeTokens& tokens;
};

struct Headline {
  const char* title = "";
  const char* summary = nullptr;  // drawn under the first one only
};

struct FrontModel {
  const char* section = "";
  const char* edition = "";    // "18 NOV", or ""
  const char* note = nullptr;  // a word at the foot while something runs behind the page
  const Headline* items = nullptr;
  int count = 0;
  int top = 0;  // first headline on this page
  int page = 1;
  int pages = 1;
  const char* emptyHeadline = nullptr;
  const char* emptyMessage = nullptr;
  bool emptyCanFetch = false;
};
// How many headlines a page holds starting at `top`: they are set whole, so it
// depends on how long they are.
int frontFit(const Ink& ink, const fui::DeviceContext& device, const Headline* items, int count, int top);
int buildFront(toybox::Screen& screen, const Ink& ink, const FrontModel& model);

struct MenuModel {
  const char* edition = "";
  const char* const* sections = nullptr;
  int sectionCount = 0;
  int current = -1;  // the section being read; -1 for the favourites
  int favourites = 0;
};
void buildMenu(toybox::Screen& screen, const Ink& ink, const MenuModel& model);

struct ChooserModel {
  const char* const* names = nullptr;  // this page's rows
  const bool* chosen = nullptr;
  int count = 0;
  int chosenTotal = 0;
  int maxChosen = 0;
  int page = 1;
  int pages = 1;
};
void buildChooser(toybox::Screen& screen, const Ink& ink, const ChooserModel& model);

struct StoryModel {
  const char* section = "";
  const char* title = "";
  const char* body = "";
  toybox::WrappedText* wrap = nullptr;
  uint32_t topLine = 0;
  bool firstPage = true;
  const char* pageLabel = "";
  bool favourite = false;
};
void buildStory(toybox::Screen& screen, const Ink& ink, const StoryModel& model);

// Where a story's text goes, and how many lines of it each page holds.
struct StoryMetrics {
  fui::Rect body;       // on every page but the first
  fui::Rect firstBody;  // under the headline
  uint16_t firstLines = 1;
  uint16_t restLines = 1;
};
StoryMetrics storyMetrics(const Ink& ink, const fui::DeviceContext& device, const char* title);

struct NoticeModel {
  const char* headline = "";
  const char* message = "";
  bool backButton = false;
};
void buildNotice(toybox::Screen& screen, const Ink& ink, const NoticeModel& model);

}  // namespace newsui
