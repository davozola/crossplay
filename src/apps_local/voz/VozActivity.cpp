#include "VozActivity.h"

#include <ESPmDNS.h>
#include <HalDisplay.h>
#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../../DevMode.h"
#include "../../SilentRestart.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../util/DeviceHostname.h"
#include "../../util/QrUtils.h"
#include "../Shelf.h"
#include "../deck/DeckFonts.h"
#include "../ui/AutoRefresh.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSerif.h"
#include "../ui/ToyboxTokens.h"
#include "VozAudio.h"
#include "VozNet.h"

namespace df = deckfonts;

namespace {

using voz::kNotesDir;
constexpr char kConfigDir[] = "/.crosspoint/voz";
constexpr char kConfigPath[] = "/.crosspoint/voz/voz.cfg";
constexpr char kModelsPath[] = "/.crosspoint/voz/modelos.txt";
constexpr char kMicPath[] = "/.crosspoint/voz/mic.cfg";
constexpr const char* kKeyPaths[] = {"/.crosspoint/voz/gemini.key", "/.crosspoint/voz/openai.key"};
constexpr const char* kServiceNames[] = {"Gemini", "OpenAI"};
constexpr const char* kServiceCaps[] = {"GEMINI", "OPENAI"};

constexpr int kMargin = 28;
constexpr int kListTop = 112;
constexpr int kRowHeight = 84;
constexpr int kRowsPerPage = 6;
constexpr int kButtonY = 700;  // the centre of the record and stop buttons
constexpr int kFooter = 64;
constexpr int kHeadWord = 130;  // the width each word at the head of the list answers to
constexpr int kMaxEntries = 200;
// Before this the device's clock has plainly never been set.
constexpr time_t kClockSetAfter = 1704067200;  // 2024-01-01

using deckfonts::Cut;
constexpr Cut kSmall{toybox::kTileCut.ascender, toybox::kTileCut.inkHeight};
constexpr Cut kTitle{toybox::kSerifTitleCut.ascender, toybox::kSerifTitleCut.inkHeight};
constexpr Cut kTile{toybox::kSerifTileCut.ascender, toybox::kSerifTileCut.inkHeight};
constexpr Cut kReading{toybox::kReadingCut.ascender, toybox::kReadingCut.inkHeight};
constexpr Cut kReadingMid{toybox::kReadingMidCut.ascender, toybox::kReadingMidCut.inkHeight};

void put(const GfxRenderer& r, const int font, const Cut& cut, const int x, const int baseline, const char* s,
         const bool ink = true) {
  r.drawText(font, x, baseline - cut.ascender, s, ink);
}
void putRight(const GfxRenderer& r, const int font, const Cut& cut, const int right, const int baseline,
              const char* s) {
  put(r, font, cut, right - r.getTextWidth(font, s), baseline, s);
}
void putCentre(const GfxRenderer& r, const int font, const Cut& cut, const int centre, const int baseline,
               const char* s, const bool ink = true) {
  put(r, font, cut, centre - r.getTextWidth(font, s) / 2, baseline, s, ink);
}

// `s` cut to `width`, ending in dots when anything had to go.
std::string fitted(const GfxRenderer& r, const int font, const std::string& s, const int width) {
  if (r.getTextWidth(font, s.c_str()) <= width) return s;
  std::string t = s;
  while (!t.empty()) {
    t.pop_back();
    while (!t.empty() && (static_cast<unsigned char>(t.back()) & 0xC0) == 0x80) t.pop_back();
    if (!t.empty()) t.pop_back();  // the lead byte of the character just emptied, or one more letter
    if (r.getTextWidth(font, (t + "...").c_str()) <= width) return t + "...";
  }
  return "...";
}

void disc(const GfxRenderer& r, const int cx, const int cy, const int radius, const bool ink) {
  for (int dy = -radius; dy <= radius; ++dy) {
    int half = 0;
    while ((half + 1) * (half + 1) + dy * dy <= radius * radius) ++half;
    r.fillRect(cx - half, cy + dy, half * 2 + 1, 1, ink);
  }
}

void dottedRow(const GfxRenderer& r, const int y, const int left, const int right) {
  for (int x = left; x < right; x += 6) r.drawPixel(x, y, true);
}

std::string readText(const char* path, const size_t cap) {
  std::string out;
  if (Storage.exists(path)) Storage.readFileToString("VOZ", path, cap, out);
  return out;
}

void writeText(const char* path, const std::string& text) {
  HalFile file;
  if (!Storage.openFileForWrite("VOZ", path, file)) return;
  file.write(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  file.flush();
}

void notePath(const char* stamp, const char* ext, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%s.%s", kNotesDir, stamp, ext);
}

void clockText(const int seconds, char* out, const size_t cap) {
  std::snprintf(out, cap, "%d:%02d", seconds / 60, seconds % 60);
}

}  // namespace

std::unique_ptr<Activity> VozActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<VozActivity>(renderer, mappedInput);
}

// ---------------------------------------------------------------- lifecycle

void VozActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  df::ensure(renderer);
  renderer.setOrientation(GfxRenderer::Orientation::Portrait);
  Storage.mkdir(kNotesDir);
  Storage.mkdir(kConfigDir);
  loadSettings();
  loadKey(voz::Service::Gemini);
  loadKey(voz::Service::OpenAI);
  voz::readModels(readText(kModelsPath, 2048), models_);
  // The setup the microphone test settled on, when it has been run.
  const std::string micCfg = readText(kMicPath, 8);
  voz::audio::useSetup(!micCfg.empty() && micCfg[0] >= '0' && micCfg[0] < '0' + voz::kMicSetups ? micCfg[0] - '0' : -1);
  scan();
  view_ = View::List;
  step_ = Step::Idle;
  cleanNext_ = true;
  requestUpdate();
}

