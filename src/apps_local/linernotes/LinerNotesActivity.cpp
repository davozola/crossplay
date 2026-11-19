#include "LinerNotesActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../DevMode.h"
#include "../../HapticFeedback.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../jukebox/JukeboxNet.h"
#include "../jukebox/JukeboxPlay.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace ln = linernotes;

namespace {

enum : fui::ActionId {
  ActionShelf = 1,  // value: a Shelf
  ActionToday,
  ActionSurprise,
  ActionLetters,
  ActionLanguage,
  ActionPick,      // value: a row of the list
  ActionListPage,  // value: -1 or +1
  ActionCategory,
  ActionLetter,  // value: 0 for '#', then A..Z
  ActionTextPage,
  ActionPlay,
  ActionMark,
  ActionRelated,
  ActionOpenRelated,  // value: which
};

constexpr char kPackDir[] = "/linernotes";
constexpr char kIndexPath[] = "/linernotes/index.tsv";
constexpr char kKeptPath[] = "/.crosspoint/linernotes.cfg";
constexpr size_t kIndexCap = 512 * 1024;
constexpr size_t kRecordCap = 16 * 1024;
constexpr uint8_t kCleanAfterPaints = 8;
constexpr int kRowH = 76;
constexpr int kFooterH = 56;

const char* shelfTitle(const uint8_t shelf) {
  constexpr const char* kTitles[] = {"ESSENTIAL", "HIDDEN GEMS", "EXPLORE", "MY MARKS", "RECENTLY OPENED", "BY LETTER"};
  return kTitles[shelf];
}

int measure(void* context, const char* text, const size_t length, const bool heading) {
  char line[256];
  const size_t n = length < sizeof(line) - 1 ? length : sizeof(line) - 1;
  std::memcpy(line, text, n);
  line[n] = '\0';
  return static_cast<GfxRenderer*>(context)->getTextWidth(
      heading ? toybox::kReadingBoldSmallFontId : toybox::kReadingSmallFontId, line);
}

// 0 for anything that is not a letter, 1..26 for A..Z, past a leading "The ".
int letterOf(const char* artist) {
  if (std::strncmp(artist, "The ", 4) == 0 || std::strncmp(artist, "the ", 4) == 0) artist += 4;
  const char c = artist[0];
  if (c >= 'a' && c <= 'z') return c - 'a' + 1;
  if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
  return 0;
}

}  // namespace

std::unique_ptr<Activity> LinerNotesActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LinerNotesActivity>(renderer, mappedInput);
}

bool LinerNotesActivity::spanish() const {
  if (kept_.spanish >= 0) return kept_.spanish != 0;
  return I18N.getLanguage() == Language::ES || I18N.getLanguage() == Language::CA;
}

void LinerNotesActivity::saveKept() {
  const std::string text = kept_.pack();
  HalFile file;
  if (!Storage.openFileForWrite("LNR", kKeptPath, file) || file.write(text.data(), text.size()) != text.size()) {
    LOG_ERR("LNR", "Could not keep the marks");
    return;
  }
  file.flush();
}

void LinerNotesActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  trail_.reserve(8);
  albumTrail_.reserve(8);
  std::string text;
  if (Storage.exists(kKeptPath)) Storage.readFileToString("LNR", kKeptPath, 16384, text);
  kept_.unpack(text);
  text.clear();
  view_ = View::NoPack;
  if (Storage.exists(kIndexPath) && Storage.readFileToString("LNR", kIndexPath, kIndexCap, text) &&
      index_.parse(std::move(text))) {
    view_ = View::Home;
    list_.reserve(index_.albums.size());
    const std::time_t now = std::time(nullptr);
    const long day = now > 1600000000 ? static_cast<long>(now / 86400) : static_cast<long>(millis());
    today_ = static_cast<int>(static_cast<unsigned long>(day) * 2654435761UL % index_.albums.size());
  }
  clean_ = true;
  requestUpdate();
}

