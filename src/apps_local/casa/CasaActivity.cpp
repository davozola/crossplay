#include "CasaActivity.h"

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
#include "../ui/ToyboxTheme.h"
#include "CasaStore.h"

namespace fui = freeink::ui;
namespace lab = toyboxlab;

namespace {

enum : fui::ActionId {
  ActionSleepLogging = 1,
};

constexpr uint32_t kMeasureEveryMs = 60000;
// Two readings further apart than this are not joined by a line: the device
// was off, or asleep without the log running.
constexpr uint32_t kJoinWithin = 2700;

// A small ring: the degree sign the pixel faces do not have.
void degree(toybox::Screen& screen, const int x, const int y, const int r, const int stroke) {
  toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r), fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(r - stroke),
               fui::Color::White);
}

int tenthsOf(const int centi) { return centi >= 0 ? (centi + 5) / 10 : -((-centi + 5) / 10); }

}  // namespace

std::unique_ptr<Activity> CasaActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<CasaActivity>(renderer, mappedInput);
}

void CasaActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  log_ = makeUniqueNoThrow<casa::Log>();
  if (!log_) {
    LOG_ERR("CASA", "OOM: %u bytes for the log", static_cast<unsigned>(sizeof(casa::Log)));
    shelf::leave(renderer, mappedInput);
    return;
  }
  casastore::loadLog(*log_);
  sleepLogging_ = casastore::sleepLogging();
  measure();
  requestUpdate();
}

void CasaActivity::measure() {
  lastMeasureMs_ = static_cast<uint32_t>(millis());
  casa::Sample sample;
  hasNow_ = casastore::readNow(sample);
  if (!hasNow_) return;
  now_ = sample;
  if (casa::append(*log_, sample)) casastore::saveLog(*log_);
}

void CasaActivity::loop() {
  if (!log_) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }

  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY) && interactionsReady_) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
    if (interactions_.route(input).action == ActionSleepLogging) {
      sleepLogging_ = !sleepLogging_;
      casastore::setSleepLogging(sleepLogging_);
      requestUpdate();
    }
    return;
  }

  if (static_cast<uint32_t>(millis()) - lastMeasureMs_ >= kMeasureEveryMs) {
    const int logged = log_->count;
    measure();
    // Repaint for a figure that changed or a new point on the lines, not for
    // every reading: the panel should hold still while the room does.
    if (hasNow_ && (tenthsOf(now_.tempCenti) != shownTenths_ || (now_.humCenti + 50) / 100 != shownHum_ ||
                    log_->count != logged)) {
      requestUpdate();
    }
  }
}

