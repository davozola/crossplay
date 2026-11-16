#include "MysteryActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;
namespace my = mystery;

namespace {

constexpr char kSavePath[] = "/.crosspoint/mystery.sav";

enum : fui::ActionId {
  ActionCase = 1,  // value: case
  ActionIntro,
  ActionSuspect,   // value: suspect
  ActionQuestion,  // value: question
  ActionPlace,     // value: place
  ActionNotebook,
  ActionAccuse,
  ActionName,  // value: the suspect accused
  ActionBack,
  ActionRestart,
};

const fui::Paint kInk = fui::Paint::solid(fui::Color::Black);
const fui::Paint kPaper = fui::Paint::solid(fui::Color::White);

// Two reading cuts on every screen: the 14 in the body slot for what people
// say and what places hold, and the 11 for clues, questions and the notebook.
// The 11 borrows the title slot once the header band has been drawn with it.
struct Prose {
  fui::FontId font;
  int lineHeight;
};
constexpr Prose kBody{toybox::kUiFont, toybox::kReadingCut.lineHeight};
constexpr Prose kFine{toybox::kDisplayFont, toybox::kReadingSmallCut.lineHeight};

void lendFine(void* target) {
  static_cast<fui::GfxRendererTarget*>(target)->setFont(fui::GfxRendererTarget::FONT_TITLE,
                                                        toybox::kReadingSmallFontId);
}

fui::TextStyle proseStyle(const Prose& cut) {
  fui::TextStyle s;
  s.font = cut.font;
  s.align = fui::TextAlign::Left;
  return s;
}

int linesOf(toybox::Screen& screen, const Prose& cut, const int w, const char* text) {
  return static_cast<int>(
      fui::textAreaMeasure(screen.target(), static_cast<int16_t>(w), text, proseStyle(cut), 0).lineCount);
}

// Wrapped prose from (x, y); returns the y under its last line.
int prose(toybox::Screen& screen, const Prose& cut, const int x, const int y, const int w, const char* text) {
  const int lines = linesOf(screen, cut, w, text);
  fui::TextAreaProps props;
  props.text = text;
  props.showCaret = false;
  props.style = proseStyle(cut);
  fui::textArea(screen.frame(), lab::rect(x, y, w, lines * cut.lineHeight + 4), props);
  return y + lines * cut.lineHeight;
}

// The same, in the 14 cut when it fits `room` and the 11 when it does not.
int proseToFit(toybox::Screen& screen, const int x, const int y, const int w, const int room, const char* text) {
  const bool fits = linesOf(screen, kBody, w, text) * kBody.lineHeight <= room;
  return prose(screen, fits ? kBody : kFine, x, y, w, text);
}

int clueBoxHeight(toybox::Screen& screen, const char* text) {
  const int inner = screen.device().width - toybox::kMargin * 2;
  return 14 + toybox::kTileCut.inkHeight + 8 + linesOf(screen, kFine, inner - 28, text) * kFine.lineHeight + 12;
}

// A clue as it goes into the notebook: a ruled box with its label.
int clueBox(toybox::Screen& screen, const int y, const char* text) {
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int h = clueBoxHeight(screen, text);
  lab::box(screen, x, y, inner, h, kInk, 10);
  lab::box(screen, x + 2, y + 2, inner - 4, h - 4, kPaper, 8);
  lab::caption(screen, x + 14, y + 14, inner - 28, "AL CUADERNO");
  prose(screen, kFine, x + 14, y + 14 + toybox::kTileCut.inkHeight + 8, inner - 28, text);
  return y + h;
}

int bottomOf(toybox::Screen& screen) { return screen.device().height - lab::kBottomReserve - 6; }

void backButton(toybox::Screen& screen, const char* text = "VOLVER") {
  const int x = toybox::kMargin;
  lab::button(screen, lab::rect(x, bottomOf(screen) - 46, screen.device().width - x * 2, 46), text, ActionBack, 0);
}

}  // namespace

