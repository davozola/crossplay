#pragma once

// The page a phone pastes an API key into, so a key of forty-odd characters is
// not typed on the panel. One page and two requests, on a server of its own,
// the way RSS takes its feeds.
//
// The address is printed in a QR code and anyone on the same network can open
// it while the screen is up, so nothing is read back through it: the page
// learns which services have a key and never a character of one. It can
// replace a key, which is the exposure RSS's and Notes' pages already have.
//
// A handler does no work on the card. It leaves the key in the inbox and the
// app takes it from its own loop.

#include <memory>
#include <mutex>

#include "VozCore.h"

class WebServer;

class VozServer {
 public:
  VozServer() = default;
  ~VozServer();
  VozServer(const VozServer&) = delete;
  VozServer& operator=(const VozServer&) = delete;

  bool begin();
  void stop();
  void handleClient();

  // Which services have a key, for the page to say so.
  void setHave(bool gemini, bool openai);
  // The key a phone sent since the last call, tidied. False when there is none.
  bool take(voz::Service& service, char* key, size_t cap);

 private:
  void page();
  void state();
  void key();

  std::unique_ptr<WebServer> server_;
  std::mutex mutex_;
  bool have_[2] = {false, false};
  bool pending_ = false;
  voz::Service pendingService_ = voz::Service::Gemini;
  char pendingKey_[voz::kMaxKeyChars + 1] = "";
};
