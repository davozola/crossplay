#pragma once

// A nap between two paints, for an app that stays on the panel and changes
// once a minute. The processor stops in light sleep with its memory and the
// display's state intact, and comes back when the time is up or when somebody
// touches the panel or a button -- so the page is still alive, it has only
// stopped burning power while nothing on it moves.
//
// Light sleep, not deep: waking from deep sleep is a reboot, which would have
// to bring the display up from nothing and flash it every minute.

#include <cstdint>

namespace lightnap {

enum class Woke : uint8_t {
  Timer,        // the time ran out
  Input,        // the touch panel or a button
  Unsupported,  // this build cannot nap (the simulator): nothing happened
};

// Call only when nothing is being painted and the radio is off. Returns as
// soon as an input asks, and at the latest after `micros`.
Woke nap(uint64_t micros);

}  // namespace lightnap
