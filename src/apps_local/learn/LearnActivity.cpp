#include "LearnActivity.h"

#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxSerif.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace lr = learn;

namespace {

enum : fui::ActionId {
  ActionToday = 1,
  ActionSubject,  // value: the subject
  ActionLesson,   // value: the nth of the open subject
  ActionOption,   // value: the option
  ActionDone,
};

constexpr char kSavePath[] = "/.crosspoint/aprende.sav";
constexpr int kBottomReserve = 40;
constexpr int kBackSwipeEdge = 28;
constexpr uint8_t kCleanAfterPaints = 8;
constexpr int kBodyLine = 37;
constexpr int kPageFont = toybox::kReadingFontId;  // a lesson is set larger than a list
constexpr int kBodyFont = toybox::kReadingSmallFontId;
constexpr int kBoldFont = toybox::kReadingBoldSmallFontId;
constexpr int kHeadFont = toybox::kReadingBoldFontId;

// A tick, drawn: the pixel faces have none.
void tick(const GfxRenderer& r, const int x, const int y, const bool black = true) {
  r.drawLine(x, y + 9, x + 6, y + 15, 3, black);
  r.drawLine(x + 6, y + 15, x + 18, y + 2, 3, black);
}

}  // namespace

std::unique_ptr<Activity> LearnActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<LearnActivity>(renderer, mappedInput);
}

void LearnActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  load();
  view_ = View::Home;
  cleanNext_ = true;
  requestUpdate();
}

int32_t LearnActivity::today() const {
  const time_t now = time(nullptr);
  return now > 1700000000 ? static_cast<int32_t>(now / 86400) : 0;
}

void LearnActivity::load() {
  progress_ = lr::Progress{};
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("LEARN", kSavePath, file)) return;
  uint8_t bytes[lr::kProgressBytes];
  if (file.read(bytes, sizeof(bytes)) != static_cast<int>(sizeof(bytes))) return;
  lr::unpackProgress(bytes, sizeof(bytes), progress_);
}

void LearnActivity::save() {
  uint8_t bytes[lr::kProgressBytes];
  if (lr::packProgress(progress_, bytes, sizeof(bytes)) == 0) return;
  HalFile file;
  if (!Storage.openFileForWrite("LEARN", kSavePath, file)) return;
  file.write(bytes, sizeof(bytes));
  file.flush();
}

int LearnActivity::linesPerPage() const {
  const int top = toybox::kChromeHeight + 40;
  const int bottom = renderer.getScreenHeight() - kBottomReserve - 34;
  return (bottom - top) / kBodyLine;
}

int LearnActivity::pageCount() const {
  const int per = linesPerPage();
  const int pages = (static_cast<int>(lines_.size()) + per - 1) / per;
  return pages < 1 ? 1 : pages;
}

void LearnActivity::open(const int lesson) {
  if (lesson < 0 || lesson >= lr::lessonCount()) return;
  lesson_ = lesson;
  const lr::Lesson& l = lr::lessonAt(lesson);
  subject_ = l.subject;
  const int width = renderer.getScreenWidth() - toybox::kMargin * 2;
  lines_.clear();
  lines_.reserve(48);
  std::vector<std::string> head;
  toyboxserif::wrap(renderer, kHeadFont, l.title, width, 3, head);
  for (std::string& h : head) lines_.push_back("\t" + h);
  lines_.emplace_back();
  // The idea, a paragraph at a time, then the example under its own label.
  std::string paragraph;
  for (const char* p = l.idea;; ++p) {
    if (*p == '\n' || *p == 0) {
      toyboxserif::wrap(renderer, kPageFont, paragraph.c_str(), width, 0, lines_);
      lines_.emplace_back();
      paragraph.clear();
      if (*p == 0) break;
    } else {
      paragraph.push_back(*p);
    }
  }
  // A heading is never left alone at the foot of a page.
  const int per = linesPerPage();
  while (static_cast<int>(lines_.size()) % per > per - 3) lines_.emplace_back();
  lines_.push_back("\vEJEMPLO");
  toyboxserif::wrap(renderer, kPageFont, l.example, width, 0, lines_);
  page_ = 0;
  chosen_ = -1;
  view_ = View::Read;
  cleanNext_ = true;
}