void VozActivity::onExit() {
  // A take in progress is kept, never lost to a Back press.
  if (view_ == View::Recording) {
    voz::audio::stop();
    while (!voz::audio::finished()) delay(10);
    char stamp[16];
    const time_t t = time(nullptr);
    struct tm parts;
    localtime_r(&t, &parts);
    voz::stamp(parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec, stamp,
               sizeof(stamp));
    char path[48];
    notePath(stamp, "wav", path, sizeof(path));
    voz::audio::save(path);
  }
  voz::audio::discard();
  stopPhone();
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  Activity::onExit();
  if (wifiActivated_ && !devmode::holdsRadio()) {
    WiFi.disconnect(false);
    esp_wifi_stop();
    delay(30);
    silentRestart();
  }
}

// ---------------------------------------------------------------- settings

void VozActivity::loadSettings() {
  const std::string cfg = readText(kConfigPath, 16);
  service_ = !cfg.empty() && cfg[0] == '1' ? voz::Service::OpenAI : voz::Service::Gemini;
}

void VozActivity::saveSettings() { writeText(kConfigPath, service_ == voz::Service::OpenAI ? "1\n" : "0\n"); }

void VozActivity::loadKey(const voz::Service service) {
  const int i = static_cast<int>(service);
  const std::string text = readText(kKeyPaths[i], 512);
  if (!voz::tidyKey(text.c_str(), keys_[i], sizeof(keys_[i]))) keys_[i][0] = 0;
}

void VozActivity::saveKey(const voz::Service service) {
  const int i = static_cast<int>(service);
  writeText(kKeyPaths[i], keys_[i]);
}

// ---------------------------------------------------------------- notes

void VozActivity::scan() {
  entries_.clear();
  std::vector<String> names = Storage.listFiles(kNotesDir, 1000);
  std::vector<std::string> stamps;
  stamps.reserve(names.size());
  for (const String& name : names) {
    const char* s = name.c_str();
    const char* slash = std::strrchr(s, '/');
    if (slash) s = slash + 1;
    const size_t n = std::strlen(s);
    if (n == 19 && voz::isStamp(s) && !std::strcmp(s + 15, ".wav")) stamps.emplace_back(s, 15);
  }
  // Newest first; the names sort by when.
  std::sort(stamps.begin(), stamps.end(), [](const std::string& a, const std::string& b) { return a > b; });
  if (stamps.size() > kMaxEntries) stamps.resize(kMaxEntries);
  entries_.reserve(stamps.size());
  for (const std::string& s : stamps) {
    Entry e;
    std::snprintf(e.stamp, sizeof(e.stamp), "%s", s.c_str());
    char path[48];
    notePath(e.stamp, "wav", path, sizeof(path));
    {
      HalFile file;
      if (Storage.openFileForRead("VOZ", path, file)) e.seconds = voz::secondsIn(file.fileSize());
    }
    notePath(e.stamp, "txt", path, sizeof(path));
    voz::Note note;
    if (voz::parse(readText(path, 64 * 1024), note)) {
      e.done = true;
      e.kind = note.kind;
      e.title = note.title;
    } else {
      notePath(e.stamp, "err", path, sizeof(path));
      const std::string err = readText(path, 16);
      e.failure = err.empty() ? 1 : std::atoi(err.c_str());
    }
    entries_.push_back(std::move(e));
  }
  const int pages = std::max(1, (static_cast<int>(entries_.size()) + kRowsPerPage - 1) / kRowsPerPage);
  if (page_ >= pages) page_ = pages - 1;
}

void VozActivity::open(const int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) return;
  opened_ = index;
  confirmErase_ = false;
  detailPage_ = 0;
  note_ = voz::Note{};
  char path[48];
  notePath(entries_[index].stamp, "txt", path, sizeof(path));
  voz::parse(readText(path, 64 * 1024), note_);
  lines_.clear();
  pageStarts_.clear();
  show(View::Detail);
}

void VozActivity::erase(const int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) return;
  for (const char* ext : {"wav", "txt", "err"}) {
    char path[48];
    notePath(entries_[index].stamp, ext, path, sizeof(path));
    if (Storage.exists(path)) Storage.remove(path);
  }
  scan();
  opened_ = -1;
  show(View::List);
}

void VozActivity::show(const View view) {
  view_ = view;
  cleanNext_ = true;
  requestUpdate();
}

void VozActivity::message(const char* headline, const char* detail) {
  headline_ = headline;
  detail_ = detail;
  show(View::Message);
}

// ---------------------------------------------------------------- recording

void VozActivity::beginRecording() {
  if (time(nullptr) < kClockSetAfter) {
    message("Sin hora", "LA STICKY NO TIENE LA HORA PUESTA, Y CADA NOTA SE NOMBRA POR CUANDO SE GRABÓ.");
    return;
  }
  if (!voz::audio::start()) {
    message("Sin micrófono", "ESTE DISPOSITIVO NO PUEDE GRABAR, O NO QUEDA MEMORIA PARA UN MINUTO DE SONIDO.");
    return;
  }
  shownSecond_ = -1;
  show(View::Recording);
}