void LinerNotesActivity::onExit() {
  Activity::onExit();
  // The radio's memory only comes back whole with a restart, as after the Jukebox.
  if (radioUsed_ && WiFi.getMode() != WIFI_MODE_NULL && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    delay(30);
    silentRestart();
  }
}

int LinerNotesActivity::pageBottom() const {
  return renderer.getScreenHeight() - (mappedInput.hasTouch() ? 8 : lab::kBottomReserve + 6);
}

int LinerNotesActivity::listRows() const {
  const int top = toybox::kChromeHeight + 10 + (shelf_ == Shelf::All ? 56 : 0);
  const int rows = (pageBottom() - kFooterH - 10 - top) / kRowH;
  return rows > 1 ? rows : 1;
}

int LinerNotesActivity::albumLines() const {
  const int top = 10 + toybox::kReadingBoldCut.lineHeight + toybox::kReadingCut.lineHeight +
                  toybox::kReadingSmallCut.lineHeight + 14;
  const int bottom = pageBottom() - 2 * (kFooterH + 8) - toybox::kTileCut.inkHeight - 12;
  const int lines = (bottom - top) / toybox::kReadingSmallCut.lineHeight;
  return lines > 1 ? lines : 1;
}

void LinerNotesActivity::drawFit(const int font, const int x, const int y, const int width, const char* text,
                                 const bool black) const {
  char line[160];
  std::snprintf(line, sizeof(line), "%s", text);
  size_t n = std::strlen(line);
  if (renderer.getTextWidth(font, line) > width) {
    while (n > 1) {
      --n;
      while (n > 1 && (static_cast<unsigned char>(line[n]) & 0xC0) == 0x80) --n;
      std::snprintf(line + n, sizeof(line) - n, "...");
      if (renderer.getTextWidth(font, line) <= width) break;
    }
  }
  renderer.drawText(font, x, y, line, black);
}

void LinerNotesActivity::fillList() {
  list_.clear();
  const int count = static_cast<int>(index_.albums.size());
  if (shelf_ == Shelf::Recent) {
    for (const std::string& id : kept_.recent) {
      const int at = index_.find(id.c_str());
      if (at >= 0) list_.push_back(static_cast<uint16_t>(at));
    }
    return;
  }
  for (int i = 0; i < count; ++i) {
    const ln::Album& a = index_.albums[i];
    bool in = true;
    switch (shelf_) {
      case Shelf::Essential:
        in = (a.categories & ln::kEssentialBit) != 0;
        break;
      case Shelf::Hidden:
        in = (a.categories & ln::kHiddenBits) != 0;
        break;
      case Shelf::All:
        in = shelfValue_ == 0 || (a.categories & (1u << (shelfValue_ - 1))) != 0;
        break;
      case Shelf::Marked:
        in = kept_.markOf(index_.str(a.id)) != ln::Mark::None;
        break;
      case Shelf::Letter:
        in = letterOf(index_.str(a.artist)) == shelfValue_;
        break;
      case Shelf::Recent:
        break;
    }
    if (in) list_.push_back(static_cast<uint16_t>(i));
  }
}

void LinerNotesActivity::openList(const Shelf shelf, const int value) {
  trail_.push_back(static_cast<uint8_t>(view_));
  shelf_ = shelf;
  shelfValue_ = value;
  listTop_ = 0;
  fillList();
  view_ = View::List;
}

bool LinerNotesActivity::loadRecord(const int album) {
  const ln::Album& a = index_.albums[album];
  const int lang = spanish() ? 0 : 1;
  const size_t length = a.length[lang];
  if (length == 0 || length > kRecordCap) return false;
  // One record, read by offset: the pack itself is never held in memory.
  auto buffer = makeUniqueNoThrow<char[]>(length);
  if (!buffer) {
    LOG_ERR("LNR", "OOM: %u bytes", static_cast<unsigned>(length));
    return false;
  }
  char path[40];
  std::snprintf(path, sizeof(path), "%s/%s.txt", kPackDir, lang == 0 ? "es" : "en");
  HalFile file;
  if (!Storage.openFileForRead("LNR", path, file) || !file.seek(a.offset[lang]) ||
      file.read(buffer.get(), length) != static_cast<int>(length)) {
    LOG_ERR("LNR", "Could not read the record at %u", static_cast<unsigned>(a.offset[lang]));
    return false;
  }
  const int width = renderer.getScreenWidth() - 2 * toybox::kMargin;
  ln::layout(buffer.get(), length, lang == 0, width, &measure, &renderer, doc_);
  return true;
}