std::unique_ptr<Activity> MysteryActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MysteryActivity>(renderer, mappedInput);
}

void MysteryActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  loadState();
  requestUpdate();
}

void MysteryActivity::onExit() {
  saveState();
  Activity::onExit();
}

MysteryActivity::View MysteryActivity::parentOf(const View v) const {
  switch (v) {
    case View::Hub:
      return View::Cases;
    case View::Answer:
      return View::Suspect;
    case View::Verdict:
      return verdict_ == my::Verdict::Solved ? View::Cases : View::Hub;
    case View::Cases:
      return View::Cases;
    default:
      return View::Hub;
  }
}

void MysteryActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view_ == View::Cases) {
      saveState();
      shelf::leave(renderer, mappedInput);
      return;
    }
    view_ = parentOf(view_);
    requestUpdate();
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
  const my::Case& c = my::caseAt(case_);
  my::Progress& p = progress_[case_];
  const int v = event.value;
  switch (event.action) {
    case ActionCase:
      if (v < 0 || v >= my::kCases) return;
      case_ = v;
      // A case never opened starts on its briefing.
      view_ = progress_[v].steps == 0 && !progress_[v].solved ? View::Intro : View::Hub;
      break;
    case ActionIntro:
      view_ = View::Intro;
      break;
    case ActionSuspect:
      if (v < 0 || v >= my::kSuspects) return;
      suspect_ = v;
      view_ = View::Suspect;
      break;
    case ActionQuestion:
      if (!my::canAsk(c, p, suspect_, v)) return;
      question_ = v;
      newClue_ = my::ask(c, p, suspect_, v);
      view_ = View::Answer;
      saveState();
      break;
    case ActionPlace:
      if (v < 0 || v >= my::kPlaces) return;
      place_ = v;
      newClue_ = my::examine(c, p, v);
      view_ = View::Place;
      saveState();
      break;
    case ActionNotebook:
      view_ = View::Notebook;
      break;
    case ActionAccuse:
      view_ = View::Accuse;
      break;
    case ActionName:
      if (v < 0 || v >= my::kSuspects) return;
      suspect_ = v;
      verdict_ = my::accuse(c, p, v);
      view_ = View::Verdict;
      saveState();
      break;
    case ActionRestart:
      my::restart(p);
      view_ = View::Intro;
      break;
    case ActionBack:
      view_ = parentOf(view_);
      break;
    default:
      return;
  }
  requestUpdate();
}

void MysteryActivity::buildCases(toybox::Screen& screen) {
  lab::chrome(screen, "MISTERIOS");
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;
  y = prose(screen, kBody, x, y, inner,
            "Tres casos. En cada uno, cuatro sospechosos y tres lugares. Pregunta, mira y, cuando lo tengas, acusa.") +
      18;
  for (int i = 0; i < my::kCases; ++i) {
    const my::Case& c = my::caseAt(i);
    const my::Progress& p = progress_[i];
    const fui::Rect r = lab::rect(x, y, inner, 104);
    lab::button(screen, r, "", ActionCase, i);
    char line[toybox::kIntChars + 12];
    std::snprintf(line, sizeof(line), "CASO %d", i + 1);
    lab::caption(screen, r.x + 18, r.y + 16, inner - 36, line, fui::TextAlign::Left, fui::Color::DarkGray);
    lab::label(screen, lab::rect(r.x + 18, r.y + 38, inner - 36, toybox::kDisplayCut.inkHeight), c.title,
               toybox::kDisplayFont, toybox::kDisplayCut, fui::TextAlign::Left);
    const char* state = p.best ? my::rankName(p.best) : (p.steps ? "EN CURSO" : "NUEVO");
    lab::caption(screen, r.x + 18, r.y + 16, inner - 36, state, fui::TextAlign::Right);
    y += 104 + 10;
  }
}