void VozActivity::endRecording() {
  voz::audio::stop();
  while (!voz::audio::finished()) delay(10);
  if (voz::audio::samples() < voz::kSampleRate / 2) {
    // Half a second is a slip of the finger, not a note.
    voz::audio::discard();
    show(View::List);
    return;
  }
  char stamp[16];
  const time_t t = time(nullptr);
  struct tm parts;
  localtime_r(&t, &parts);
  voz::stamp(parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday, parts.tm_hour, parts.tm_min, parts.tm_sec, stamp,
             sizeof(stamp));
  char path[48];
  notePath(stamp, "wav", path, sizeof(path));
  if (!voz::audio::save(path)) {
    message("No se guardó", "NO SE PUDO ESCRIBIR LA GRABACIÓN EN LA TARJETA.");
    return;
  }
  scan();
  page_ = 0;
  int index = -1;
  for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
    if (!std::strcmp(entries_[i].stamp, stamp)) index = i;
  }
  if (index < 0) {
    show(View::List);
    return;
  }
  if (voz::audio::lastTake().peak < voz::kHeardPeak) {
    // Sent anyway, a silent take comes back as a note the service made up.
    message("No se oyó nada",
            "LA GRABACIÓN QUEDA GUARDADA, PERO ESTÁ EN SILENCIO Y NO SE HA ENVIADO. "
            "PRUEBA EL MICRÓFONO EN AJUSTES.");
    return;
  }
  if (keys_[static_cast<int>(service_)][0] == 0) {
    open(index);  // the detail says the key is missing
    return;
  }
  send(index);
}

// ---------------------------------------------------------------- sending

void VozActivity::send(const int index) {
  if (index < 0 || index >= static_cast<int>(entries_.size())) return;
  sending_ = index;
  wifiActivated_ = true;
  headline_ = "Escuchando";
  detail_ = "UNIÉNDOSE A LA WI-FI";
  show(View::Working);
  if (WiFi.status() == WL_CONNECTED || autorefresh::canJoinAlone()) {
    step_ = Step::Ready;
    return;
  }
  ActivityResultHandler afterWifi = [this](const ActivityResult& result) {
    if (result.isCancelled) {
      step_ = Step::Idle;
      sending_ = -1;
      message("Sin Wi-Fi", "LA NOTA QUEDA GUARDADA. ÁBRELA Y TOCA ENVIAR CUANDO HAYA RED.");
      return;
    }
    step_ = Step::Ready;
    requestUpdate();
  };
  if (autorefresh::joinSaved()) {
    afterWifi(ActivityResult{});
  } else {
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput), std::move(afterWifi));
  }
}

void VozActivity::runSend() {
  step_ = Step::Idle;
  const int index = sending_;
  sending_ = -1;
  if (index < 0 || index >= static_cast<int>(entries_.size())) return;
  if (WiFi.status() != WL_CONNECTED) autorefresh::joinSaved();
  char wav[48];
  notePath(entries_[index].stamp, "wav", wav, sizeof(wav));
  voz::Note note;
  int status = 0;
  const int s = static_cast<int>(service_);
  char txt[48];
  char err[48];
  notePath(entries_[index].stamp, "txt", txt, sizeof(txt));
  notePath(entries_[index].stamp, "err", err, sizeof(err));
  if (voz::net::transcribe(wav, service_, keys_[s], models_, note, status)) {
    writeText(txt, voz::serialise(note));
    if (Storage.exists(err)) Storage.remove(err);
  } else {
    char code[16];
    std::snprintf(code, sizeof(code), "%d\n", status);
    writeText(err, code);
  }
  const std::string stamp = entries_[index].stamp;
  scan();
  for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
    if (stamp == entries_[i].stamp) {
      open(i);
      return;
    }
  }
  show(View::List);
}

void VozActivity::askKey(const voz::Service service) {
  const int i = static_cast<int>(service);
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, i == 0 ? "GEMINI API KEY" : "OPENAI API KEY", "", sizeof(keys_[i]) - 1);
  if (!keyboard) {
    LOG_ERR("VOZ", "OOM: the keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this, service, i](const ActivityResult& result) {
    if (!result.isCancelled) {
      char typed[sizeof(keys_[0])];
      if (voz::tidyKey(std::get<KeyboardResult>(result.data).text.c_str(), typed, sizeof(typed))) {
        std::snprintf(keys_[i], sizeof(keys_[i]), "%s", typed);
        saveKey(service);
      }
    }
    renderer.setOrientation(GfxRenderer::Orientation::Portrait);
    show(View::Settings);
  });
}

// ---------------------------------------------------------------- phone

void VozActivity::openPhone(const View from) {
  phoneFrom_ = from;
  wifiActivated_ = true;
  if (WiFi.status() == WL_CONNECTED) {
    startPhone();
    return;
  }
  headline_ = "Desde el móvil";
  detail_ = "UNIÉNDOSE A LA WI-FI";
  show(View::Working);
  if (autorefresh::canJoinAlone()) {
    // The page must be on the panel before the join blocks.
    requestUpdateAndWait();
    if (autorefresh::joinSaved()) {
      startPhone();
      return;
    }
  }
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult& result) {
                           renderer.setOrientation(GfxRenderer::Orientation::Portrait);
                           if (result.isCancelled) {
                             show(phoneFrom_);
                           } else {
                             startPhone();
                           }
                         });
}