void LinerNotesActivity::openAlbum(const int album) {
  if (album < 0 || album >= static_cast<int>(index_.albums.size())) return;
  if (!loadRecord(album)) {
    note_ = "THE PACK ON THE CARD COULD NOT BE READ.";
    return;
  }
  trail_.push_back(static_cast<uint8_t>(view_));
  albumTrail_.push_back(static_cast<uint16_t>(album_ < 0 ? album : album_));
  album_ = album;
  lineTop_ = 0;
  note_ = "";
  view_ = View::Album;
  kept_.opened(index_.str(index_.albums[album].id));
  saveKept();
}

void LinerNotesActivity::back() {
  if (trail_.empty()) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  const View from = view_;
  view_ = static_cast<View>(trail_.back());
  trail_.pop_back();
  note_ = "";
  if (from == View::Album && !albumTrail_.empty()) {
    const int previous = albumTrail_.back();
    albumTrail_.pop_back();
    // Back from an album reached through another one: that one's text again.
    if (view_ == View::Related || view_ == View::Album) {
      if (loadRecord(previous)) album_ = previous;
      lineTop_ = 0;
    }
  }
  if (view_ == View::List) fillList();
  clean_ = true;
  requestUpdate();
}

void LinerNotesActivity::play() {
  if (jukebox::net::kDemo || WiFi.status() == WL_CONNECTED) {
    note_ = "ASKING PLEXAMP...";
    playStep_ = 1;
    requestUpdate();
    return;
  }
  radioUsed_ = true;
  WiFi.mode(WIFI_STA);
  {
    // The saved network is joined behind the page; the picker is for when there is none.
    ActivityResultHandler afterWifi = [this](const ActivityResult& result) {
      interactionsReady_ = false;
      clean_ = true;
      if (result.isCancelled) {
        note_ = "NO WI-FI: PLEXAMP IS REACHED OVER YOUR NETWORK.";
      } else {
        note_ = "ASKING PLEXAMP...";
        playStep_ = 1;
      }
      requestUpdate();
    };
    if (autorefresh::joinSaved()) {
      afterWifi(ActivityResult{});
    } else {
      startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput), std::move(afterWifi));
    }
  }
}

void LinerNotesActivity::startAlbum() {
  const char* key = index_.str(index_.albums[album_].plexKey);
  if (key[0] == '\0') {
    note_ = "THIS PACK DOES NOT SAY WHERE THE ALBUM IS IN PLEX.";
    return;
  }
  switch (jukebox::playAlbum(key)) {
    case jukebox::Started::Yes:
      note_ = "PLAYING ON YOUR PLEXAMP.";
      haptic_feedback::touchAction();
      return;
    case jukebox::Started::NotSetUp:
      note_ = "OPEN JUKEBOX ONCE TO SIGN IN AND CHOOSE A PLAYER.";
      break;
    case jukebox::Started::NoServer:
      note_ = "OPEN JUKEBOX AND BROWSE THE LIBRARY ONCE, SO IT LEARNS THE SERVER.";
      break;
    case jukebox::Started::NoQueue:
      note_ = "THE MEDIA SERVER DID NOT FIND THIS ALBUM.";
      break;
    case jukebox::Started::PlayerRefused:
      note_ = "THE PLAYER DID NOT START IT. IS PLEXAMP OPEN?";
      break;
  }
  haptic_feedback::touchAction(true);
}

