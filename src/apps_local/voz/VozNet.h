#pragma once

// VOZ's errand: a saved take to the chosen service and the note back. Blocks
// for the length of the exchange (seconds); call with Wi-Fi up.

#include <string>

#include "VozCore.h"

namespace voz::net {

// The note for the WAV file at `wavPath`, or false with `status` set to the
// HTTP status, 0 for no answer, or one of failure()'s negative codes.
bool transcribe(const char* wavPath, Service service, const char* key, const Models& models, Note& note, int& status);

}  // namespace voz::net