void VozActivity::startPhone() {
  // Developer Mode holds port 80 while its toggle is on, and two binds on one
  // port fail in a way that reads as a broken screen. It yields while this
  // page is up; every way off it goes through stopPhone().
  devmode::pause();
  devPaused_ = true;
  server_ = makeUniqueNoThrow<VozServer>();
  if (!server_ || !server_->begin()) {
    LOG_ERR("VOZ", "the phone page did not start");
    stopPhone();
    message("No se pudo", "EL LECTOR NO PUDO ABRIR SU PÁGINA. PRUEBA OTRA VEZ EN UN MOMENTO.");
    return;
  }
  server_->setHave(keys_[0][0] != 0, keys_[1][0] != 0);
#ifdef SIMULATOR
  const bool mdnsUp = false;
  const std::string dotted = "127.0.0.1";
#else
  MDNS.end();
  const bool mdnsUp = MDNS.begin(devicehost::mdnsName());
  const std::string dotted = std::string(WiFi.localIP().toString().c_str());
#endif
  // The code carries the address, always; the name depends on a responder
  // that can fail to start.
  phoneUrl_ = "http://" + dotted + "/v";
  phoneReadable_ = mdnsUp ? std::string("http://") + devicehost::mdnsName() + ".local/v" : phoneUrl_;
  phoneStatus_ = "ESPERANDO AL MÓVIL";
  show(View::Phone);
}

void VozActivity::stopPhone() {
  if (server_) {
    server_->stop();
    server_.reset();
#ifndef SIMULATOR
    MDNS.end();
#endif
  }
  if (devPaused_) {
    devPaused_ = false;
    devmode::resume();
  }
}

void VozActivity::loopPhone() {
  int x = 0;
  int y = 0;
  if (mappedInput.wasScreenTapped(x, y) || mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    stopPhone();
    show(phoneFrom_);
    return;
  }
  if (!server_) return;
  server_->handleClient();
  voz::Service service = voz::Service::Gemini;
  char key[sizeof(keys_[0])];
  if (!server_->take(service, key, sizeof(key))) return;
  const int i = static_cast<int>(service);
  std::snprintf(keys_[i], sizeof(keys_[i]), "%s", key);
  std::memset(key, 0, sizeof(key));
  saveKey(service);
  phoneStatus_ = service == voz::Service::Gemini ? "CLAVE DE GEMINI GUARDADA" : "CLAVE DE OPENAI GUARDADA";
  cleanNext_ = false;
  requestUpdate();
}

// ---------------------------------------------------------------- microphone

void VozActivity::testMic() {
  micTesting_ = true;
  micTested_ = false;
  show(View::MicTest);
  // The page asking for a voice must be on the panel before the test blocks.
  requestUpdateAndWait();
  micTested_ = voz::audio::probe(micLevels_, micOpened_);
  micBest_ = micTested_ ? voz::bestSetup(micLevels_, micOpened_, voz::kMicSetups) : -1;
  if (micBest_ >= 0) {
    voz::audio::useSetup(micBest_);
    writeText(kMicPath, std::string(1, static_cast<char>('0' + micBest_)));
  }
  micTesting_ = false;
  show(View::MicTest);
}

// ---------------------------------------------------------------- input

void VozActivity::loop() {
  if (step_ == Step::Ready) {
    step_ = Step::Running;
    detail_ = service_ == voz::Service::Gemini ? "GEMINI ESCRIBE LA NOTA" : "OPENAI ESCRIBE LA NOTA";
    // The page must be on the panel before the request blocks.
    cleanNext_ = false;
    requestUpdateAndWait();
    return;
  }
  if (step_ == Step::Running) {
    runSend();
    return;
  }

  switch (view_) {
    case View::Recording: {
      const int second = static_cast<int>(voz::audio::samples() / voz::kSampleRate);
      int x = 0;
      int y = 0;
      if (voz::audio::finished() || mappedInput.wasScreenTapped(x, y) ||
          mappedInput.wasReleased(MappedInputManager::Button::Back) ||
          mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
        endRecording();
        return;
      }
      if (second != shownSecond_) {
        shownSecond_ = second;
        requestUpdate();
      }
      return;
    }
    case View::Working:
      return;
    case View::Message: {
      int x = 0;
      int y = 0;
      if (mappedInput.wasScreenTapped(x, y) || mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        show(View::List);
      }
      return;
    }
    case View::Detail:
      loopDetail();
      return;
    case View::Settings:
      loopSettings();
      return;
    case View::Phone:
      loopPhone();
      return;
    case View::MicTest: {
      int x = 0;
      int y = 0;
      if (mappedInput.wasScreenTapped(x, y) || mappedInput.wasReleased(MappedInputManager::Button::Back)) {
        show(View::Settings);
      }
      return;
    }
    case View::List:
      loopList();
      return;
  }
}