void LinerNotesActivity::act(const int action, const int value) {
  const int count = static_cast<int>(index_.albums.size());
  switch (action) {
    case ActionShelf:
      openList(static_cast<Shelf>(value));
      break;
    case ActionToday:
      openAlbum(today_);
      break;
    case ActionSurprise:
      if (count > 0) openAlbum(static_cast<int>((static_cast<uint32_t>(millis()) * 2654435761UL >> 8) % count));
      break;
    case ActionLetters:
      trail_.push_back(static_cast<uint8_t>(view_));
      view_ = View::Letters;
      break;
    case ActionLanguage:
      kept_.spanish = spanish() ? 0 : 1;
      saveKept();
      break;
    case ActionPick:
      if (listTop_ + value < static_cast<int>(list_.size())) openAlbum(list_[listTop_ + value]);
      break;
    case ActionListPage: {
      const int next = listTop_ + value * listRows();
      if (next < 0 || next >= static_cast<int>(list_.size())) return;
      listTop_ = next;
      break;
    }
    case ActionCategory:
      shelfValue_ = (shelfValue_ + 1) % (ln::kCategories + 1);
      listTop_ = 0;
      fillList();
      break;
    case ActionLetter:
      openList(Shelf::Letter, value);
      break;
    case ActionTextPage: {
      const int next = lineTop_ + value * albumLines();
      if (next < 0 || next >= static_cast<int>(doc_.lines.size())) return;
      lineTop_ = next;
      break;
    }
    case ActionPlay:
      play();
      return;
    case ActionMark: {
      const char* id = index_.str(index_.albums[album_].id);
      const int next = (static_cast<int>(kept_.markOf(id)) + 1) % static_cast<int>(ln::Mark::Count);
      kept_.setMark(id, static_cast<ln::Mark>(next));
      saveKept();
      break;
    }
    case ActionRelated:
      trail_.push_back(static_cast<uint8_t>(view_));
      view_ = View::Related;
      break;
    case ActionOpenRelated:
      if (value < static_cast<int>(doc_.related.size())) openAlbum(doc_.related[value].album);
      break;
    default:
      return;
  }
  clean_ = clean_ || action != ActionTextPage;
  requestUpdate();
}

void LinerNotesActivity::loop() {
  using Button = MappedInputManager::Button;
  if (playStep_ == 2) {
    playStep_ = 0;
    startAlbum();
    requestUpdate();
    return;
  }
  if (mappedInput.wasReleased(Button::Back)) {
    back();
    return;
  }
  const bool forward = mappedInput.wasReleased(Button::Right) || mappedInput.wasReleased(Button::Down);
  const bool backward = mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Up);
  if (forward || backward) {
    if (view_ == View::List) act(ActionListPage, forward ? 1 : -1);
    if (view_ == View::Album) act(ActionTextPage, forward ? 1 : -1);
    return;
  }
  if (view_ == View::Album && mappedInput.wasReleased(Button::Confirm)) {
    act(ActionPlay, 0);
    return;
  }
  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  const fui::ActionEvent event = interactions_.route(input);
  if (event.action != 0) act(event.action, event.value);
}

void LinerNotesActivity::buildNoPack(toybox::Screen& screen) {
  lab::chrome(screen, "LINER NOTES");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 16;
  lab::value(screen, x, y, inner, "NO NOTES ON THE CARD YET");
  y += toybox::kUiCut.inkHeight + 16;
  constexpr const char* kSteps[] = {
      "LINER NOTES READS A PACK OF NOTES ABOUT THE RECORDS IN YOUR OWN LIBRARY. THE PACK IS YOURS AND IS MADE ON A "
      "COMPUTER, NOT HERE.",
      "1. WRITE THE NOTES AND BUILD THE PACK WITH LN_PACK.PY, IN TOOLS_LOCAL/LINERNOTES OF THE CROSSPLAY SOURCES.",
      "2. COPY THE FOLDER IT MAKES TO THE CARD AS /LINERNOTES. IT HOLDS INDEX.TSV, ES.TXT AND EN.TXT.",
      "3. OPEN LINER NOTES AGAIN.",
  };
  for (const char* step : kSteps) {
    lab::paragraph(screen, x, y, inner, step, 5);
    y += 5 * toybox::kTileCut.lineHeight + 10;
  }
}

