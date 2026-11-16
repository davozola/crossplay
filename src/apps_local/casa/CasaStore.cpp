#include "CasaStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cmath>
#include <ctime>

#if !defined(SIMULATOR)
#include <EnvironmentSensor.h>
#endif

namespace casastore {

namespace {

constexpr char kDir[] = "/.crosspoint/casa";
constexpr char kLogPath[] = "/.crosspoint/casa/log.bin";
constexpr char kSleepPath[] = "/.crosspoint/casa/sleep";

#if !defined(SIMULATOR)
EnvironmentSensor sensor;
bool sensorTried = false;
#endif

}  // namespace

bool readNow(casa::Sample& out) {
  float temp = 0;
  float hum = 0;
#if defined(SIMULATOR)
  temp = 21.4f;
  hum = 48.0f;
#else
  if (!sensorTried) {
    sensorTried = true;
    if (!sensor.begin()) LOG_ERR("CASA", "no temperature sensor answered");
  }
  if (!sensor.present() || !sensor.read(temp, hum)) return false;
#endif
  out.epoch = static_cast<uint32_t>(time(nullptr));
  out.tempCenti = static_cast<int16_t>(std::lround(temp * 100.0f));
  out.humCenti = static_cast<uint16_t>(std::lround(hum * 100.0f));
  return true;
}

bool loadLog(casa::Log& log) {
  log.count = 0;
  if (!Storage.exists(kLogPath)) return false;
  auto bytes = makeUniqueNoThrow<uint8_t[]>(casa::kBytesCap);
  if (!bytes) {
    LOG_ERR("CASA", "OOM: %u bytes", static_cast<unsigned>(casa::kBytesCap));
    return false;
  }
  HalFile file;
  if (!Storage.openFileForRead("CASA", kLogPath, file)) return false;
  const int got = file.read(bytes.get(), casa::kBytesCap);
  return got > 0 && casa::fromBytes(bytes.get(), static_cast<size_t>(got), log);
}

bool saveLog(const casa::Log& log) {
  auto bytes = makeUniqueNoThrow<uint8_t[]>(casa::kBytesCap);
  if (!bytes) {
    LOG_ERR("CASA", "OOM: %u bytes", static_cast<unsigned>(casa::kBytesCap));
    return false;
  }
  const size_t size = casa::toBytes(log, bytes.get(), casa::kBytesCap);
  if (size == 0) return false;
  Storage.ensureDirectoryExists(kDir);
  HalFile file;
  if (!Storage.openFileForWrite("CASA", kLogPath, file)) {
    LOG_ERR("CASA", "cannot write %s", kLogPath);
    return false;
  }
  file.write(bytes.get(), size);
  file.flush();
  return true;
}

void note() {
  casa::Sample sample;
  if (!readNow(sample)) return;
  auto log = makeUniqueNoThrow<casa::Log>();
  if (!log) {
    LOG_ERR("CASA", "OOM: %u bytes", static_cast<unsigned>(sizeof(casa::Log)));
    return;
  }
  loadLog(*log);
  if (casa::append(*log, sample)) saveLog(*log);
}

bool sleepLogging() {
  if (!Storage.exists(kSleepPath)) return false;
  HalFile file;
  uint8_t value = 0;
  return Storage.openFileForRead("CASA", kSleepPath, file) && file.read(&value, 1) == 1 && value == 1;
}

void setSleepLogging(const bool on) {
  Storage.ensureDirectoryExists(kDir);
  HalFile file;
  if (!Storage.openFileForWrite("CASA", kSleepPath, file)) return;
  const uint8_t value = on ? 1 : 0;
  file.write(&value, 1);
  file.flush();
}

}  // namespace casastore
