#pragma once

// CASA's sensor and card: the reading now, the log file, and whether the
// device keeps noting the room while it sleeps.

#include "CasaCore.h"

namespace casastore {

// How often a sleeping device wakes to take a reading, when asked to.
constexpr uint32_t kSleepEverySeconds = 1800;

// False on a board with no sensor, or when it did not answer.
bool readNow(casa::Sample& out);

bool loadLog(casa::Log& log);
bool saveLog(const casa::Log& log);

// Reads the sensor and adds the reading to the log on the card, for callers
// with no log in hand. Quietly does nothing when there is nothing to add.
void note();

bool sleepLogging();
void setSleepLogging(bool on);

}  // namespace casastore
