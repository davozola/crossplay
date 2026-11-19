#pragma once

// The bell for a Pomodoro phase that ends while the app is not on the panel:
// in another app, or with the device asleep.

#include <cstdint>

namespace pomodorobell {

// The phase ends at `dueEpoch` (seconds since 1970). 0 clears it.
void set(uint32_t dueEpoch);
void clear();
// The app is back on the panel and rings for itself: a bell still to come is
// dropped, one that has already rung stays for takeRang().
void cancelPending();

// Seconds from now until the bell, 0 when there is none or the clock is not
// set. A bell already past reads as 1, so the next wake rings it.
uint32_t secondsUntil();

// Rings if the bell is due. Cheap enough for every pass of the main loop.
void poll();

// The timer woke the device for the bell: ring it.
void ringFromSleep();

// True once if the bell rang while the app was away, so the app does not ring
// the same ending a second time.
bool takeRang();

// Two rounds of three rising beeps; nothing on a board without a buzzer.
void chime();

}  // namespace pomodorobell
