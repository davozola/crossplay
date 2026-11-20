#include "ShelfFolderActivity.h"

#include <FreeInkUIIcon.h>
#include <InflateReader.h>
#include <Logging.h>
#include <Memory.h>

#include "Shelf.h"
#include "ShelfCatalogue.h"
#include "ShelfOrder.h"
#include "ShelfScreen.h"
#include "ShelfShots.h"
#include "player/PlayerName.h"
#include "ui/Toybox.h"
#include "ui/ToyboxFonts.h"
#include "ui/ToyboxTheme.h"

std::unique_ptr<Activity> ShelfFolderActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                      const int folderIndex) {
  return makeUniqueNoThrow<ShelfFolderActivity>(renderer, mappedInput, folderIndex);
}

namespace {
// Kept while the device is on, so the filter is still there on the way back
// from the game it was used to find.
// Where each folder was left, for as long as the reader stays awake: a game
// closed lands back on the tab and page it was opened from.
int tabIn[8] = {};
int pageIn[8] = {};
bool visitedIn[8] = {};
}  // namespace

int ShelfFolderActivity::rowsInList() const {
  if (choosing) return shelf::folders()[folder].count;
  return listedCount;
}

// The one place a row becomes an item, so a tap cannot open what the row under
// the finger was not naming. Choosing lists the whole registry; otherwise it
// is whatever the tab holds.
int ShelfFolderActivity::itemAtRow(const int row) const {
  if (row < 0) return -1;
  if (choosing) return row < shelf::folders()[folder].count ? row : -1;
  return row < listedCount ? listed[row] : -1;
}

// What the tab holds. The first is the favourites and nothing else;
// a kind is every shown item of that kind; the last tab is every shown item.
// Nothing is listed twice, and a hidden item is on none of them.
void ShelfFolderActivity::rebuildList() {
  const shelf::Folder& self = shelf::folders()[folder];
  listedCount = 0;
  auto add = [&](const int item) {
    for (int i = 0; i < listedCount; ++i) {
      if (listed[i] == item) return;
    }
    if (listedCount < kMaxListed && !shelf::isHidden(folder, item)) listed[listedCount++] = item;
  };
  if (tab == 0) {
    // Only what was starred, in the order it was starred. It used to be filled
    // out with what was last opened, and a star then landed among rows that
    // were not stars: the tab could not say where a favourite had gone.
    int held[shelf::kFavouriteSlots];
    const int stars = shelf::favourites(folder, held);
    for (int i = 0; i < stars; ++i) add(held[i]);
    return;
  }
  const int kind = tab - 1;
  for (int i = 0; i < self.count; ++i) {
    if (tab == self.tabCount + 1 || self.items[i].category == kind || self.items[i].also == kind) add(i);
  }
}

void ShelfFolderActivity::setTab(const int to) {
  const shelf::Folder& self = shelf::folders()[folder];
  tab = to < 0 || to > self.tabCount + 1 ? 0 : to;
  interactionsReady = false;
  rebuildList();
  itemCount = rowsInList();
  selected = 0;
  if (folder >= 0 && folder < 8) {
    tabIn[folder] = tab;
    pageIn[folder] = 0;
  }
  requestUpdate();
}

void ShelfFolderActivity::openSheet(const int item) {
  const shelf::Folder& self = shelf::folders()[folder];
  if (item < 0 || item >= self.count) return;
  sheetItem = item;
  sheetPicture.reset();
  // The picture is kept deflated in flash and inflated only while its sheet is up.
  if (const shelfshots::Shot* shot = shelfshots::find(self.items[item].title)) {
    auto bits = makeUniqueNoThrow<uint8_t[]>(shelfshots::kRawBytes);
    InflateReader reader;
    if (!bits) {
      LOG_ERR("SHELF", "OOM: %u bytes for a picture", static_cast<unsigned>(shelfshots::kRawBytes));
    } else if (reader.init(false)) {
      reader.setSource(shot->data, shot->size);
      // The two zlib header bytes (CMF, FLG). Upstream dropped
      // InflateReader::skipZlibHeader() as unused, so it is skipped here.
      uzlib_get_byte(reader.raw());
      uzlib_get_byte(reader.raw());
      if (reader.read(bits.get(), shelfshots::kRawBytes)) {
        sheetPicture = std::move(bits);
      } else {
        LOG_ERR("SHELF", "picture of %s would not inflate", self.items[item].title);
      }
    }
  }
  interactionsReady = false;
  requestUpdate();
}