void LinerNotesActivity::buildHome(toybox::Screen& screen) {
  lab::chrome(screen, "LINER NOTES");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 10;

  const ln::Album& day = index_.albums[today_];
  const int cardH = 136;
  lab::button(screen, lab::rect(x, y, inner, cardH), "", ActionToday, 0, false, false);
  lab::caption(screen, x + 18, y + 14, inner - 36, "ALBUM OF THE DAY");
  drawFit(toybox::kReadingBoldFontId, x + 18, y + 36, inner - 36, index_.str(day.title));
  char by[120];
  std::snprintf(by, sizeof(by), "%s, %u", index_.str(day.artist), static_cast<unsigned>(day.year));
  drawFit(toybox::kReadingFontId, x + 18, y + 36 + toybox::kReadingBoldCut.lineHeight, inner - 36, by);
  y += cardH + 10;

  int essential = 0;
  int hidden = 0;
  for (const ln::Album& a : index_.albums) {
    essential += (a.categories & ln::kEssentialBit) != 0;
    hidden += (a.categories & ln::kHiddenBits) != 0;
  }
  const int rowH = 62;
  const int counts[] = {essential, hidden, static_cast<int>(index_.albums.size()),
                        static_cast<int>(kept_.marks.size())};
  for (int i = 0; i < 4; ++i) {
    char text[48];
    std::snprintf(text, sizeof(text), "%s  %d", shelfTitle(static_cast<uint8_t>(i)), counts[i]);
    lab::button(screen, lab::rect(x, y, inner, rowH), text, ActionShelf, i, false, false);
    y += rowH + 8;
  }
  const int half = (inner - 8) / 2;
  lab::button(screen, lab::rect(x, y, half, rowH), "RECENT", ActionShelf, static_cast<int>(Shelf::Recent), false, true);
  lab::button(screen, lab::rect(x + half + 8, y, half, rowH), "BY LETTER", ActionLetters, 0, false, true);
  y += rowH + 8;
  lab::button(screen, lab::rect(x, y, half, rowH), "SURPRISE ME", ActionSurprise, 0, true, true);
  lab::button(screen, lab::rect(x + half + 8, y, half, rowH), spanish() ? "NOTES: ESPANOL" : "NOTES: ENGLISH",
              ActionLanguage, 0, false, true);
  if (note_[0] != 0) lab::paragraph(screen, x, y + rowH + 10, inner, note_, 2);
}

void LinerNotesActivity::buildList(toybox::Screen& screen) {
  lab::chrome(screen, shelfTitle(static_cast<uint8_t>(shelf_)));
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 10;
  if (shelf_ == Shelf::All) {
    char text[48];
    std::snprintf(text, sizeof(text), "SHOWING: %s",
                  shelfValue_ == 0 ? "EVERYTHING" : ln::kCategoryNames[shelfValue_ - 1]);
    lab::button(screen, lab::rect(x, y, inner, 46), text, ActionCategory, 0, false, true);
    y += 56;
  }
  const int rows = listRows();
  const int total = static_cast<int>(list_.size());
  if (total == 0) {
    lab::paragraph(
        screen, x, y + 10, inner,
        shelf_ == Shelf::Marked ? "NOTHING MARKED YET. OPEN AN ALBUM AND TAP ITS MARK." : "NOTHING HERE YET.", 3);
  }
  for (int row = 0; row < rows && listTop_ + row < total; ++row) {
    const ln::Album& a = index_.albums[list_[listTop_ + row]];
    lab::button(screen, lab::rect(x, y, inner, kRowH - 8), "", ActionPick, row, false, false);
    drawFit(toybox::kReadingBoldSmallFontId, x + 14, y + 5, inner - 28, index_.str(a.artist));
    char line[140];
    std::snprintf(line, sizeof(line), "%s, %u", index_.str(a.title), static_cast<unsigned>(a.year));
    drawFit(toybox::kReadingSmallFontId, x + 14, y + 5 + toybox::kReadingBoldSmallCut.lineHeight, inner - 28, line);
    y += kRowH;
  }
  const int footY = pageBottom() - kFooterH;
  const int side = 120;
  if (listTop_ > 0) lab::button(screen, lab::rect(x, footY, side, kFooterH), "BACK", ActionListPage, -1, false, true);
  if (listTop_ + rows < total) {
    lab::button(screen, lab::rect(x + inner - side, footY, side, kFooterH), "MORE", ActionListPage, 1, false, true);
  }
  if (total > 0) {
    char page[32];
    std::snprintf(page, sizeof(page), "%d OF %d", listTop_ / rows + 1, (total + rows - 1) / rows);
    lab::caption(screen, x + side, footY + (kFooterH - toybox::kTileCut.inkHeight) / 2, inner - 2 * side, page,
                 fui::TextAlign::Center);
  }
}

