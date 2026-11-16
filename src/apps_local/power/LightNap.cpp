#include "LightNap.h"

#if defined(SIMULATOR)

namespace lightnap {
Woke nap(uint64_t) { return Woke::Unsupported; }
}  // namespace lightnap

#else

#include <BoardConfig.h>
#include <Logging.h>
#include <WiFi.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

namespace lightnap {

namespace {

// Arms a pin to end the nap when it leaves the level it rests at now. Read
// rather than assumed: the touch controller's interrupt line can be set to
// idle either way, and a pin armed for the level it already holds would end
// every nap the moment it began. Returns false for a pin the board lacks.
bool arm(const int8_t pin) {
  if (pin < 0) return false;
  const gpio_num_t gpio = static_cast<gpio_num_t>(pin);
  const int resting = gpio_get_level(gpio);
  return gpio_wakeup_enable(gpio, resting ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL) == ESP_OK;
}

void disarm(const int8_t pin) {
  if (pin >= 0) gpio_wakeup_disable(static_cast<gpio_num_t>(pin));
}

}  // namespace

Woke nap(const uint64_t micros) {
  // With the radio up a nap would drop the connection; the caller's page would
  // come back to a dead link. Stay awake instead.
  if (WiFi.getMode() != WIFI_MODE_NULL || micros < 200000) return Woke::Unsupported;

  const auto& board = BoardConfig::ACTIVE;
  const int8_t pins[] = {board.touch.irq, board.input.power, board.input.confirm,
                         board.input.up,  board.input.down,  board.input.back};
  bool any = false;
  for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
    // The same pin can serve two roles (confirm and power on the Sticky).
    bool seen = false;
    for (size_t k = 0; k < i; ++k) seen = seen || pins[k] == pins[i];
    if (!seen) any = arm(pins[i]) || any;
  }
  // With nothing to wake it early the page would be deaf for up to a minute.
  if (!any) return Woke::Unsupported;

  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(micros);
  const esp_err_t result = esp_light_sleep_start();
  const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  for (const int8_t pin : pins) disarm(pin);
  if (result != ESP_OK) {
    // Refused (a wake source was already asserted). Not an error worth more
    // than a line: the caller simply stays awake this time round.
    LOG_DBG("NAP", "light sleep refused: %d", static_cast<int>(result));
    return Woke::Input;
  }
  return cause == ESP_SLEEP_WAKEUP_TIMER ? Woke::Timer : Woke::Input;
}

}  // namespace lightnap

#endif