void ShelfFolderActivity::closeSheet() {
  sheetItem = -1;
  sheetPicture.reset();
  interactionsReady = false;
  rebuildList();
  itemCount = rowsInList();
  if (selected >= itemCount) selected = itemCount > 0 ? itemCount - 1 : 0;
  requestUpdate();
}

void ShelfFolderActivity::buildPage(const int first, const int count) {
  const shelf::Folder& self = shelf::folders()[folder];
  // Cannot happen on this panel: the tallest band a folder can have fits ten
  // rows and the array holds sixteen. Logged rather than clamped in silence
  // because a page that quietly drew fewer rows than it was asked for is
  // exactly the failure the registry cap used to have.
  if (count > kMaxRowsPerPage) LOG_ERR("SHELF", "Page of %d exceeds %d rows", count, kMaxRowsPerPage);
  for (int i = 0; i < count && i < kMaxRowsPerPage; ++i) {
    const int item = itemAtRow(first + i);
    if (item < 0) {
      // A row the list does not have. Cannot happen -- render() slices against
      // the same count -- and is logged rather than drawn blank, because a row
      // with no label is the one thing a person cannot report.
      LOG_ERR("SHELF", "Row %d has no item in %s", first + i, self.title);
      items[i].label = "";
      icons[i] = nullptr;
      checks[i] = false;
      stars[i] = false;
      // And it must not still answer for whatever the last page put here: the
      // row is registered whatever its label says, so a stale value is a blank
      // row that opens a game.
      items[i].actionValue = -1;
      continue;
    }
    items[i].label = self.items[item].title;
    icons[i] = self.items[item].icon;
    checks[i] = !shelf::isHidden(folder, item);
    stars[i] = shelf::isFavourite(folder, item);
    // The row's index in the WHOLE list, so a tap says which entry it is rather
    // than which row of which page. The list component emits actionValue rather
    // than the row, which is what lets the screen be handed a slice at all.
    items[i].actionValue = static_cast<int16_t>(first + i);
  }
}

void ShelfFolderActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  favouritesOnly = false;
  sheetItem = -1;
  const bool known = folder >= 0 && folder < 8 && visitedIn[folder];
  tab = known ? tabIn[folder] : 0;
  rebuildList();
  // The first visit opens on what you use; with nothing starred or opened yet
  // that is an empty page, so it opens on everything instead.
  if (!known && listedCount == 0) {
    tab = shelf::folders()[folder].tabCount + 1;
    rebuildList();
  }
  itemCount = rowsInList();
  selected = 0;
  if (known) {
    const int row = pageIn[folder] * shelfcat::kRowsPerPage;
    selected = row < itemCount ? row : 0;
  }
  if (folder >= 0 && folder < 8) {
    visitedIn[folder] = true;
    tabIn[folder] = tab;
  }
  requestUpdate();
}

// The star filter became the home tab; this is the one way onto it by name.
void ShelfFolderActivity::setFavouritesOnly(const bool on) { setTab(on ? 0 : shelf::folders()[folder].tabCount + 1); }

// In and out of choosing what the folder shows. The chooser lists the whole
// registry; leaving it goes back to the tab, rebuilt for what was just shown
// or hidden.
void ShelfFolderActivity::setChoosing(const bool on) {
  if (choosing == on) return;
  interactionsReady = false;
  choosing = on;
  if (!choosing) rebuildList();
  itemCount = rowsInList();
  selected = 0;
  requestUpdate();
}

