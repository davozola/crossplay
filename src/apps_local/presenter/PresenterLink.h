#pragma once

// PRESENTER's radio: the device shows up to a computer as a Bluetooth keyboard.
// Nothing here knows about slides; it sends one key and reports the link.

#include <cstdint>

namespace presenter::link {

enum class State : uint8_t { Off, Failed, Waiting, Linked };

// Starts the radio and begins advertising. False when Bluetooth would not start.
bool begin(const char* name, uint8_t batteryPercent);
// Stops the radio. The computer keeps its pairing.
void end();
State state();
// One key press and release. False when no computer is linked.
bool tap(uint8_t usage);
// The same with modifiers held: the report's first byte, Ctrl Shift Alt Gui
// from bit 0. DECK's shortcuts go through here.
bool chord(uint8_t modifiers, uint8_t usage);
// Forgets every computer paired with this device.
void forget();
// True where the radio is real; false in builds that only pretend.
bool real();

// What a computer lists the device as, for PRESENTER and DECK alike: one
// pairing serves both.
constexpr char kName[] = "Sticky";

}  // namespace presenter::link