void LearnActivity::turn(const int by) {
  const int next = page_ + by;
  if (next >= 0 && next < pageCount()) {
    page_ = next;
  } else if (by > 0) {
    view_ = View::Question;
    cleanNext_ = true;
  }
}

void LearnActivity::loop() {
  int pressX = 0;
  int pressY = 0;
  if (mappedInput.wasScreenTouchPressed(pressX, pressY)) pressX_ = pressX;

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Read && mappedInput.wasBackGesture() && pressX_ >= kBackSwipeEdge && page_ > 0) {
      turn(-1);
      requestUpdate();
      return;
    }
    if (view_ == View::Home) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view_ == View::Question && chosen_ < 0) {
      // Back from the question is back to the reading.
      view_ = View::Read;
    } else {
      view_ = View::Home;
    }
    cleanNext_ = true;
    requestUpdate();
    return;
  }

  if (view_ == View::Read) {
    const MappedInputManager::SwipeDir swipe = mappedInput.wasSwipe();
    if (swipe != MappedInputManager::SwipeDir::None) {
      turn(swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up ? 1 : -1);
      requestUpdate();
      return;
    }
  }

  int tapX = 0;
  int tapY = 0;
  if (!mappedInput.wasScreenTapped(tapX, tapY) || !interactionsReady_) return;
  if (view_ == View::Read) {
    turn(tapX < renderer.getScreenWidth() / 3 ? -1 : 1);
    requestUpdate();
    return;
  }
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(tapX);
  input.touchY = static_cast<int16_t>(tapY);
  route(interactions_.route(input));
}

void LearnActivity::route(const fui::ActionEvent& event) {
  switch (event.action) {
    case ActionToday: {
      const int lesson = lr::lessonFor(progress_, today());
      if (lesson < 0) return;
      open(lesson);
      break;
    }
    case ActionSubject:
      if (event.value < 0 || event.value >= lr::kSubjects) return;
      subject_ = static_cast<lr::Subject>(event.value);
      view_ = View::Subject;
      cleanNext_ = true;
      break;
    case ActionLesson: {
      const int lesson = lr::lessonOf(subject_, event.value);
      if (lesson < 0) return;
      open(lesson);
      break;
    }
    case ActionOption:
      if (view_ != View::Question || chosen_ >= 0 || event.value < 0 || event.value > 2) return;
      chosen_ = static_cast<int8_t>(event.value);
      lr::finish(progress_, lesson_, chosen_ == lr::lessonAt(lesson_).answer, today());
      save();
      break;
    case ActionDone:
      view_ = View::Home;
      cleanNext_ = true;
      break;
    default:
      return;
  }
  requestUpdate();
}