void LinerNotesActivity::buildLetters(toybox::Screen& screen) {
  lab::chrome(screen, "BY LETTER");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  uint32_t used = 0;
  for (const ln::Album& a : index_.albums) used |= 1u << letterOf(index_.str(a.artist));
  const int columns = 4;
  const int gap = 8;
  const int cell = (inner - gap * (columns - 1)) / columns;
  const int cellH = 72;
  int placed = 0;
  for (int letter = 0; letter <= 26; ++letter) {
    if ((used & (1u << letter)) == 0) continue;
    char text[2] = {letter == 0 ? '#' : static_cast<char>('A' + letter - 1), '\0'};
    // The label must outlive this frame's build: one static cell per letter.
    static char labels[27][2];
    labels[letter][0] = text[0];
    labels[letter][1] = '\0';
    lab::button(screen,
                lab::rect(x + (placed % columns) * (cell + gap),
                          toybox::kChromeHeight + 10 + (placed / columns) * (cellH + gap), cell, cellH),
                labels[letter], ActionLetter, letter, false, false);
    ++placed;
  }
}

void LinerNotesActivity::buildAlbum(toybox::Screen& screen) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  const ln::Album& a = index_.albums[album_];
  int y = 10;
  drawFit(toybox::kReadingBoldFontId, x, y, inner, index_.str(a.title));
  y += toybox::kReadingBoldCut.lineHeight;
  drawFit(toybox::kReadingFontId, x, y, inner, index_.str(a.artist));
  y += toybox::kReadingCut.lineHeight;
  char meta[120];
  std::snprintf(meta, sizeof(meta), "%u%s%s", static_cast<unsigned>(a.year), index_.str(a.label)[0] != 0 ? " / " : "",
                index_.str(a.label));
  drawFit(toybox::kReadingSmallFontId, x, y, inner, meta);
  y += toybox::kReadingSmallCut.lineHeight + 4;
  renderer.fillRect(x, y, inner, 2, true);
  y += 10;

  const int lines = albumLines();
  const int total = static_cast<int>(doc_.lines.size());
  for (int i = 0; i < lines && lineTop_ + i < total; ++i) {
    const ln::Line& line = doc_.lines[lineTop_ + i];
    if (line.style != ln::Style::Gap) {
      renderer.drawText(
          line.style == ln::Style::Heading ? toybox::kReadingBoldSmallFontId : toybox::kReadingSmallFontId, x, y,
          doc_.text.c_str() + line.start, true);
    }
    y += toybox::kReadingSmallCut.lineHeight;
  }

  const int bottom = pageBottom();
  const int actionsY = bottom - kFooterH;
  const int pagesY = actionsY - kFooterH - 8;
  if (note_[0] != 0) {
    lab::caption(screen, x, pagesY - toybox::kTileCut.inkHeight - 8, inner, note_, fui::TextAlign::Center);
  }
  const int side = 120;
  if (lineTop_ > 0) lab::button(screen, lab::rect(x, pagesY, side, kFooterH), "BACK", ActionTextPage, -1, false, true);
  if (lineTop_ + lines < total) {
    lab::button(screen, lab::rect(x + inner - side, pagesY, side, kFooterH), "MORE", ActionTextPage, 1, true, true);
  }
  char page[32];
  std::snprintf(page, sizeof(page), "%d OF %d", lineTop_ / lines + 1, (total + lines - 1) / lines);
  lab::caption(screen, x + side, pagesY + (kFooterH - toybox::kTileCut.inkHeight) / 2, inner - 2 * side, page,
               fui::TextAlign::Center);

  const int third = (inner - 16) / 3;
  lab::button(screen, lab::rect(x, actionsY, third, kFooterH), "PLAY", ActionPlay, 0, false, true);
  const ln::Mark mark = kept_.markOf(index_.str(a.id));
  lab::button(screen, lab::rect(x + third + 8, actionsY, third, kFooterH),
              mark == ln::Mark::None ? "MARK" : ln::kMarkNames[static_cast<int>(mark)], ActionMark, 0,
              mark != ln::Mark::None, true);
  if (!doc_.related.empty()) {
    lab::button(screen, lab::rect(x + 2 * (third + 8), actionsY, third, kFooterH), "RELATED", ActionRelated, 0, false,
                true);
  }
}