void CasaActivity::plot(toybox::Screen& screen, const int x, const int y, const int w, const int h,
                        const bool humidity) {
  const casa::Log& log = *log_;
  const uint32_t end = hasNow_ ? now_.epoch : static_cast<uint32_t>(time(nullptr));
  const uint32_t start = end - casa::kDay;
  const casa::Range range = casa::rangeSince(log, start);
  char a[16];
  char b[40];

  // The title, and the day's low and high beside it.
  lab::caption(screen, x, y, w, humidity ? "HUMEDAD 24 H" : "TEMPERATURA 24 H", fui::TextAlign::Left,
               fui::Color::DarkGray);
  if (range.count > 0) {
    if (humidity) {
      std::snprintf(b, sizeof(b), "%d - %d%%", (range.minHum + 50) / 100, (range.maxHum + 50) / 100);
    } else {
      char lo[12];
      casa::formatTenths(range.minTemp, lo, sizeof(lo));
      casa::formatTenths(range.maxTemp, a, sizeof(a));
      std::snprintf(b, sizeof(b), "%s - %s", lo, a);
    }
    lab::caption(screen, x, y, w, b, fui::TextAlign::Right);
  }

  const int axisW = 40;  // the scale's figures, on the left
  const int top = y + toybox::kTileCut.inkHeight + 12;
  const int labelsH = toybox::kTileCut.inkHeight + 8;
  const int px = x + axisW;
  const int pw = w - axisW;
  const int ph = h - (top - y) - labelsH;
  const int bottom = top + ph;

  const casa::Scale scale =
      humidity ? casa::scaleFor(range.count > 0 ? range.minHum : 4000, range.count > 0 ? range.maxHum : 6000, 10)
               : casa::scaleFor(range.count > 0 ? range.minTemp : 1900, range.count > 0 ? range.maxTemp : 2300, 4);
  std::snprintf(a, sizeof(a), "%d", scale.hi);
  lab::caption(screen, x, top, axisW - 8, a, fui::TextAlign::Right, fui::Color::DarkGray);
  std::snprintf(a, sizeof(a), "%d", scale.lo);
  lab::caption(screen, x, bottom - toybox::kTileCut.inkHeight, axisW - 8, a, fui::TextAlign::Right,
               fui::Color::DarkGray);

  // The frame: a floor, and a dotted line at the top and the middle.
  lab::ink(screen, px, bottom, pw, toybox::kHairline);
  lab::box(screen, px, top, pw, 1, fui::Paint::dither(fui::Color::DarkGray));
  lab::box(screen, px, top + ph / 2, pw, 1, fui::Paint::dither(fui::Color::DarkGray));

  // Every six hours on the clock, a tick and the hour.
  {
    const time_t endT = static_cast<time_t>(end);
    struct tm parts = {};
    localtime_r(&endT, &parts);
    const int intoBlock = (parts.tm_hour % 6) * 3600 + parts.tm_min * 60 + parts.tm_sec;
    for (int k = 0; k < 4; ++k) {
      const int ago = intoBlock + k * 6 * 3600;
      if (ago > static_cast<int>(casa::kDay)) break;
      const int tx = px + static_cast<int>(static_cast<int64_t>(casa::kDay - ago) * (pw - 1) / casa::kDay);
      lab::ink(screen, tx, bottom, toybox::kHairline, 6);
      const int hour = ((parts.tm_hour / 6) * 6 - k * 6 + 48) % 24;
      std::snprintf(a, sizeof(a), "%02dH", hour);
      const int lw = 44;
      int lx = tx - lw / 2;
      if (lx < px) lx = px;
      if (lx + lw > px + pw) lx = px + pw - lw;
      lab::caption(screen, lx, bottom + 10, lw, a, fui::TextAlign::Center, fui::Color::DarkGray);
    }
  }

  if (range.count < 2) {
    lab::paragraph(screen, px + 12, top + ph / 2 - toybox::kTileCut.lineHeight - 6, pw - 24,
                   "TODAVIA NO HAY UN DIA QUE DIBUJAR. LA LINEA CRECE CON CADA LECTURA.", 2);
    return;
  }

  // The line itself.
  const int span = (scale.hi - scale.lo) * 100;
  int prevX = 0;
  int prevY = 0;
  uint32_t prevEpoch = 0;
  for (int i = 0; i < log.count; ++i) {
    const casa::Sample& s = log.samples[i];
    if (s.epoch < start || s.epoch > end) continue;
    const int v = humidity ? static_cast<int>(s.humCenti) : static_cast<int>(s.tempCenti);
    const int sx = px + static_cast<int>(static_cast<int64_t>(s.epoch - start) * (pw - 1) / casa::kDay);
    const int sy = bottom - 2 - static_cast<int>(static_cast<int64_t>(v - scale.lo * 100) * (ph - 4) / span);
    if (prevEpoch != 0 && s.epoch - prevEpoch <= kJoinWithin) {
      renderer.drawLine(prevX, prevY, sx, sy, 3, true);
    } else {
      renderer.fillRect(sx - 1, sy - 1, 3, 3, true);
    }
    prevX = sx;
    prevY = sy;
    prevEpoch = s.epoch;
  }
  // Where it stands now.
  if (prevEpoch != 0) {
    toybox::disc(screen, static_cast<int16_t>(prevX), static_cast<int16_t>(prevY), 6, fui::Color::Black);
    toybox::disc(screen, static_cast<int16_t>(prevX), static_cast<int16_t>(prevY), 3, fui::Color::White);
  }
}

