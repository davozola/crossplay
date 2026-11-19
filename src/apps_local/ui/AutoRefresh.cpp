#include "AutoRefresh.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <Logging.h>
#include <WiFi.h>

#include "../../DevMode.h"
#include "../../MappedInputManager.h"
#include "../../WifiCredentialStore.h"
#include "../power/LightNap.h"
#include "ToyboxFonts.h"

namespace autorefresh {

bool canJoinAlone() {
  WIFI_STORE.loadFromFile();
  return !WIFI_STORE.getLastConnectedSsid().empty();
}

bool joinSaved() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (devmode::holdsRadio()) return false;
  WIFI_STORE.loadFromFile();
  const std::string ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) return false;
  const auto credential = WIFI_STORE.findCredential(ssid);
  if (!credential.has_value()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), credential->password.empty() ? nullptr : credential->password.c_str());
  constexpr uint32_t kJoinMs = 12000;
  const uint32_t started = millis();
  while (millis() - started < kJoinMs) {
    if (WiFi.status() == WL_CONNECTED) {
      LOG_INF("AUTO", "joined the saved network in %lu ms", static_cast<unsigned long>(millis() - started));
      return true;
    }
    delay(100);
  }
  LOG_ERR("AUTO", "the saved network did not answer; the picker takes over");
  WiFi.disconnect(true);
  return false;
}

void drawUpdatingMark(GfxRenderer& renderer) {
  constexpr const char* kText = "UPDATING";
  const int textW = renderer.getTextWidth(toybox::kTileFontId, kText);
  const int lineH = renderer.getLineHeight(toybox::kTileFontId);
  const int w = textW + 16;
  const int h = lineH + 8;
  const int x = renderer.getScreenWidth() - w;
  const int y = renderer.getScreenHeight() - h;
  renderer.fillRect(x, y, w, h, true);
  renderer.drawText(toybox::kTileFontId, x + 8, y + 4, kText, false);
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