void VozActivity::loopList() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  const int pages = std::max(1, (static_cast<int>(entries_.size()) + kRowsPerPage - 1) / kRowsPerPage);
  const auto turn = [this, pages](const int by) {
    const int next = page_ + by;
    if (next < 0 || next >= pages) return;
    page_ = next;
    requestUpdate();
  };
  switch (mappedInput.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
    case MappedInputManager::SwipeDir::Up:
      turn(1);
      return;
    case MappedInputManager::SwipeDir::Right:
    case MappedInputManager::SwipeDir::Down:
      turn(-1);
      return;
    default:
      break;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    turn(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    turn(-1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    beginRecording();
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  const int w = renderer.getScreenWidth();
  // The head of the page: MÓVIL, then AJUSTES at the edge.
  if (y < kListTop - 8 && x > w - kHeadWord) {
    show(View::Settings);
    return;
  }
  if (y < kListTop - 8 && x > w - 2 * kHeadWord) {
    openPhone(View::List);
    return;
  }
  if (y >= kButtonY - 80 && x > w / 2 - 130 && x < w / 2 + 130) {
    beginRecording();
    return;
  }
  if (y >= kListTop && y < kListTop + kRowsPerPage * kRowHeight) {
    const int index = page_ * kRowsPerPage + (y - kListTop) / kRowHeight;
    if (index < static_cast<int>(entries_.size())) open(index);
  }
}

void VozActivity::loopDetail() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    show(View::List);
    return;
  }
  const int pages = std::max(1, static_cast<int>(pageStarts_.size()));
  const auto turn = [this, pages](const int by) {
    const int next = detailPage_ + by;
    if (next < 0 || next >= pages) return;
    detailPage_ = next;
    confirmErase_ = false;
    requestUpdate();
  };
  switch (mappedInput.wasSwipe()) {
    case MappedInputManager::SwipeDir::Left:
    case MappedInputManager::SwipeDir::Up:
      turn(1);
      return;
    case MappedInputManager::SwipeDir::Right:
    case MappedInputManager::SwipeDir::Down:
      turn(-1);
      return;
    default:
      break;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    turn(1);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    turn(-1);
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  const int w = renderer.getScreenWidth();
  const int h = renderer.getScreenHeight();
  if (y >= h - kFooter) {
    if (x < w / 3) {
      if (confirmErase_) {
        erase(opened_);
      } else {
        confirmErase_ = true;
        requestUpdate();
      }
    } else if (x > w * 2 / 3) {
      if (keys_[static_cast<int>(service_)][0] == 0) {
        show(View::Settings);
      } else {
        send(opened_);
      }
    }
    return;
  }
  // The lower half of the page turns forward, the upper half back.
  turn(y > h / 2 ? 1 : -1);
}

void VozActivity::loopSettings() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    show(View::List);
    return;
  }
  int x = 0;
  int y = 0;
  if (!mappedInput.wasScreenTapped(x, y)) return;
  if (y >= 150 && y < 245) {
    service_ = voz::Service::Gemini;
  } else if (y >= 245 && y < 340) {
    service_ = voz::Service::OpenAI;
  } else if (y >= 420 && y < 500) {
    askKey(voz::Service::Gemini);
    return;
  } else if (y >= 500 && y < 580) {
    askKey(voz::Service::OpenAI);
    return;
  } else if (y >= 594 && y < 690) {
    openPhone(View::Settings);
    return;
  } else if (y >= 692 && y < 768) {
    testMic();
    return;
  } else {
    return;
  }
  saveSettings();
  requestUpdate();
}

// ---------------------------------------------------------------- drawing

void VozActivity::render(RenderLock&&) {
  renderer.setClipRect(0, 0, renderer.getScreenWidth(), renderer.getScreenHeight());
  renderer.clearScreen();
  switch (view_) {
    case View::List:
      drawList();
      break;
    case View::Phone:
      drawPhone();
      break;
    case View::MicTest:
      drawMicTest();
      break;
    case View::Recording:
      drawRecording();
      break;
    case View::Working:
      drawWorking();
      break;
    case View::Detail:
      drawDetail();
      break;
    case View::Settings:
      drawSettings();
      break;
    case View::Message:
      drawMessage();
      break;
  }
  renderer.displayBuffer(cleanNext_ ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanNext_ = false;
}

void VozActivity::drawList() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, "Voz");
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin, 60, "AJUSTES");
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin - kHeadWord, 60, "MÓVIL");
  char line[64];
  const int count = static_cast<int>(entries_.size());
  std::snprintf(line, sizeof(line), "%d %s · %s", count, count == 1 ? "NOTA" : "NOTAS",
                kServiceCaps[static_cast<int>(service_)]);
  put(r, toybox::kTileFontId, kSmall, kMargin, 94, line);
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);

  if (entries_.empty()) {
    putCentre(r, df::kItalicSmallFontId, df::kItalicSmallCut, w / 2, 330, "Ninguna nota todavía");
    putCentre(r, toybox::kTileFontId, kSmall, w / 2, 368, "TOCA GRABAR Y HABLA");
  }
  const int first = page_ * kRowsPerPage;
  for (int row = 0; row < kRowsPerPage && first + row < count; ++row) {
    const Entry& e = entries_[first + row];
    const int y = kListTop + row * kRowHeight;
    char when[32];
    voz::when(e.stamp, when, sizeof(when));
    char length[12];
    clockText(e.seconds, length, sizeof(length));
    const char* tag = e.done ? voz::kindTag(e.kind) : (e.failure == 1 ? "SIN ENVIAR" : "ERROR");
    std::snprintf(line, sizeof(line), "%s · %s · %s", tag, when, length);
    put(r, toybox::kTileFontId, kSmall, kMargin, y + 26, line);
    std::string title;
    if (e.done) {
      title = e.title;
    } else if (e.failure == 1) {
      title = "Sin transcribir";
    } else {
      title = voz::failure(e.failure);
      // The failures are written in capitals for the tag line; here a sentence.
      std::string sentence;
      bool initial = true;
      for (size_t i = 0; i < title.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(title[i]);
        if (c == 0xC3 && i + 1 < title.size()) {
          unsigned char next = static_cast<unsigned char>(title[i + 1]);
          if (!initial && next >= 0x80 && next <= 0x9E) next = static_cast<unsigned char>(next + 0x20);
          sentence += static_cast<char>(c);
          sentence += static_cast<char>(next);
          ++i;
        } else {
          sentence += static_cast<char>(!initial && c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        }
        initial = false;
      }
      title = sentence;
    }
    put(r, toybox::kSerifTileFontId, kTile, kMargin, y + 62,
        fitted(r, toybox::kSerifTileFontId, title, w - 2 * kMargin).c_str());
    if (row + 1 < kRowsPerPage && first + row + 1 < count) dottedRow(r, y + kRowHeight - 4, kMargin, w - kMargin);
  }
  const int pages = std::max(1, (count + kRowsPerPage - 1) / kRowsPerPage);
  if (pages > 1) {
    std::snprintf(line, sizeof(line), "%d / %d", page_ + 1, pages);
    putRight(r, toybox::kTileFontId, kSmall, w - kMargin, kListTop + kRowsPerPage * kRowHeight + 14, line);
  }

  // The record button: a ring and a dot, the shape every recorder has.
  disc(r, w / 2, kButtonY, 52, true);
  disc(r, w / 2, kButtonY, 46, false);
  disc(r, w / 2, kButtonY, 30, true);
  putCentre(r, toybox::kTileFontId, kSmall, w / 2, kButtonY + 86, "GRABAR");
}

