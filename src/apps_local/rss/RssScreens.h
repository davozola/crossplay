#pragma once

// What RSS draws. Set like NEWS, a page of a paper rather than the fork's
// black band and boxed rows: a serif masthead over a double rule, headlines
// set whole and divided by hairlines, and controls in one quiet line at the
// foot. Freestanding builders: a model in, a drawn frame out, no renderer and
// no Activity, so host-tests/ui/ can assert what they drew and made tappable.
//
//   front   the posts of every feed, of one feed, or of the saved shelf. An
//           unread post is set bold behind a square; an opened one is plain
//   menu    where the front can go: every post, the saved shelf, each feed
//           with its unread count, and the way to the phone and to a refresh
//   notice  a wait or a refusal, with one word at the foot to leave by
//   phone   the QR code a phone scans to add and remove feeds
//
// A post is read in NEWS's story page (newsui::buildStory), so the two apps
// turn pages, change the type size and save the same way.

#include <cstdint>

#include "../news/NewsScreens.h"
#include "../ui/ToyboxScreen.h"

namespace rssui {

namespace fui = freeink::ui;
using Ink = newsui::Ink;

// NEWS owns 340 to 420 (its story page is drawn through); these sit above.
enum : fui::ActionId {
  ActionOpenPost = 430,  // value: the row's index in the list being shown
  ActionMenu = 431,
  ActionView = 432,  // value: 0 every post, 1 the saved shelf, 2 + n feed n
  ActionAddFeeds = 433,
  ActionPhoneDone = 434,
  ActionStopRefresh = 435,
  ActionRefresh = 436,
  ActionNext = 437,
  ActionNoticeBack = 438,
};

// What the front is showing: every post, one feed's, or the saved shelf.
enum class Tab : uint8_t { All, Feeds, Saved };

constexpr int kMaxRows = 16;
constexpr int kViewAll = 0;
constexpr int kViewSaved = 1;
constexpr int kViewFirstFeed = 2;

// --- The front -----------------------------------------------------------

struct Row {
  const char* title = "";
  // The small line over the headline: the site, the date, or both. May be "".
  const char* kicker = "";
  bool unread = false;
};

struct FrontModel {
  const char* title = "RSS";   // the masthead: the app, a feed's name, Saved
  const char* right = "";      // opposite it: "12 NEW", or ""
  const char* note = nullptr;  // the middle of the foot, in place of Refresh
  bool canRefresh = false;
  // A window of rows: rows[0] is row `first` of the list. Taps carry the
  // absolute index.
  const Row* rows = nullptr;
  int count = 0;
  int first = 0;
  // Pages are measured as they are reached, so the front knows which page it
  // is on and whether another follows, not how many there are.
  int page = 1;
  bool more = false;
  // Drawn instead of rows when there are none. The control is drawn only when
  // both its label and its action are set.
  const char* emptyHeadline = nullptr;
  const char* emptyMessage = nullptr;
  const char* emptyActionLabel = nullptr;
  fui::ActionId emptyAction = fui::NO_ACTION;
};

// How many of `rows` a page holds from its first: headlines are set whole, so
// it depends on how long they are. At least one when there is one.
int frontFit(const Ink& ink, const fui::DeviceContext& device, const Row* rows, int count);
// Returns how many rows it drew.
int buildFront(toybox::Screen& screen, const Ink& ink, const FrontModel& model);

// What an empty front says, and whether it offers a way to fill itself.
struct Empty {
  const char* headline = nullptr;
  const char* message = nullptr;
  const char* actionLabel = nullptr;
  fui::ActionId action = fui::NO_ACTION;
};
// `haveFeeds`: any feed at all is subscribed.
Empty emptyFor(Tab tab, bool haveFeeds);

// --- The menu ------------------------------------------------------------

struct MenuRow {
  const char* name = "";
  const char* right = nullptr;  // an unread count, "failed", or nothing
  int view = 0;                 // what ActionView carries
  bool current = false;
};

struct MenuModel {
  const char* right = "";         // opposite the masthead: "4 FEEDS"
  const MenuRow* rows = nullptr;  // this page's
  int count = 0;
  int page = 1;
  int pages = 1;
};
// The rows a page of the menu holds.
int menuRows(const Ink& ink, const fui::DeviceContext& device);
void buildMenu(toybox::Screen& screen, const Ink& ink, const MenuModel& model);

// --- The notice ----------------------------------------------------------

struct NoticeModel {
  const char* headline = "";
  const char* message = "";
  // The one word at the foot, drawn only when both are set.
  const char* actionLabel = nullptr;
  fui::ActionId action = fui::NO_ACTION;
};
void buildNotice(toybox::Screen& screen, const Ink& ink, const NoticeModel& model);

// --- The phone -----------------------------------------------------------

struct PhoneModel {
  // What a person reads and can type: the mDNS name when the responder
  // started, the dotted address when it did not.
  const char* readable = "";
  // One line saying what the phone has done, or what the reader is doing for
  // it: "3 feeds.", "Adding kottke.org...", "Added Kottke.", "No feed there."
  const char* status = "";
};

// Returns the square the caller draws the code into: QrUtils needs a renderer,
// which this layer does not have.
fui::Rect buildPhone(toybox::Screen& screen, const Ink& ink, const PhoneModel& model);

}  // namespace rssui