void LinerNotesActivity::buildRelated(toybox::Screen& screen) {
  lab::chrome(screen, "RELATED");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - 2 * toybox::kMargin;
  int y = toybox::kChromeHeight + 10;
  const int count = static_cast<int>(doc_.related.size());
  const int noteLines = 3;
  for (int i = 0; i < count && i < 5; ++i) {
    const ln::Related& r = doc_.related[i];
    if (r.album >= index_.albums.size()) continue;
    const ln::Album& a = index_.albums[r.album];
    lab::button(screen, lab::rect(x, y, inner, 44), "", ActionOpenRelated, i, false, false);
    char name[160];
    std::snprintf(name, sizeof(name), "%s: %s", index_.str(a.artist), index_.str(a.title));
    drawFit(toybox::kReadingBoldSmallFontId, x + 12, y + 6, inner - 24, name);
    y += 48;
    // The note, wrapped by hand in the small reading face.
    const char* text = r.note.c_str();
    size_t left = r.note.size();
    for (int line = 0; line < noteLines && left > 0; ++line) {
      char buffer[200];
      size_t fits = 0;
      size_t at = 0;
      while (at < left && at < sizeof(buffer) - 1) {
        size_t end = at;
        while (end < left && text[end] != ' ') ++end;
        if (end >= sizeof(buffer) - 1) break;
        std::memcpy(buffer, text, end);
        buffer[end] = '\0';
        if (renderer.getTextWidth(toybox::kReadingSmallFontId, buffer) > inner) break;
        fits = end;
        at = end;
        while (at < left && text[at] == ' ') ++at;
      }
      if (fits == 0) break;
      std::memcpy(buffer, text, fits);
      buffer[fits] = '\0';
      if (line == noteLines - 1 && fits < left) {
        drawFit(toybox::kReadingSmallFontId, x, y, inner, text);
      } else {
        renderer.drawText(toybox::kReadingSmallFontId, x, y, buffer, true);
      }
      y += toybox::kReadingSmallCut.lineHeight;
      text += fits;
      left -= fits;
      while (left > 0 && *text == ' ') ++text, --left;
    }
    y += 12;
  }
}

void LinerNotesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::NoPack:
      buildNoPack(screen);
      break;
    case View::Home:
      buildHome(screen);
      break;
    case View::List:
      buildList(screen);
      break;
    case View::Letters:
      buildLetters(screen);
      break;
    case View::Album:
      buildAlbum(screen);
      break;
    case View::Related:
      buildRelated(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "LinerNotes");
  const bool pages = view_ == View::List || view_ == View::Album;
  const auto labels =
      mappedInput.mapLabels("Back", view_ == View::Album ? "Play" : "", pages ? "Back" : "", pages ? "More" : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!clean_ && ++paints_ >= kCleanAfterPaints) clean_ = true;
  if (clean_) paints_ = 0;
  renderer.displayBuffer(clean_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  clean_ = false;
  // The line saying it is asking is on the panel now; the asking happens next pass.
  if (playStep_ == 1) playStep_ = 2;
}