void VozActivity::drawRecording() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, "Grabando");
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);
  const int second = static_cast<int>(voz::audio::samples() / voz::kSampleRate);
  char clock[12];
  clockText(second, clock, sizeof(clock));
  putCentre(r, df::kNumeralFontId, df::kNumeralCut, w / 2, 380, clock);
  // How much of the minute is gone.
  const int barW = w - 2 * kMargin;
  r.fillRect(kMargin, 430, barW, 1, true);
  r.fillRect(kMargin, 427, barW * std::min(second, voz::kMaxSeconds) / voz::kMaxSeconds, 7, true);
  put(r, toybox::kTileFontId, kSmall, kMargin, 462, "0:00");
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin, 462, "1:00");
  putCentre(r, toybox::kTileFontId, kSmall, w / 2, 520, "HABLA CERCA DE LA PANTALLA");
  // What the microphone hears right now: a bar that stays empty is a
  // microphone that is not listening.
  put(r, toybox::kTileFontId, kSmall, kMargin, 578, "NIVEL");
  const int levelX = kMargin + 70;
  const int levelW = w - kMargin - levelX;
  r.drawRect(levelX, 562, levelW, 18, 1, true);
  r.fillRect(levelX, 562, levelW * voz::levelBar(voz::audio::level()) / 100, 18, true);

  r.fillRect(w / 2 - 52, kButtonY - 52, 104, 104, true);
  r.fillRect(w / 2 - 18, kButtonY - 18, 36, 36, false);
  putCentre(r, toybox::kTileFontId, kSmall, w / 2, kButtonY + 86, "TOCA PARA PARAR");
}

void VozActivity::drawWorking() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, headline_);
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);
  put(r, toybox::kTileFontId, kSmall, kMargin, 150, detail_);
  if (sending_ >= 0 && sending_ < static_cast<int>(entries_.size())) {
    char line[48];
    char length[12];
    clockText(entries_[sending_].seconds, length, sizeof(length));
    std::snprintf(line, sizeof(line), "%s DE AUDIO", length);
    put(r, toybox::kTileFontId, kSmall, kMargin, 176, line);
  }
  put(r, toybox::kTileFontId, kSmall, kMargin, 202, "TARDA UNOS SEGUNDOS");
}

void VozActivity::drawPhone() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, "Desde el móvil");
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);
  put(r, toybox::kTileFontId, kSmall, kMargin, 140, "ESCANEA EL CÓDIGO EN LA MISMA WI-FI PARA LEER");
  put(r, toybox::kTileFontId, kSmall, kMargin, 164, "Y GUARDAR LAS NOTAS O PEGAR UNA CLAVE.");

  constexpr int kCode = 300;
  constexpr int kCodeTop = 200;
  QrUtils::drawQrCode(renderer, Rect{(w - kCode) / 2, kCodeTop, kCode, kCode}, phoneUrl_);

  // The one string somebody may have to type, so it is set to be read.
  const std::string address = fitted(r, toybox::kSerifTileFontId, phoneReadable_, w - 2 * kMargin);
  put(r, toybox::kSerifTileFontId, kTile, (w - r.getTextWidth(toybox::kSerifTileFontId, address.c_str())) / 2,
      kCodeTop + kCode + 52, address.c_str());
  dottedRow(r, kCodeTop + kCode + 80, kMargin, w - kMargin);

  put(r, toybox::kTileFontId, kSmall, kMargin, kCodeTop + kCode + 116, phoneStatus_);
  for (int i = 0; i < 2; ++i) {
    char hint[32];
    voz::keyHint(keys_[i], hint, sizeof(hint));
    char line[64];
    std::snprintf(line, sizeof(line), "%s: %s", kServiceCaps[i], hint);
    put(r, toybox::kTileFontId, kSmall, kMargin, kCodeTop + kCode + 150 + i * 24, line);
  }
  put(r, toybox::kTileFontId, kSmall, kMargin, r.getScreenHeight() - 40, "TOCA PARA CERRAR LA PÁGINA");
}