// One step past the end of a tab's pages: the tab beside it, on its first page
// going forward and its last going back, so a swipe walks the whole folder as
// one run of pages. A tab with nothing on it is stepped over. False when there
// is no tab that way.
bool ShelfFolderActivity::stepSection(const int delta) {
  if (choosing) return false;
  const int last = shelf::folders()[folder].tabCount + 1;
  const int from = tab;
  for (int to = from + delta; to >= 0 && to <= last; to += delta) {
    tab = to;
    rebuildList();
    if (listedCount == 0) continue;
    setTab(to);
    if (delta < 0) {
      const int pages = shelfui::pageCountFor(itemCount, shelfcat::kRowsPerPage);
      if (pages > 1) {
        selected = shelfui::rowForPage(pages - 1, shelfcat::kRowsPerPage);
        if (folder >= 0 && folder < 8) pageIn[folder] = pages - 1;
      }
    }
    return true;
  }
  tab = from;
  rebuildList();
  return false;
}

bool ShelfFolderActivity::showPage(const int page) {
  const int landing = shelfui::rowForPage(page, rowsPerPage);
  // Which page a step actually landed on, because "it moved by two" and "it
  // started somewhere else" look identical from outside and cost three cold
  // testers an evening between them.
  LOG_DBG("SHELF", "Page %d -> %d of %d (row %d of %d)", shelfui::pageFor(selected, rowsPerPage), page,
          shelfui::pageCountFor(itemCount, rowsPerPage), landing, itemCount);
  if (landing < 0 || landing >= itemCount) return false;
  // Within this screen the page is DERIVED from the selection and held nowhere
  // else, so changing page means moving the selection onto the page's first row.
  selected = landing;
  requestUpdate();
  // The chooser pages over a different list, so its rows are not the folder's
  // and writing one would move where the FOLDER resumes. Leaving the mode
  // writes the row it lands on instead, which is the same fact recorded once.
  if (choosing) return true;
  // And that row is what the folder comes back to. Written here rather than on
  // the way out because there is no way out to hook: Back destroys this
  // activity, and the idle timeout deep-sleeps wherever you happen to be, with
  // wake a chip reset. Paging is a deliberate act a handful of times a session,
  // and the write is twenty bytes beside a full-panel repaint.
  if (folder >= 0 && folder < 8) pageIn[folder] = page;
  return true;
}