void CasaActivity::buildPage(toybox::Screen& screen) {
  lab::chrome(screen, "CASA");
  const int width = screen.device().width;
  const int x = toybox::kMargin;
  const int inner = width - x * 2;
  const int bottom = screen.device().height - lab::kBottomReserve - 6;
  char a[24];
  char b[72];

  int y = 96;
  if (!hasNow_) {
    lab::label(screen, lab::rect(x, y + 60, inner, toybox::kDisplayCut.inkHeight), "SIN SENSOR", toybox::kDisplayFont,
               toybox::kDisplayCut, fui::TextAlign::Left);
    lab::paragraph(screen, x, y + 60 + toybox::kDisplayCut.inkHeight + 22, inner,
                   "EL SENSOR DE TEMPERATURA NO HA CONTESTADO. SAL Y VUELVE A ENTRAR; SI SIGUE ASI, ESTA PLACA NO LO "
                   "TIENE.",
                   4);
    return;
  }

  // Now: the temperature large on the left, the humidity on the right.
  const int half = inner / 2;
  lab::caption(screen, x, y, half, "TEMPERATURA", fui::TextAlign::Left, fui::Color::DarkGray);
  lab::caption(screen, x + half + 30, y, half - 30, "HUMEDAD", fui::TextAlign::Left, fui::Color::DarkGray);
  y += toybox::kTileCut.inkHeight + 16;
  // The large cut has no slot of its own in this frame, so it is drawn straight.
  const int base = y - (toybox::kLargeCut.ascender - toybox::kLargeCut.inkHeight);
  casa::formatTenths(now_.tempCenti, a, sizeof(a));
  renderer.drawText(toybox::kLargeFontId, x, base, a, true);
  degree(screen, x + renderer.getTextWidth(toybox::kLargeFontId, a) + 16, y + 10, 10, 4);
  std::snprintf(a, sizeof(a), "%d%%", (now_.humCenti + 50) / 100);
  renderer.drawText(toybox::kLargeFontId, x + half + 30, base, a, true);
  y += toybox::kLargeCut.inkHeight + 18;

  // Under them: how the air is, where it would bead, and the last hour.
  const float temp = static_cast<float>(now_.tempCenti) / 100.0f;
  const float hum = static_cast<float>(now_.humCenti) / 100.0f;
  lab::value(screen, x, y, half + 20, casa::airEs(casa::airOf(hum)));
  casa::formatTenths(static_cast<int>(casa::dewPoint(temp, hum) * 100.0f), a, sizeof(a));
  std::snprintf(b, sizeof(b), "ROCIO %s", a);
  lab::value(screen, x + half + 30, y, half - 30, b);
  y += toybox::kUiCut.inkHeight + 12;
  int dt = 0;
  int dh = 0;
  if (casa::changeOver(*log_, now_, 3600, dt, dh)) {
    casa::formatTenths(dt < 0 ? -dt : dt, a, sizeof(a));
    const int dhWhole = (dh < 0 ? -dh + 50 : dh + 50) / 100;
    // The sign of what is printed, so a fall too small to show is not "-0.0".
    std::snprintf(b, sizeof(b), "EN UNA HORA  %s%s  Y  %s%d%%", tenthsOf(dt) < 0 ? "-" : "+", a,
                  dh < 0 && dhWhole > 0 ? "-" : "+", dhWhole);
    lab::caption(screen, x, y, inner, b, fui::TextAlign::Left, fui::Color::DarkGray);
  }
  y += toybox::kTileCut.inkHeight + 14;
  lab::ink(screen, x, y, inner, toybox::kHairline);
  y += 16;

  // The day, twice: what is left of the page between here and the button.
  const int buttonH = 48;
  const int plotsBottom = bottom - buttonH - 16;
  const int plotH = (plotsBottom - y - 14) / 2;
  plot(screen, x, y, inner, plotH, false);
  plot(screen, x, y + plotH + 14, inner, plotH, true);

  lab::button(screen, lab::rect(x, bottom - buttonH, inner, buttonH),
              sleepLogging_ ? "MEDIR TAMBIEN DORMIDA: ACTIVO" : "MEDIR TAMBIEN DORMIDA: APAGADO", ActionSleepLogging, 0,
              sleepLogging_);
}

void CasaActivity::render(RenderLock&&) {
  shownTenths_ = tenthsOf(now_.tempCenti);
  shownHum_ = (now_.humCenti + 50) / 100;
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  toybox::Frame frame(target, device, noInput, interactions_);
  toybox::Screen screen(frame);
  if (log_) buildPage(screen);
  interactionsReady_ = true;
  toybox::reportOverflow(interactions_, "Casa");
  const auto labels = mappedInput.mapLabels("Back", "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