void VozActivity::drawMicTest() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, "Micrófono");
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);
  if (micTesting_) {
    put(r, toybox::kTileFontId, kSmall, kMargin, 150, "HABLA SIN PARAR, CERCA DE LA PANTALLA,");
    put(r, toybox::kTileFontId, kSmall, kMargin, 176, "HASTA QUE CAMBIE ESTA PÁGINA.");
    put(r, toybox::kTileFontId, kSmall, kMargin, 228, "TARDA UNOS OCHO SEGUNDOS");
    return;
  }
  if (!micTested_) {
    put(r, toybox::kTileFontId, kSmall, kMargin, 150, "ESTE APARATO NO TIENE MICRÓFONO.");
    put(r, toybox::kTileFontId, kSmall, kMargin, r.getScreenHeight() - 40, "TOCA PARA VOLVER");
    return;
  }
  put(r, df::kItalicSmallFontId, df::kItalicSmallCut, kMargin, 160, micBest_ >= 0 ? "Te oye" : "No oye nada");
  put(r, toybox::kTileFontId, kSmall, kMargin, 196,
      micBest_ >= 0 ? "SE QUEDA CON EL AJUSTE MARCADO." : "NINGÚN AJUSTE HA OÍDO UNA VOZ.");

  // Every setup and what it heard, so a test that fails still says something.
  put(r, toybox::kTileFontId, kSmall, kMargin, 250, "AJUSTE");
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin - 110, 250, "PICO");
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin, 250, "MEDIA");
  dottedRow(r, 262, kMargin, w - kMargin);
  for (int i = 0; i < voz::kMicSetups; ++i) {
    const int base = 296 + i * 40;
    char name[24];
    voz::micSetupName(voz::micSetup(i), name, sizeof(name));
    if (i == micBest_) r.fillRect(kMargin, base - 14, 12, 12, true);
    put(r, toybox::kTileFontId, kSmall, kMargin + 24, base, name);
    char number[16];
    if (micOpened_[i]) {
      std::snprintf(number, sizeof(number), "%d", micLevels_[i].peak);
      putRight(r, toybox::kTileFontId, kSmall, w - kMargin - 110, base, number);
      std::snprintf(number, sizeof(number), "%d", micLevels_[i].rms);
      putRight(r, toybox::kTileFontId, kSmall, w - kMargin, base, number);
    } else {
      putRight(r, toybox::kTileFontId, kSmall, w - kMargin, base, "NO ABRE");
    }
  }
  put(r, toybox::kTileFontId, kSmall, kMargin, r.getScreenHeight() - 40, "TOCA PARA VOLVER");
}

void VozActivity::drawMessage() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, headline_);
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);
  std::vector<std::string> lines;
  toyboxserif::wrap(r, toybox::kTileFontId, detail_, w - 2 * kMargin, 8, lines);
  int base = 150;
  for (const std::string& l : lines) {
    put(r, toybox::kTileFontId, kSmall, kMargin, base, l.c_str());
    base += 26;
  }
  put(r, toybox::kTileFontId, kSmall, kMargin, base + 26, "TOCA PARA VOLVER");
}

// The note as lines, then cut into pages that fit between the head and the
// footer. The first page also carries the title.
void VozActivity::layoutDetail() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  const int width = w - 2 * kMargin;
  lines_.clear();
  pageStarts_.clear();
  const Entry& e = entries_[opened_];
  const auto add = [this](const int font, const int height, std::string text) {
    lines_.push_back(Line{font, height, std::move(text)});
  };
  const auto block = [&](const int font, const int height, const std::string& text) {
    std::vector<std::string> wrapped;
    // Paragraph by paragraph: the transcript keeps its own breaks.
    size_t at = 0;
    while (at <= text.size()) {
      size_t nl = text.find('\n', at);
      if (nl == std::string::npos) nl = text.size();
      const std::string para = text.substr(at, nl - at);
      wrapped.clear();
      if (!para.empty()) toyboxserif::wrap(r, font, para.c_str(), width, 0, wrapped);
      for (std::string& l : wrapped) add(font, height, std::move(l));
      if (nl < text.size()) add(font, height / 2, "");
      at = nl + 1;
    }
  };
  if (e.done) {
    if (!note_.summary.empty()) {
      add(toybox::kTileFontId, 30, "RESUMEN");
      block(toybox::kReadingFontId, 34, note_.summary);
      add(toybox::kTileFontId, 22, "");
    }
    add(toybox::kTileFontId, 30, "LO QUE DIJISTE");
    block(toybox::kReadingMidFontId, 31, note_.transcript);
  } else {
    const int failure = e.failure;
    const char* why = failure == 1 ? "TODAVÍA NO SE HA ENVIADO." : voz::failure(failure);
    add(toybox::kTileFontId, 30, why);
    const char* what = keys_[static_cast<int>(service_)][0] == 0
                           ? "Falta la clave del servicio. Añádela en Ajustes y vuelve a enviar la nota."
                           : "La grabación está guardada en la tarjeta. Toca Enviar para que se transcriba.";
    block(toybox::kReadingFontId, 34, what);
  }

  // Pages: the first starts under the title, the rest under the head line.
  std::vector<std::string> titleLines;
  if (e.done) toyboxserif::wrap(r, toybox::kSerifTitleFontId, note_.title.c_str(), width, 2, titleLines);
  const int titleSpace = e.done ? static_cast<int>(titleLines.size()) * 48 + 30 : 0;
  const int bottom = r.getScreenHeight() - kFooter - 16;
  int y = 76 + titleSpace;
  pageStarts_.push_back(0);
  for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
    if (y + lines_[i].height > bottom) {
      pageStarts_.push_back(i);
      y = 76;
    }
    y += lines_[i].height;
  }
}