void MysteryActivity::buildHub(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Progress& p = progress_[case_];
  lab::chrome(screen, c.title);
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int bottom = bottomOf(screen);
  int y = toybox::kChromeHeight + 12;

  lab::button(screen, lab::rect(x, y, inner, 44), "EL CASO", ActionIntro, 0);
  y += 44 + 14;
  lab::caption(screen, x, y, inner, "SOSPECHOSOS", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 8;
  for (int s = 0; s < my::kSuspects; ++s) {
    const my::Suspect& su = c.suspects[s];
    const fui::Rect r = lab::rect(x, y, inner, 62);
    lab::button(screen, r, "", ActionSuspect, s);
    lab::label(screen, lab::rect(r.x + 16, r.y + 11, inner - 32, toybox::kButtonCut.inkHeight), su.name,
               toybox::kTileFont, toybox::kTileCut, fui::TextAlign::Left);
    lab::caption(screen, r.x + 16, r.y + 37, inner - 32, su.role, fui::TextAlign::Left, fui::Color::DarkGray);
    // How much of what they have to say has been heard.
    int asked = 0;
    for (int q = 0; q < my::kQuestions; ++q) asked += my::wasAsked(p, s, q) ? 1 : 0;
    for (int q = 0; q < my::kQuestions; ++q) {
      const int dx = r.x + inner - 22 - (my::kQuestions - 1 - q) * 20;
      toybox::disc(screen, static_cast<int16_t>(dx), static_cast<int16_t>(r.y + 31), 7, fui::Color::Black);
      if (q >= asked)
        toybox::disc(screen, static_cast<int16_t>(dx), static_cast<int16_t>(r.y + 31), 5, fui::Color::White);
    }
    y += 62 + 6;
  }
  y += 8;
  lab::caption(screen, x, y, inner, "LUGARES", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 8;
  for (int pl = 0; pl < my::kPlaces; ++pl) {
    const bool seen = (p.examined >> pl) & 1;
    const fui::Rect r = lab::rect(x, y, inner, 46);
    lab::button(screen, r, c.places[pl].name, ActionPlace, pl);
    toybox::disc(screen, static_cast<int16_t>(r.x + inner - 22), static_cast<int16_t>(r.y + 23), 7, fui::Color::Black);
    if (!seen)
      toybox::disc(screen, static_cast<int16_t>(r.x + inner - 22), static_cast<int16_t>(r.y + 23), 5,
                   fui::Color::White);
    y += 46 + 6;
  }
  const int half = (inner - 8) / 2;
  char notebook[toybox::kIntChars * 2 + 16];
  std::snprintf(notebook, sizeof(notebook), "CUADERNO %d/%d", my::clueCount(p), static_cast<int>(c.clueCount));
  lab::button(screen, lab::rect(x, bottom - 48, half, 48), notebook, ActionNotebook, 0);
  lab::button(screen, lab::rect(x + half + 8, bottom - 48, half, 48), "ACUSAR", ActionAccuse, 0, true);
}

void MysteryActivity::buildIntro(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Progress& p = progress_[case_];
  lab::chrome(screen, c.title);
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  proseToFit(screen, x, toybox::kChromeHeight + 16, inner, bottomOf(screen) - 58 - toybox::kChromeHeight - 16, c.intro);
  const int bottom = bottomOf(screen);
  if (p.steps > 0 || p.solved) {
    const int half = (inner - 8) / 2;
    lab::button(screen, lab::rect(x, bottom - 46, half, 46), "EMPEZAR DE NUEVO", ActionRestart, 0);
    lab::button(screen, lab::rect(x + half + 8, bottom - 46, half, 46), "SEGUIR", ActionBack, 0);
  } else {
    backButton(screen, "INVESTIGAR");
  }
}

void MysteryActivity::buildSuspect(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Progress& p = progress_[case_];
  const my::Suspect& su = c.suspects[suspect_];
  lab::chrome(screen, su.name);
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;
  y = prose(screen, kBody, x, y, inner, su.about) + 16;
  lab::caption(screen, x, y, inner, "PREGUNTAR", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 8;
  for (int q = 0; q < my::kQuestions; ++q) {
    if (!my::canAsk(c, p, suspect_, q)) continue;
    const char* ask = su.questions[q].ask;
    const int lines = linesOf(screen, kFine, inner - 44, ask);
    const int h = lines * kFine.lineHeight + 26;
    const fui::Rect r = lab::rect(x, y, inner, h);
    lab::button(screen, r, "", ActionQuestion, q);
    // A bar down the side of what has been asked already.
    if (my::wasAsked(p, suspect_, q)) lab::box(screen, r.x + 8, r.y + 10, 5, h - 20, kInk, 2);
    prose(screen, kFine, r.x + 24, r.y + 13, inner - 44, ask);
    y += h + 8;
  }
  if (my::hiddenQuestions(c, p, suspect_) > 0) {
    lab::caption(screen, x, y + 6, inner, "QUEDA ALGO POR PREGUNTAR. HACEN FALTA MAS PISTAS.", fui::TextAlign::Left,
                 fui::Color::DarkGray);
  }
  backButton(screen);
}

void MysteryActivity::buildAnswer(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Suspect& su = c.suspects[suspect_];
  const my::Question& q = su.questions[question_];
  lab::chrome(screen, su.name);
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;
  // The question, set off by a rule down its side; then what they say.
  const int top = y;
  y = prose(screen, kFine, x + 16, y, inner - 16, q.ask);
  lab::box(screen, x, top + 4, 4, y - top - 4, kInk, 2);
  y += 16;
  const int box = newClue_ >= 0 ? clueBoxHeight(screen, c.clues[newClue_]) + 18 : 0;
  y = proseToFit(screen, x, y, inner, bottomOf(screen) - 58 - box - y, q.answer) + 18;
  if (newClue_ >= 0) clueBox(screen, y, c.clues[newClue_]);
  backButton(screen);
}

void MysteryActivity::buildPlace(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Place& pl = c.places[place_];
  lab::chrome(screen, pl.name);
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  const int top = toybox::kChromeHeight + 14;
  const int box = newClue_ >= 0 ? clueBoxHeight(screen, c.clues[newClue_]) + 18 : 0;
  const int y = proseToFit(screen, x, top, inner, bottomOf(screen) - 58 - box - top, pl.text) + 18;
  if (newClue_ >= 0) clueBox(screen, y, c.clues[newClue_]);
  backButton(screen);
}

void MysteryActivity::buildNotebook(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Progress& p = progress_[case_];
  lab::chrome(screen, "CUADERNO");
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 16;
  if (my::clueCount(p) == 0) {
    prose(screen, kFine, x, y, inner,
          "Todavía no hay nada apuntado. Las pistas salen de lo que dicen y de lo que hay en cada lugar.");
  }
  for (int k = 0; k < c.clueCount; ++k) {
    if (!my::hasClue(p, k)) continue;
    lab::box(screen, x, y + 9, 8, 8, kInk);
    y = prose(screen, kFine, x + 22, y, inner - 22, c.clues[k]) + 12;
  }
  backButton(screen);
}

void MysteryActivity::buildAccuse(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  lab::chrome(screen, "ACUSAR");
  lendFine(target_);
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;
  y = prose(screen, kFine, x, y, inner,
            "¿Quién fue? Para que la acusación se sostenga hacen falta tres cosas en el cuaderno: un motivo, la "
            "ocasión y una coartada rota. Equivocarse cuesta rango.") +
      18;
  for (int s = 0; s < my::kSuspects; ++s) {
    lab::button(screen, lab::rect(x, y, inner, 54), c.suspects[s].name, ActionName, s);
    y += 54 + 8;
  }
  backButton(screen);
}

void MysteryActivity::buildVerdict(toybox::Screen& screen) {
  const my::Case& c = my::caseAt(case_);
  const my::Progress& p = progress_[case_];
  const int x = toybox::kMargin;
  const int inner = screen.device().width - x * 2;
  int y = toybox::kChromeHeight + 14;
  if (verdict_ == my::Verdict::Solved) {
    lab::chrome(screen, "CASO RESUELTO");
    lendFine(target_);
    lab::caption(screen, x, y, inner, c.suspects[c.culprit].name);
    y += toybox::kTileCut.inkHeight + 12;
    y = proseToFit(screen, x, y, inner, bottomOf(screen) - 58 - 50 - y, c.solution) + 20;
    lab::ink(screen, x, y, inner, toybox::kHairline);
    char line[64];
    std::snprintf(line, sizeof(line), "%d PASOS, %d ERRORES", static_cast<int>(p.steps), static_cast<int>(p.wrong));
    lab::caption(screen, x, y + 14, inner, my::rankName(my::rankOf(p)));
    lab::caption(screen, x, y + 14, inner, line, fui::TextAlign::Right, fui::Color::DarkGray);
    backButton(screen, "OTROS CASOS");
    return;
  }
  if (verdict_ == my::Verdict::WrongSuspect) {
    lab::chrome(screen, "NO FUE");
    lendFine(target_);
    lab::caption(screen, x, y, inner, c.suspects[suspect_].name);
    prose(screen, kBody, x, y + toybox::kTileCut.inkHeight + 12, inner, c.suspects[suspect_].alibi);
  } else {
    lab::chrome(screen, "FALTAN PRUEBAS");
    lendFine(target_);
    const char* text =
        verdict_ == my::Verdict::NoMotive
            ? "El juez te escucha y niega con la cabeza. No ve el motivo: ¿por qué iba a hacerlo? Sin eso no hay caso."
            : (verdict_ == my::Verdict::NoOpportunity
                   ? "El juez te escucha y niega con la cabeza. Motivo hay, pero ¿cómo llegó hasta la víctima? Falta "
                     "probar la ocasión."
                   : "El juez te escucha y niega con la cabeza. Su coartada sigue en pie, y mientras nadie la rompa "
                     "no puede hacer nada.");
    prose(screen, kBody, x, y, inner, text);
  }
  backButton(screen, "SEGUIR INVESTIGANDO");
}

void MysteryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer, toybox::readingFaces());
  target_ = &target;
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  switch (view_) {
    case View::Cases:
      buildCases(screen);
      break;
    case View::Hub:
      buildHub(screen);
      break;
    case View::Intro:
      buildIntro(screen);
      break;
    case View::Suspect:
      buildSuspect(screen);
      break;
    case View::Answer:
      buildAnswer(screen);
      break;
    case View::Place:
      buildPlace(screen);
      break;
    case View::Notebook:
      buildNotebook(screen);
      break;
    case View::Accuse:
      buildAccuse(screen);
      break;
    case View::Verdict:
      buildVerdict(screen);
      break;
  }
  target_ = nullptr;
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Mystery");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

void MysteryActivity::saveState() {
  uint8_t blob[my::kSaveBytes];
  if (my::save(progress_, blob, sizeof(blob)) == 0) return;
  HalFile file;
  if (!Storage.openFileForWrite("MYST", kSavePath, file)) return;
  file.write(blob, sizeof(blob));
  file.flush();
}

void MysteryActivity::loadState() {
  if (!Storage.exists(kSavePath)) return;
  HalFile file;
  if (!Storage.openFileForRead("MYST", kSavePath, file)) return;
  uint8_t blob[my::kSaveBytes];
  const int size = file.read(blob, sizeof(blob));
  if (size > 0) my::restore(progress_, blob, static_cast<size_t>(size));
}
