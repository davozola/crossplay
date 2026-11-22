#include "VozServer.h"

#include <Logging.h>
#include <WebServer.h>

#include <cstdio>
#include <cstring>
#include <new>
#include <string>

#include "VozPageHtml.generated.h"

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