void VozActivity::drawDetail() {
  if (opened_ < 0 || opened_ >= static_cast<int>(entries_.size())) return;
  if (pageStarts_.empty()) layoutDetail();
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  const int h = r.getScreenHeight();
  const Entry& e = entries_[opened_];
  char when[32];
  voz::when(e.stamp, when, sizeof(when));
  char length[12];
  clockText(e.seconds, length, sizeof(length));
  char head[64];
  std::snprintf(head, sizeof(head), "%s · %s · %s", e.done ? voz::kindTag(e.kind) : "SIN TRANSCRIBIR", when, length);
  put(r, toybox::kTileFontId, kSmall, kMargin, 46, head);
  r.fillRect(kMargin, 58, w - 2 * kMargin, 2, true);

  int y = 76;
  if (detailPage_ == 0 && e.done) {
    std::vector<std::string> titleLines;
    toyboxserif::wrap(r, toybox::kSerifTitleFontId, note_.title.c_str(), w - 2 * kMargin, 2, titleLines);
    for (const std::string& t : titleLines) {
      put(r, toybox::kSerifTitleFontId, kTitle, kMargin, y + kTitle.cap + 4, t.c_str());
      y += 48;
    }
    y += 30;
  }
  const int pages = static_cast<int>(pageStarts_.size());
  const int from = pageStarts_[detailPage_];
  const int to = detailPage_ + 1 < pages ? pageStarts_[detailPage_ + 1] : static_cast<int>(lines_.size());
  for (int i = from; i < to; ++i) {
    const Line& l = lines_[i];
    if (!l.text.empty()) {
      const Cut cut =
          l.font == toybox::kTileFontId ? kSmall : (l.font == toybox::kReadingMidFontId ? kReadingMid : kReading);
      put(r, l.font, cut, kMargin, y + l.height - (l.font == toybox::kTileFontId ? 10 : 9), l.text.c_str());
    }
    y += l.height;
  }

  // The footer: erase, the page, send.
  r.fillRect(kMargin, h - kFooter, w - 2 * kMargin, 1, true);
  const int base = h - kFooter / 2 + kSmall.cap / 2;
  put(r, toybox::kTileFontId, kSmall, kMargin, base, confirmErase_ ? "¿BORRAR? TOCA OTRA VEZ" : "BORRAR");
  if (pages > 1 && !confirmErase_) {
    char page[16];
    std::snprintf(page, sizeof(page), "%d / %d", detailPage_ + 1, pages);
    putCentre(r, toybox::kTileFontId, kSmall, w / 2, base, page);
  }
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin, base, e.done ? "REENVIAR" : "ENVIAR");
}

void VozActivity::drawSettings() {
  const GfxRenderer& r = renderer;
  const int w = r.getScreenWidth();
  put(r, df::kItalicFontId, df::kItalicCut, kMargin, 28 + df::kItalicCut.cap, "Ajustes");
  r.fillRect(kMargin, 104, w - 2 * kMargin, 2, true);

  put(r, toybox::kTileFontId, kSmall, kMargin, 140, "QUIÉN ESCRIBE LAS NOTAS");
  const char* const notes[] = {"GRATIS PARA EMPEZAR · UNA PETICIÓN", "DE PAGO · DOS PETICIONES"};
  for (int i = 0; i < 2; ++i) {
    const int top = 160 + i * 95;
    const bool chosen = static_cast<int>(service_) == i;
    r.drawRect(kMargin, top + 10, 26, 26, 2, true);
    if (chosen) r.fillRect(kMargin + 6, top + 16, 14, 14, true);
    put(r, toybox::kSerifTitleFontId, kTitle, kMargin + 46, top + 10 + kTitle.cap - 4, kServiceNames[i]);
    put(r, toybox::kTileFontId, kSmall, kMargin + 46, top + 70, notes[i]);
  }
  dottedRow(r, 352, kMargin, w - kMargin);

  put(r, toybox::kTileFontId, kSmall, kMargin, 400, "CLAVES");
  for (int i = 0; i < 2; ++i) {
    const int top = 420 + i * 80;
    put(r, toybox::kSerifTileFontId, kTile, kMargin, top + 34, kServiceNames[i]);
    char hint[32];
    voz::keyHint(keys_[i], hint, sizeof(hint));
    put(r, toybox::kTileFontId, kSmall, kMargin, top + 62, hint);
    putRight(r, toybox::kTileFontId, kSmall, w - kMargin, top + 34, keys_[i][0] ? "CAMBIAR" : "AÑADIR");
  }
  dottedRow(r, 592, kMargin, w - kMargin);

  // A key is forty characters or more: the phone pastes it.
  put(r, toybox::kSerifTileFontId, kTile, kMargin, 634, "Pegarlas desde el móvil");
  put(r, toybox::kTileFontId, kSmall, kMargin, 662, "ESCANEA UN CÓDIGO Y PEGA LA CLAVE");
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin, 634, "ABRIR");
  dottedRow(r, 690, kMargin, w - kMargin);
  put(r, toybox::kSerifTileFontId, kTile, kMargin, 728, "Micrófono");
  char micLine[40];
  char micName[24];
  voz::micSetupName(voz::micSetup(voz::audio::setupInUse()), micName, sizeof(micName));
  std::snprintf(micLine, sizeof(micLine), "AJUSTE: %s", micName);
  put(r, toybox::kTileFontId, kSmall, kMargin, 754, micLine);
  putRight(r, toybox::kTileFontId, kSmall, w - kMargin, 728, "PROBAR");
  int base = 786;
  char model[96];
  if (service_ == voz::Service::Gemini) {
    std::snprintf(model, sizeof(model), "MODELO: %s", models_.gemini.c_str());
  } else {
    std::snprintf(model, sizeof(model), "MODELOS: %s, %s", models_.transcribe.c_str(), models_.chat.c_str());
  }
  for (char* p = model; *p; ++p) {
    if (*p >= 'a' && *p <= 'z') *p = static_cast<char>(*p - 'a' + 'A');
  }
  put(r, toybox::kTileFontId, kSmall, kMargin, base, fitted(r, toybox::kTileFontId, model, w - 2 * kMargin).c_str());
}
