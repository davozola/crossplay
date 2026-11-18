#include "AutoRefresh.h"

#include <Arduino.h>
#include <Logging.h>
#include <WiFi.h>

#include "../../DevMode.h"
#include "../../MappedInputManager.h"
#include "../../WifiCredentialStore.h"
#include "../power/LightNap.h"

namespace autorefresh {

bool canJoinAlone() {
  WIFI_STORE.loadFromFile();
  return !WIFI_STORE.getLastConnectedSsid().empty();
}

Radio::Radio() {
  lease_.emplace(message_);
  online_ = lease_->held() && WiFi.status() == WL_CONNECTED;
  if (!online_) LOG_INF("AUTO", "no network for this round: %s", message_.c_str());
}

Radio::~Radio() {
  // The lease puts down only what it raised; a radio the picker raised is
  // this page's too once the work is done.
  lease_.reset();
  if (devmode::holdsRadio()) return;
  if (WiFi.getMode() == WIFI_MODE_NULL) return;
  if (WiFi.status() == WL_CONNECTED) WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void Quiet::touched() { lastInputMs = static_cast<uint32_t>(millis()); }

void Quiet::noteInput(const MappedInputManager& input) {
  using Button = MappedInputManager::Button;
  int x = 0;
  int y = 0;
  bool any = input.wasScreenTouchDown(x, y) || input.isScreenTouchHeld(x, y) || input.wasScreenTouchReleased();
  for (const Button button : {Button::Back, Button::Confirm, Button::Left, Button::Right, Button::Up, Button::Down}) {
    any = any || input.isPressed(button) || input.wasReleased(button);
  }
  if (any) touched();
}

void Quiet::rest(const uint32_t lastFetchMs, const uint32_t intervalMs) {
  if (paintPending) return;
  const uint32_t now = static_cast<uint32_t>(millis());
  if (now - lastInputMs < kAwakeAfterInputMs) return;
  const uint32_t since = now - lastFetchMs;
  if (since >= intervalMs) return;
  const uint64_t micros = static_cast<uint64_t>(intervalMs - since) * 1000ULL;
  if (lightnap::nap(micros) == lightnap::Woke::Input) touched();
}

}  // namespace autorefresh
