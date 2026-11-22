#pragma once

// The page a phone or a computer opens to read the notes, keep them and paste
// an API key, on a server of its own, the way RSS takes its feeds.
//
// The address is printed in a QR code and anyone on the same network can open
// it while the screen is up. What is behind it is the notes in /voz, to read
// and to download, and the two keys, to replace: the page learns which
// services have a key and never a character of one. Nothing else on the card
// is reachable, because a request can name a note only by its stamp.
//
// A key is not written by a handler. It is left in the inbox and the app takes
// it from its own loop.

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
  void notes();
  void note();
  void audio();
  void exportAll();

  std::unique_ptr<WebServer> server_;
  std::mutex mutex_;
  bool have_[2] = {false, false};
  bool pending_ = false;
  voz::Service pendingService_ = voz::Service::Gemini;
  char pendingKey_[voz::kMaxKeyChars + 1] = "";
};
