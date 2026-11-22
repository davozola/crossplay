#include "VozServer.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>
#include <WebServer.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#include "VozPageHtml.generated.h"

namespace {

constexpr size_t kMaxNoteBytes = 64 * 1024;
constexpr size_t kMaxListed = 200;
constexpr size_t kAudioChunk = 4096;

void notePath(const char* stamp, const char* ext, char* out, const size_t cap) {
  std::snprintf(out, cap, "%s/%s.%s", voz::kNotesDir, stamp, ext);
}

// Every take on the card, newest first: the names sort by when.
std::vector<std::string> stampsOnCard() {
  std::vector<std::string> stamps;
  const std::vector<String> names = Storage.listFiles(voz::kNotesDir, 1000);
  stamps.reserve(names.size());
  for (const String& name : names) {
    const char* s = name.c_str();
    const char* slash = std::strrchr(s, '/');
    if (slash) s = slash + 1;
    if (std::strlen(s) == 19 && voz::isStamp(s) && !std::strcmp(s + 15, ".wav")) stamps.emplace_back(s, 15);
  }
  std::sort(stamps.begin(), stamps.end(), [](const std::string& a, const std::string& b) { return a > b; });
  if (stamps.size() > kMaxListed) stamps.resize(kMaxListed);
  return stamps;
}

bool readNote(const char* stamp, voz::Note& note) {
  char path[48];
  notePath(stamp, "txt", path, sizeof(path));
  if (!Storage.exists(path)) return false;
  std::string text;
  Storage.readFileToString("VOZ", path, kMaxNoteBytes, text);
  return voz::parse(text, note);
}

}  // namespace

VozServer::~VozServer() { stop(); }

bool VozServer::begin() {
  if (server_) return true;
  server_.reset(new (std::nothrow) WebServer(80));
  if (!server_) {
    LOG_ERR("VOZ", "OOM: web server");
    return false;
  }
  // The address under the code is printed to be typed, so a bare one lands on
  // the page.
  server_->on("/", HTTP_GET, [this] {
    server_->sendHeader("Location", "/v", true);
    server_->send(302, "text/plain", "");
  });
  server_->on("/v", HTTP_GET, [this] { page(); });
  server_->on("/v/state", HTTP_GET, [this] { state(); });
  // PUT with a plain body, which this core hands over whole as arg("plain").
  server_->on("/v/key", HTTP_PUT, [this] { key(); });
  server_->on("/v/notes", HTTP_GET, [this] { notes(); });
  server_->on("/v/note", HTTP_GET, [this] { note(); });
  server_->on("/v/audio", HTTP_GET, [this] { audio(); });
  server_->on("/v/export", HTTP_GET, [this] { exportAll(); });
  server_->onNotFound([this] { server_->send(404, "text/plain", "No hay nada aqui."); });
  server_->begin();
  LOG_INF("VOZ", "phone page up");
  return true;
}

void VozServer::stop() {
  if (!server_) return;
  server_->stop();
  server_.reset();
  LOG_INF("VOZ", "phone page down");
}

void VozServer::handleClient() {
  if (server_) server_->handleClient();
}

void VozServer::setHave(const bool gemini, const bool openai) {
  std::lock_guard<std::mutex> lock(mutex_);
  have_[0] = gemini;
  have_[1] = openai;
}

bool VozServer::take(voz::Service& service, char* key, const size_t cap) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!pending_) return false;
  pending_ = false;
  service = pendingService_;
  std::snprintf(key, cap, "%s", pendingKey_);
  // Not left lying in a buffer that outlives its use.
  std::memset(pendingKey_, 0, sizeof(pendingKey_));
  return true;
}

void VozServer::page() {
  server_->sendHeader("Content-Encoding", "gzip");
  server_->sendHeader("Cache-Control", "no-cache");
  server_->send_P(200, "text/html", VozPageHtml, sizeof(VozPageHtml));
}

void VozServer::state() {
  bool gemini = false;
  bool openai = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    gemini = have_[0];
    openai = have_[1];
  }
  const std::string json = voz::phoneStateJson(gemini, openai);
  server_->sendHeader("Cache-Control", "no-store");
  server_->send(200, "application/json", json.c_str());
}

