#include "FrameActivity.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>

#include "../../CrossPointSettings.h"
#include "../../components/UITheme.h"
#include "../Shelf.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxLab.h"
#include "../ui/ToyboxTheme.h"
#include "../wake/WakeJobs.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;

namespace {

enum : fui::ActionId {
  ActionChoose = 1,  // value: an index into the frame's intervals
  ActionPictures,
};

constexpr char kSetDir[] = "/.sleep";
constexpr char kPinned[] = "/sleep.bmp";

// The intervals said in full; the short labels are wakejobs' own.
constexpr const char* kEvery[wakejobs::kFrameChoices] = {"",        "15 MINUTOS", "30 MINUTOS", "HORA",
                                                         "3 HORAS", "12 HORAS",   "24 HORAS"};

}  // namespace

std::unique_ptr<Activity> FrameActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<FrameActivity>(renderer, mappedInput);
}

void FrameActivity::readCard() {
  choice_ = wakejobs::frameChoice();
  changes_ = wakejobs::frameChanges();
  pinned_ = Storage.exists(kPinned);
  inSet_ = 0;
  auto dir = Storage.open(kSetDir);
  if (!dir || !dir.isDirectory()) return;
  char name[96];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(name, sizeof(name));
    if (name[0] != '\0' && name[0] != '.' && FsHelpers::hasBmpExtension(name)) ++inSet_;
  }
}

void FrameActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  readCard();
  requestUpdate();
}

void FrameActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
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
  if (event.action == ActionChoose && event.value >= 0 && event.value < wakejobs::kFrameChoices) {
    if (event.value == choice_) return;
    wakejobs::setFrameChoice(event.value);
    choice_ = event.value;
    changes_ = 0;
    requestUpdate();
  } else if (event.action == ActionPictures) {
    // The set is chosen where the pictures are, and Back there comes back here.
    shelf::openFrom("WALLPAPERS", "MARCO", renderer, mappedInput);
  }
}

void FrameActivity::buildPage(toybox::Screen& screen) {
  lab::chrome(screen, "MARCO");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - toybox::kMargin * 2;
  int y = toybox::kChromeHeight + 18;

  lab::caption(screen, x, y, inner, "CAMBIAR LA FOTO DE REPOSO CADA");
  y += toybox::kTileCut.inkHeight + 12;
  // Seven choices: off on a row of its own, then the six intervals three to a row.
  const int gap = 8;
  const int cellH = 58;
  lab::button(screen, lab::rect(x, y, inner, cellH), "APAGADO", ActionChoose, 0, choice_ == 0, false);
  y += cellH + gap;
  const int cellW = (inner - gap * 2) / 3;
  for (int i = 1; i < wakejobs::kFrameChoices; ++i) {
    const int k = i - 1;
    lab::button(screen, lab::rect(x + (k % 3) * (cellW + gap), y + (k / 3) * (cellH + gap), cellW, cellH),
                wakejobs::frameLabel(i), ActionChoose, i, choice_ == i, false);
  }
  y += 2 * (cellH + gap) + 12;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 18;

  // Where it stands: the set it has to work with, and whether it can run.
  char line[toybox::kIntChars + 32];
  std::snprintf(line, sizeof(line), "%d", inSet_ % 10000);
  lab::caption(screen, x, y, inner / 2, "FOTOS EN EL SET");
  lab::caption(screen, x + inner / 2, y, inner / 2, "CAMBIOS HECHOS");
  y += toybox::kTileCut.inkHeight + 10;
  lab::value(screen, x, y, inner / 2, pinned_ ? "1 FIJA" : line);
  std::snprintf(line, sizeof(line), "%u", static_cast<unsigned>(changes_ % 100000u));
  lab::value(screen, x + inner / 2, y, inner / 2, line);
  y += toybox::kUiCut.inkHeight + 32;

  const bool usesPictures = SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::CUSTOM;
  const char* headline = "LISTO";
  const char* detail = "";
  char ready[96];
  if (choice_ == 0) {
    headline = "APAGADO";
    detail = "LA FOTO SOLO CAMBIA CUANDO DUERMES LA STICKY A MANO. ELIGE UN INTERVALO PARA QUE CAMBIE SOLA.";
  } else if (pinned_) {
    headline = "FALTA UN SET";
    detail = "HAY UNA SOLA FOTO FIJADA, ASI QUE NO HAY NADA QUE ALTERNAR. ELIGE DOS O MAS EN WALLPAPERS.";
  } else if (inSet_ < 2) {
    headline = "FALTA UN SET";
    detail = "EL MARCO NECESITA DOS FOTOS O MAS. ELIGELAS EN WALLPAPERS CON EL BOTON DE ABAJO.";
  } else if (!usesPictures) {
    headline = "REPOSO SIN FOTOS";
    detail = "LA PANTALLA DE REPOSO ESTA EN OTRO MODO. VUELVE A ELEGIR EL SET EN WALLPAPERS PARA QUE USE TUS FOTOS.";
  } else {
    std::snprintf(ready, sizeof(ready), "CAMBIARA CADA %s MIENTRAS LA STICKY DUERME. DESPIERTA NO CAMBIA.",
                  kEvery[choice_]);
    detail = ready;
  }
  lab::label(screen, lab::rect(x, y, inner, toybox::kDisplayCut.inkHeight), headline, toybox::kDisplayFont,
             toybox::kDisplayCut, fui::TextAlign::Left);
  y += toybox::kDisplayCut.inkHeight + 14;
  lab::paragraph(screen, x, y, inner, detail, 4);

  const int buttonH = 60;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  lab::button(screen, lab::rect(x, bottom - buttonH, inner, buttonH), "ELEGIR FOTOS EN WALLPAPERS", ActionPictures, 0);
}

void FrameActivity::render(RenderLock&&) {
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  buildPage(screen);
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Frame");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
