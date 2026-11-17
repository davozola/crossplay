#include "PresenterLink.h"

#include <Logging.h>

#include "PresenterCore.h"

#if defined(PRESENTER_BLE) && PRESENTER_BLE

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLEServer.h>
#include <host/ble_store.h>

#include <atomic>

namespace presenter::link {

namespace {

constexpr uint16_t kAppearanceKeyboard = 0x03C1;
// Espressif's vendor id with a product id of this app's own; hosts only use the
// pair to remember per-device settings.
constexpr uint16_t kVendorId = 0x303A;
constexpr uint16_t kProductId = 0x8F51;

std::atomic<bool> linked{false};
bool started = false;
bool failed = false;
BLEHIDDevice* hid = nullptr;
BLECharacteristic* input = nullptr;

class ServerEvents final : public BLEServerCallbacks {
  void onConnect(BLEServer*) override { linked = true; }
  void onDisconnect(BLEServer*) override {
    linked = false;
    // A computer that sleeps or walks out of range finds the device again.
    BLEDevice::startAdvertising();
  }
};

ServerEvents events;

}  // namespace

bool real() { return true; }

bool begin(const char* name, const uint8_t batteryPercent) {
  if (started) return true;
  failed = false;
  LOG_INF("PRS", "heap before bluetooth: %u", static_cast<unsigned>(ESP.getFreeHeap()));
  if (!BLEDevice::init(name) || !BLEDevice::getInitialized()) {
    LOG_ERR("PRS", "bluetooth did not start");
    failed = true;
    return false;
  }
  // Paired and encrypted, with nothing to type on either side: the device has
  // no keys to enter a code with.
  BLESecurity::setAuthenticationMode(true, false, true);
  BLESecurity::setCapability(ESP_IO_CAP_NONE);

  BLEServer* server = BLEDevice::createServer();
  if (!server) {
    LOG_ERR("PRS", "no bluetooth server");
    BLEDevice::deinit(false);
    failed = true;
    return false;
  }
  server->setCallbacks(&events);

  // Owned by the BLE stack's service table once started; released by deinit.
  hid = new (std::nothrow) BLEHIDDevice(server);
  if (!hid) {
    LOG_ERR("PRS", "OOM: hid device");
    BLEDevice::deinit(false);
    failed = true;
    return false;
  }
  input = hid->inputReport(kReportId);
  hid->outputReport(kReportId);
  hid->manufacturer("CrossPlay");
  hid->pnp(0x02, kVendorId, kProductId, 0x0100);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap(const_cast<uint8_t*>(kReportMap), sizeof(kReportMap));
  hid->startServices();
  hid->setBatteryLevel(batteryPercent);

  BLEAdvertising* advertising = server->getAdvertising();
  advertising->setAppearance(kAppearanceKeyboard);
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
  advertising->start();

  started = true;
  LOG_INF("PRS", "advertising as %s, heap %u", name, static_cast<unsigned>(ESP.getFreeHeap()));
  return true;
}

void end() {
  if (!started) return;
  BLEDevice::deinit(false);
  started = false;
  linked = false;
  hid = nullptr;
  input = nullptr;
}

State state() {
  if (failed) return State::Failed;
  if (!started) return State::Off;
  return linked ? State::Linked : State::Waiting;
}

bool tap(const uint8_t usage) {
  if (!started || !linked || !input) return false;
  uint8_t report[kReportBytes];
  fillReport(report, usage);
  input->setValue(report, sizeof(report));
  input->notify();
  delay(12);
  fillReport(report, 0);
  input->setValue(report, sizeof(report));
  input->notify();
  return true;
}

void forget() {
  if (!started) return;
  const int rc = ble_store_clear();
  LOG_INF("PRS", "pairings cleared: %d", rc);
}

}  // namespace presenter::link

#else

// No radio in this build. The simulator pretends a computer is linked so the
// screens can be drawn; anything else says plainly that it cannot.

namespace presenter::link {

namespace {
bool started = false;
}

bool real() { return false; }

#if defined(SIMULATOR)
bool begin(const char*, uint8_t) {
  started = true;
  return true;
}
State state() { return started ? State::Linked : State::Off; }
bool tap(const uint8_t usage) {
  LOG_DBG("PRS", "pretend key %02x", usage);
  return started;
}
#else
bool begin(const char*, uint8_t) { return false; }
State state() { return State::Failed; }
bool tap(uint8_t) { return false; }
#endif

void end() { started = false; }
void forget() {}

}  // namespace presenter::link

#endif