void LearnActivity::buildHome(toybox::Screen& screen) {
  lab::chrome(screen, "APRENDE");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;
  char text[64];
  const int32_t day = today();
  const int lesson = lr::lessonFor(progress_, day);

  // The day's lesson, in a frame of its own.
  const int cardH = 250;
  renderer.drawRoundedRect(x, y, inner, cardH, 2, 12, true);
  if (lesson >= 0) {
    const lr::Lesson& l = lr::lessonAt(lesson);
    lab::caption(screen, x + 16, y + 16, inner - 32,
                 lr::doneToday(progress_, day) ? "OTRA MAS, SI QUIERES" : "LA DE HOY");
    lab::caption(screen, x + 16, y + 16, inner - 32, lr::subjectName(l.subject), fui::TextAlign::Right);
    toyboxserif::draw(renderer, kHeadFont, 36, x + 16, y + 16 + toybox::kTileCut.inkHeight + 12, inner - 32, l.title,
                      3);
    lab::caption(screen, x + 16, y + cardH - 60 - 12 - toybox::kTileCut.inkHeight - 2, inner - 32, "5 MINUTOS",
                 fui::TextAlign::Left, fui::Color::DarkGray);
    lab::button(screen, lab::rect(x + 16, y + cardH - 60 - 12, inner - 32, 60), "EMPEZAR", ActionToday, 0, true, false);
  } else {
    toyboxserif::draw(renderer, kHeadFont, 36, x + 16, y + 40, inner - 32, "Las has hecho todas.", 2);
    toyboxserif::draw(renderer, kBodyFont, 28, x + 16, y + 100, inner - 32,
                      "Puedes repasar cualquiera desde su materia. Las nuevas llegan con la siguiente versión.", 4);
  }
  y += cardH + 16;

  const int streak = lr::streakOn(progress_, day);
  std::snprintf(text, sizeof(text), "RACHA %d %s", streak % 100000, streak == 1 ? "DIA" : "DIAS");
  lab::value(screen, x, y, inner, text);
  std::snprintf(text, sizeof(text), "MEJOR %d   HECHAS %d DE %d", progress_.bestStreak % 100000,
                lr::doneCount(progress_) % 1000, lr::lessonCount() % 1000);
  lab::caption(screen, x, y + 6, inner, text, fui::TextAlign::Right, fui::Color::DarkGray);
  y += toybox::kUiCut.inkHeight + 14;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 12;

  // The four subjects, each with how far along it is.
  const int bottom = screen.device().height - kBottomReserve - 8;
  int rowH = (bottom - y) / lr::kSubjects;
  if (rowH > 84) rowH = 84;
  for (int s = 0; s < lr::kSubjects; ++s) {
    const lr::Subject subject = static_cast<lr::Subject>(s);
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionSubject;
    row.value = static_cast<int16_t>(s);
    row.styles = toybox::rowStyles();
    row.styles.normal.borderWidth = 0;
    row.minTouchSize = 0;
    screen.button(row, lab::rect(x, y, inner, rowH - 4));
    const int done = lr::doneIn(progress_, subject);
    const int count = lr::countIn(subject);
    lab::value(screen, x, y + 10, inner, lr::subjectName(subject));
    std::snprintf(text, sizeof(text), "%d / %d", done % 1000, count % 1000);
    lab::caption(screen, x, y + 16, inner, text, fui::TextAlign::Right);
    const int barY = y + 10 + toybox::kUiCut.inkHeight + 12;
    renderer.drawRect(x, barY, inner, 12, 1, true);
    if (count > 0) renderer.fillRect(x, barY, inner * done / count, 12, true);
    y += rowH;
  }
}

void LearnActivity::buildSubject(toybox::Screen& screen) {
  lab::chrome(screen, lr::subjectName(subject_));
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int count = lr::countIn(subject_);
  int y = toybox::kChromeHeight + 10;
  const int bottom = screen.device().height - kBottomReserve - 8;
  int rowH = count > 0 ? (bottom - y) / count : 60;
  if (rowH > 68) rowH = 68;
  char number[toybox::kIntChars + 2];
  for (int i = 0; i < count; ++i) {
    const int lesson = lr::lessonOf(subject_, i);
    fui::ButtonProps row;
    row.label = "";
    row.action = ActionLesson;
    row.value = static_cast<int16_t>(i);
    row.styles = toybox::rowStyles();
    row.styles.normal.borderWidth = 0;
    row.minTouchSize = 0;
    screen.button(row, lab::rect(x, y, inner, rowH - 2));
    std::snprintf(number, sizeof(number), "%d", (i + 1) % 100);
    lab::caption(screen, x, y + (rowH - toybox::kTileCut.inkHeight) / 2 - 2, 28, number, fui::TextAlign::Left,
                 fui::Color::DarkGray);
    const bool done = lr::isDone(progress_, lesson);
    toyboxserif::draw(renderer, done ? kBodyFont : kBoldFont, 26, x + 32, y + (rowH >= 60 ? 4 : 0) + 2, inner - 32 - 34,
                      lr::lessonAt(lesson).title, rowH >= 60 ? 2 : 1);
    if (done) {
      if (lr::wasRight(progress_, lesson)) {
        tick(renderer, x + inner - 24, y + rowH / 2 - 10);
      } else {
        renderer.drawRoundedRect(x + inner - 22, y + rowH / 2 - 9, 16, 16, 2, 8, true);
      }
    }
    y += rowH;
    lab::box(screen, x, y - 2, inner, 1, fui::Paint::dither(fui::Color::DarkGray));
  }
}

