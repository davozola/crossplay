#pragma once
// What every page that fetches by itself shares. The radio is up only for a
// fetch -- joined, used and put down again -- and between two rounds the page
// naps instead of idling with Wi-Fi held at ~80mA.
#include <cstdint>
#include <optional>
#include <string>

#include "../live/LiveEngine.h"

class GfxRenderer;
class MappedInputManager;

namespace autorefresh {

// A round whose join failed comes up again this soon.
constexpr uint32_t kRetryMs = 180000;
// After a touch or a key the page stays awake this long before it naps.
constexpr uint32_t kAwakeAfterInputMs = 20000;

// True when a network is saved for the radio to join with no picker shown.
bool canJoinAlone();

// The radio for one piece of network work. Joins the saved network when nobody
// holds one, and puts the radio down on the way out unless Developer Mode owns
// it -- on every path, the failures included.
class Radio {
 public:
  Radio();
  ~Radio();
  bool online() const { return online_; }

  Radio(const Radio&) = delete;
  Radio& operator=(const Radio&) = delete;

 private:
  std::string message_;
  std::optional<live::engine::RadioLease> lease_;
  bool online_ = false;
};

// Joins the network this reader last used, with no screen of its own, and
// leaves it up for the app that asked: the app puts it down as it would one the
// picker raised. False when there is no saved network, Developer Mode holds
// the radio, or the join did not happen in time -- the picker is the fallback.
bool joinSaved();

// The one sign a page gives that it is fetching behind what it shows: a small
// tag in the bottom right corner, drawn over the finished frame just before it
// goes to the panel. No screen of its own, so what was cached stays readable.
void drawUpdatingMark(GfxRenderer& renderer);

// When the next round is due after one that could not join.
inline void retrySoon(const uint32_t now, const uint32_t intervalMs, uint32_t& lastFetchMs) {
  if (intervalMs > kRetryMs) lastFetchMs = now - (intervalMs - kRetryMs);
}

// A page's rest between rounds: when it was last touched, and whether a paint
// it asked for has reached the panel yet, since a nap must not start before.
struct Quiet {
  uint32_t lastInputMs = 0;
  bool paintPending = false;

  void touched();
  // Call once a loop, before the input is handled.
  void noteInput(const MappedInputManager& input);
  // Naps until the next round is due, or until somebody touches the panel or
  // a key. Does nothing while a paint is pending, soon after an input, or
  // with the radio up.
  void rest(uint32_t lastFetchMs, uint32_t intervalMs);
};

}  // namespace autorefresh