void ShelfFolderActivity::loop() {
  namespace fui = freeink::ui;

  int pressX = 0;
  int pressY = 0;
  if (mappedInput.wasScreenTouchPressed(pressX, pressY)) swipeStartX = pressX;

  if (sheetItem >= 0 && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    closeSheet();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    // A swipe to the right is the page before, and it is also the system's
    // Back when it starts in the left quarter. So that one gesture cannot
    // mean two things at an invisible line, a Back that came from a swipe
    // turns the page while there is a page before to turn to, and leaves only
    // from the first page: swiping right walks back through the pages and
    // then out. A swipe begun at the very edge always leaves.
    if (mappedInput.wasBackGesture() && swipeStartX >= kBackSwipeEdge && !choosing) {
      const int from = shelfui::pageFor(selected, rowsPerPage);
      if (from > 0) {
        swipeStartX = -1;
        showPage(from - 1);
        return;
      }
      // And from a tab's first page it is the tab before, so the same swipe
      // walks back through the sections and leaves only from the first.
      if (sheetItem < 0 && stepSection(-1)) {
        swipeStartX = -1;
        return;
      }
    }
    // Back has two rules in this fork -- an app returns to its folder, a folder
    // returns to Home -- and choosing adds the only exception: it closes the
    // mode and stays put. A mode that Back walked straight out of would make
    // Back mean "undo the thing I was doing" on the one screen where it is also
    // the only way out of the app, and the taps are already saved by then.
    if (choosing) {
      setChoosing(false);
      return;
    }
    // A folder's parent is Home. It does not name Home; leave() knows.
    shelf::leave(renderer, mappedInput);
    return;
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  // A long press on a row opens its sheet: what it is, and what can be done with it.
  if (!choosing && sheetItem < 0 && interactionsReady && mappedInput.wasScreenLongPress(tapX, tapY)) {
    fui::InputSnapshot held;
    held.touchReleased = true;
    held.touchX = static_cast<int16_t>(tapX);
    held.touchY = static_cast<int16_t>(tapY);
    const fui::ActionEvent on = interactions.route(held);
    const int item = on.action == shelfui::ActionOpen ? itemAtRow(on.value) : -1;
    if (item >= 0) openSheet(item);
    return;
  }
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  // The two side keys PAGE. They are the only physical buttons the X4 Pro has,
  // the case labels them previous and next page, and the reader turns pages with
  // them -- so paging the shelf with them is consistency with what the hardware
  // already says, not a new thing to learn. That is also why there is no
  // on-screen hint for it: the affordance is moulded into the case.
  //
  // They used to move a CURSOR, opened with Confirm. On this device that was a
  // dead end in the most literal way: `frontButtonConfirm` resolves to
  // PIN_UNASSIGNED, which InputManager::begin skips entirely, so Confirm can
  // never fire. You could move a selection you had no way to act on. The
  // design language had already removed this exact input model from Chess and
  // Connections for being a second, worse one running beside the real one;
  // here it was second, worse, and broken.
  //
  // The page marks stay tappable. A button must never be the only route to
  // something, or the invisible input model wins arguments it should not.
  //
  // Left and Right page as well. They are PIN_UNASSIGNED on both target boards
  // and can never fire there, but the simulator and the browser emulator wire
  // all six keys to the arrow keys, and on Home every arrow moves the cursor.
  // One level in, two of the four did nothing, which is the only place a person
  // meets these keys at all.
  const bool keyNext = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                       mappedInput.wasReleased(MappedInputManager::Button::Right);
  const bool keyPrev = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                       mappedInput.wasReleased(MappedInputManager::Button::Left);

  // A swipe pages, on both axes. Up or to the left is the next page, the way
  // the content moves under a finger; down or to the right is the one before.
  //
  // The horizontal axis is shared with Back, which is a left-to-right swipe
  // anchored in the left quarter of the panel. This list once paged sideways
  // and stopped, because a tester swiped back from page two and landed on
  // Home: the same gesture meant two things either side of a line nothing
  // drew. It pages sideways again with that settled above, where Back is
  // handled: a rightward swipe is the page before for as long as there is one.
  //
  // One band is consumed above this activity and cannot be had: a down-swipe
  // STARTING in the top 14% (y <= 112) is the light-panel gesture, taken by
  // ActivityManager before any activity sees it. That band is the header, and
  // the list starts at 112, so a swipe that starts on a row is never eaten.
  //
  // The bottom edge is free on the X4 Pro and is not free on the Sticky. The
  // Home gesture is a bottom-edge UP swipe only where the board has no home
  // key (`MappedInputManager::wasHomeGesture`); the X4 Pro's profile carries
  // `hasHomeKey = true` for the capacitive key under the panel, so its Home
  // gesture is that key and an up-swipe from the bottom pages here. The Sticky
  // has no such key, so on that board an up-swipe starting below y=688 goes
  // Home instead. What sits below 688 on this screen is the page bar and the
  // player bar, so the cost is a swipe begun on chrome.
  const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
  const bool next = keyNext || swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Left;
  const bool prev =
      keyPrev || swipe == MappedInputManager::SwipeDir::Down || swipe == MappedInputManager::SwipeDir::Right;

  if (sheetItem >= 0 && (next || prev)) return;
  if (next || prev) {
    // A step goes to the page beside this one, and past a tab's last page to
    // the tab beside it. Nothing happens at the two ends of the folder, and
    // nothing repaints either: a full-panel refresh that redraws the same rows
    // is half a second of blink saying a step was taken when none was.
    const int pages = itemCount > 0 ? shelfui::pageCountFor(itemCount, rowsPerPage) : 1;
    const int from = itemCount > 0 ? shelfui::pageFor(selected, rowsPerPage) : 0;
    const int to = shelfui::pageStepClamped(from, pages, next ? 1 : -1);
    if (to != from) {
      showPage(to);
    } else if (!stepSection(next ? 1 : -1)) {
      LOG_DBG("SHELF", "Page %d of %d is an end; step refused", from, pages);
    }
    return;
  }

  if (!input.touchReleased || !interactionsReady) return;

  const fui::ActionEvent event = interactions.route(input);
  if (sheetItem >= 0) {
    const int item = sheetItem;
    if (event.action == shelfcat::ActionSheetOpen) {
      sheetPicture.reset();
      sheetItem = -1;
      shelf::openItem(folder, item, renderer, mappedInput);
    } else if (event.action == shelfcat::ActionSheetFavourite) {
      shelf::toggleFavourite(folder, item);
      interactionsReady = false;
      requestUpdate();
    } else if (event.action == shelfcat::ActionSheetHide) {
      shelf::setHidden(folder, item, true);
      closeSheet();
    }
    return;
  }
  if (event.action == shelfcat::ActionTab) {
    setTab(event.value);
    return;
  }
  if (event.action == shelfui::ActionOpen) {
    selected = event.value;
    const int item = itemAtRow(selected);
    if (item >= 0) shelf::openItem(folder, item, renderer, mappedInput);
    return;
  }
  if (event.action == shelfui::ActionToggleFavourites) {
    setFavouritesOnly(!favouritesOnly);
    return;
  }
  if (event.action == shelfui::ActionChoose) {
    setChoosing(!choosing);
    return;
  }
  if (event.action == shelfui::ActionToggleShown) {
    const int item = itemAtRow(event.value);
    if (item >= 0) {
      shelf::setHidden(folder, item, !shelf::isHidden(folder, item));
      // The row the finger is on stays the row the finger is on: the chooser's
      // list is the registry, so hiding something never moves it. The list that
      // shrinks is the other one, and it is not on screen.
      selected = event.value;
      requestUpdate();
    }
    return;
  }
  if (event.action == shelfui::ActionGoToPage) {
    showPage(event.value);
    return;
  }
  if (event.action == shelfui::ActionOpenPlayer) {
    // A destination, not an edit. Nothing about the name changes here any more;
    // PLAYER owns it, and this bar is only how you get there.
    shelf::openPlayer(renderer, mappedInput);
  }
}

void ShelfFolderActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  const shelf::Folder& self = shelf::folders()[folder];

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::ThemeTokens& tokens = toybox::themeTokens();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);

  // Keep the selection on screen. The list is virtualized, so a selection below
  // the fold would otherwise be styled on a row that is never drawn.
  //
  // The page is derived from the selection rather than held beside it in a
  // member. Two facts that must agree are one fact stored once: a page member
  // would drift the moment a button moved the cursor off it, and the screen
  // would style a row it was not showing -- which is the bug the icons had, in a
  // second place. What crosses a reboot is that same one fact, the row; see
  // shelfui::rowForPage for why a row and not a page number.
  //
  // The folder's own bar, in both modes: the screen puts the player bar in it
  // while browsing and the chooser's caption in it while choosing, so the two
  // modes fit the same games on the same page and entering the chooser adds
  // boxes to the list rather than reshuffling it.
  const bool starShown = false;
  shelfui::Paging paging = shelfui::pagingFor(device, tokens, self.showsDeviceName, itemCount, starShown);
  if (!choosing) {
    paging.rowsPerPage = shelfcat::kRowsPerPage;
    paging.pageCount = shelfui::pageCountFor(itemCount, shelfcat::kRowsPerPage);
  }
  rowsPerPage = paging.rowsPerPage;
  const int page = shelfui::pageFor(selected, rowsPerPage);
  const int first = shelfui::rowForPage(page, rowsPerPage);
  // Short on the last page, which is the whole reason the screen is handed a
  // slice rather than the folder plus an offset. See MenuModel::items.
  const int onThisPage = itemCount - first < rowsPerPage ? itemCount - first : rowsPerPage;
  buildPage(first, onThisPage);

  // Toybox chrome is capitals; upstream's Home list is Title Case. The registry
  // stores the Title Case name because that is the one a person reads on Home,
  // and the shout happens here, where our own design language starts.
  char shouted[24];
  size_t n = 0;
  for (const char* c = self.title; *c != '\0' && n + 1 < sizeof(shouted); ++c, ++n) {
    shouted[n] = *c >= 'a' && *c <= 'z' ? static_cast<char>(*c - 'a' + 'A') : *c;
  }
  shouted[n] = '\0';

  if (!choosing && sheetItem >= 0 && sheetItem < self.count) {
    const shelf::Item& item = self.items[sheetItem];
    shelfcat::SheetModel sheet;
    sheet.title = item.title;
    sheet.about = item.about;
    sheet.kind = self.tabs[item.category];
    sheet.online = item.online;
    sheet.favourite = shelf::isFavourite(folder, sheetItem);
    sheet.picture = sheetPicture.get();
    shelfcat::drawSheet(screen, renderer, sheet);
    interactionsReady = true;
    toybox::reportOverflow(interactions, self.title);
    renderer.displayBuffer();
    return;
  }
  if (!choosing) {
    shelfcat::Row rows[shelfcat::kRowsPerPage];
    for (int i = 0; i < onThisPage && i < shelfcat::kRowsPerPage; ++i) {
      const int item = itemAtRow(first + i);
      if (item < 0) continue;
      rows[i].title = self.items[item].title;
      rows[i].blurb = self.items[item].blurb;
      rows[i].icon = self.items[item].icon;
      rows[i].star = stars[i];
      rows[i].value = static_cast<int16_t>(first + i);
    }
    shelfcat::ListModel list;
    list.title = shouted;
    list.mark = self.mark;
    list.kinds = self.tabs;
    list.kindCount = self.tabCount;
    list.tab = tab;
    list.rows = rows;
    list.rowCount = onThisPage < 0 ? 0 : onThisPage;
    list.page = page;
    list.pageCount = paging.pageCount;
    shelfcat::drawList(screen, renderer, list);
    interactionsReady = true;
    toybox::reportOverflow(interactions, self.title);
    const auto hints = mappedInput.mapLabels("Back", "Open", "Up", "Down");
    GUI.drawButtonHints(renderer, hints.btn1, hints.btn2, hints.btn3, hints.btn4);
    renderer.displayBuffer();
    return;
  }

  shelfui::MenuModel model;
  model.title = shouted;
  model.mark = self.mark;
  model.items = items;
  model.icons = icons;
  model.count = onThisPage;
  // Non-null is the mode, for the screen as well as for this file.
  model.checks = choosing ? checks : nullptr;
  model.playerName = self.showsDeviceName ? player::name() : nullptr;
  model.page = page;
  model.pageCount = paging.pageCount;
  model.stars = stars;
  model.starShown = starShown;
  model.starOn = false;
  shelfui::buildMenu(screen, model);

  interactionsReady = true;
  toybox::reportOverflow(interactions, self.title);

  // Only a board with no touch panel ever draws these (BaseTheme::drawButtonHints
  // returns immediately when gpio.hasTouch()), so they are the Sticky's copy of
  // what the chip in the corner says.
  const auto labels = choosing ? mappedInput.mapLabels("Done", "Show/Hide", "Up", "Down")
                               : mappedInput.mapLabels("Back", "Open", "Up", "Down");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