void LearnActivity::buildRead(toybox::Screen& screen) {
  lab::chrome(screen, lr::subjectName(subject_));
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int per = linesPerPage();
  const int pages = pageCount();
  int y = toybox::kChromeHeight + 40;
  for (int i = page_ * per; i < static_cast<int>(lines_.size()) && i < (page_ + 1) * per; ++i) {
    const std::string& line = lines_[static_cast<size_t>(i)];
    if (!line.empty() && line[0] == '\t') {
      renderer.drawText(kHeadFont, x, y - 6, line.c_str() + 1, true);
    } else if (!line.empty() && line[0] == '\v') {
      // A label on a black tab.
      const int w = renderer.getTextWidth(toybox::kTileFontId, line.c_str() + 1) + 16;
      renderer.fillRect(x, y + 8, w, 22, true);
      renderer.drawText(
          toybox::kTileFontId, x + 8,
          y + 4 + (22 - toybox::kTileCut.inkHeight) / 2 - (toybox::kTileCut.ascender - toybox::kTileCut.inkHeight),
          line.c_str() + 1, false);
    } else if (!line.empty()) {
      renderer.drawText(kPageFont, x, y, line.c_str(), true);
    }
    y += kBodyLine;
  }
  const int barY = screen.device().height - kBottomReserve - 26;
  renderer.fillRectDither(x, barY, inner, 3, Color::LightGray);
  renderer.fillRect(x, barY, inner * (page_ + 1) / (pages + 1), 3, true);
  char text[40];
  std::snprintf(text, sizeof(text), "PAGINA %d DE %d", (page_ + 1) % 100, pages % 100);
  lab::caption(screen, x, barY + 9, inner, text, fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, x, barY + 9, inner, page_ + 1 < pages ? "SIGUE >" : "LA PREGUNTA >", fui::TextAlign::Right,
               fui::Color::DarkGray);
}

void LearnActivity::buildQuestion(toybox::Screen& screen) {
  lab::chrome(screen, "LA PREGUNTA");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const lr::Lesson& l = lr::lessonAt(lesson_);
  int y = toybox::kChromeHeight + 18;
  y += toyboxserif::draw(renderer, kHeadFont, 36, x, y, inner, l.question, 7) + 18;

  const bool answered = chosen_ >= 0;
  for (int i = 0; i < 3; ++i) {
    std::vector<std::string> lines;
    lines.reserve(3);
    toyboxserif::wrap(renderer, kPageFont, l.options[i], inner - 32 - 34, 3, lines);
    const int h = static_cast<int>(lines.size()) * 34 + 30;
    const bool right = i == l.answer;
    const bool mine = i == chosen_;
    // Answered: the right one goes solid, a wrong choice keeps a heavy frame.
    const bool solid = answered && right;
    if (solid) {
      renderer.fillRoundedRect(x, y, inner, h, 10, Color::Black);
    } else {
      renderer.drawRoundedRect(x, y, inner, h, answered && mine ? 4 : 2, 10, true);
    }
    for (size_t k = 0; k < lines.size(); ++k) {
      renderer.drawText(kPageFont, x + 16, y + 10 + static_cast<int>(k) * 34, lines[k].c_str(), !solid);
    }
    if (solid) tick(renderer, x + inner - 34, y + h / 2 - 9, false);
    if (!answered) {
      fui::ButtonProps option;
      option.label = "";
      option.action = ActionOption;
      option.value = static_cast<int16_t>(i);
      option.styles.explicitlySet = true;
      option.minTouchSize = 0;
      screen.button(option, lab::rect(x, y, inner, h));
    }
    y += h + 10;
  }

  if (!answered) return;
  y += 8;
  lab::value(screen, x, y, inner, chosen_ == l.answer ? "CORRECTO." : "NO ERA ESA.");
  y += toybox::kUiCut.inkHeight + 12;
  toyboxserif::draw(renderer, kBodyFont, 28, x, y, inner, l.why, 7);
  const int footY = screen.device().height - kBottomReserve - 56 - 6;
  lab::button(screen, lab::rect(x, footY, inner, 56), "HECHO", ActionDone, 0, true, false);
}

void LearnActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Home:
      buildHome(screen);
      break;
    case View::Subject:
      buildSubject(screen);
      break;
    case View::Read:
      buildRead(screen);
      break;
    case View::Question:
      buildQuestion(screen);
      break;
  }
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Learn");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (!cleanNext_ && ++fastPaints_ >= kCleanAfterPaints) cleanNext_ = true;
  if (cleanNext_) fastPaints_ = 0;
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}