void VozServer::key() {
  voz::Service service = voz::Service::Gemini;
  if (!voz::serviceFromName(server_->arg("s").c_str(), service)) {
    server_->send(400, "text/plain", "Falta decir de quien es la clave.");
    return;
  }
  const String body = server_->arg("plain");
  // A paste can bring spaces and line ends, so the raw body may be longer than
  // a key; anything far past one is not a key.
  if (body.length() > 2 * voz::kMaxKeyChars) {
    server_->send(413, "text/plain", "Eso es demasiado largo para una clave.");
    return;
  }
  char tidy[voz::kMaxKeyChars + 1];
  if (!voz::tidyPastedKey(body.c_str(), tidy, sizeof(tidy))) {
    server_->send(400, "text/plain", "Eso no parece una clave. Pegala entera.");
    return;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_ = true;
    pendingService_ = service;
    std::snprintf(pendingKey_, sizeof(pendingKey_), "%s", tidy);
    have_[static_cast<int>(service)] = true;
  }
  std::memset(tidy, 0, sizeof(tidy));
  // Which service, never the key.
  LOG_INF("VOZ", "phone sent a key for %s", service == voz::Service::Gemini ? "gemini" : "openai");
  server_->send(200, "text/plain", "Guardada.");
}

// The list goes out a row at a time, so two hundred notes cost one note's
// worth of RAM.
void VozServer::notes() {
  const std::vector<std::string> stamps = stampsOnCard();
  server_->sendHeader("Cache-Control", "no-store");
  server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_->send(200, "application/json", "");
  server_->sendContent("[");
  bool first = true;
  for (const std::string& stamp : stamps) {
    char path[48];
    notePath(stamp.c_str(), "wav", path, sizeof(path));
    int seconds = 0;
    {
      HalFile file;
      if (Storage.openFileForRead("VOZ", path, file)) seconds = voz::secondsIn(file.fileSize());
    }
    voz::Note note;
    const bool done = readNote(stamp.c_str(), note);
    std::string row = first ? "" : ",";
    row += voz::listRowJson(stamp.c_str(), done, note, seconds);
    server_->sendContent(row.c_str(), row.size());
    first = false;
  }
  server_->sendContent("]");
  server_->sendContent("");
}

void VozServer::note() {
  char stamp[16];
  voz::Note note;
  if (!voz::stampFromRequest(server_->arg("id").c_str(), stamp, sizeof(stamp)) || !readNote(stamp, note)) {
    server_->send(404, "text/plain", "Esa nota no esta.");
    return;
  }
  const std::string json = voz::noteJson(stamp, note);
  server_->sendHeader("Cache-Control", "no-store");
  server_->send(200, "application/json", json.c_str());
}

void VozServer::audio() {
  char stamp[16];
  if (!voz::stampFromRequest(server_->arg("id").c_str(), stamp, sizeof(stamp))) {
    server_->send(404, "text/plain", "Esa grabacion no esta.");
    return;
  }
  char path[48];
  notePath(stamp, "wav", path, sizeof(path));
  HalFile file;
  if (!Storage.exists(path) || !Storage.openFileForRead("VOZ", path, file)) {
    server_->send(404, "text/plain", "Esa grabacion no esta.");
    return;
  }
  auto buffer = makeUniqueNoThrow<uint8_t[]>(kAudioChunk);
  if (!buffer) {
    LOG_ERR("VOZ", "OOM: %d bytes", static_cast<int>(kAudioChunk));
    server_->send(503, "text/plain", "El lector no tiene memoria ahora.");
    return;
  }
  char disposition[64];
  std::snprintf(disposition, sizeof(disposition), "attachment; filename=\"voz-%s.wav\"", stamp);
  server_->sendHeader("Content-Disposition", disposition);
  server_->sendHeader("Cache-Control", "no-store");
  server_->setContentLength(file.fileSize());
  server_->send(200, "audio/wav", "");
  // A minute of audio is close to two megabytes; the pause lets the radio and
  // the watchdog have their turn between chunks.
  for (;;) {
    const int got = file.read(buffer.get(), kAudioChunk);
    if (got <= 0) break;
    server_->sendContent(reinterpret_cast<const char*>(buffer.get()), static_cast<size_t>(got));
    if (!server_->client().connected()) break;
    delay(1);
  }
}

// Every note with words, newest first, as one Markdown file.
void VozServer::exportAll() {
  const std::vector<std::string> stamps = stampsOnCard();
  server_->sendHeader("Content-Disposition", "attachment; filename=\"voz-notas.md\"");
  server_->sendHeader("Cache-Control", "no-store");
  server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_->send(200, "text/markdown; charset=utf-8", "");
  server_->sendContent("# Notas de voz\n\n");
  for (const std::string& stamp : stamps) {
    voz::Note note;
    if (!readNote(stamp.c_str(), note)) continue;
    const std::string text = voz::noteMarkdown(stamp.c_str(), note);
    server_->sendContent(text.c_str(), text.size());
  }
  server_->sendContent("");
}
