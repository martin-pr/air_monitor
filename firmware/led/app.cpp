#include "app.h"

#include <memory>

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <battery.h>

#include "led.h"

constexpr Led::Config LED_PINS {};   // uses the Config defaults

constexpr int BUTTON_PIN = D3;

// Battery ADC on D0, VBUS-detect divider on D1.
constexpr Battery::Config BAT_CONFIG {
    .batteryPin = A0,
    .vbusPin    = D1,
};

// LED cycle timing (same for both power modes so behavior looks identical).
constexpr uint32_t BLINK_PERIOD_MS = 3000;
constexpr uint32_t BLINK_ON_MS     = 500;

// Dim blue shown continuously during the dark phase when on USB — so it's
// easy to tell "device awake on USB" from "device asleep on battery".
constexpr Led::Color USB_IDLE { 0, 0, 20 };

namespace {

std::unique_ptr<Led>     g_led;
std::unique_ptr<Battery> g_battery;

// Deep-sleep for the dark portion of the blink cycle. Wakes on the
// timer (to run the next blink) or on a button press (to run the
// button-response cycle). Never returns — chip resets on wake.
[[noreturn]] void sleepUntilNextCycle() {
    g_led->set(Led::OFF);
    esp_sleep_enable_timer_wakeup(
        static_cast<uint64_t>(BLINK_PERIOD_MS - BLINK_ON_MS) * 1000);
    esp_deep_sleep_enable_gpio_wakeup(
        1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    // Button is on D3 (GPIO5), an RTC-capable pin, so it's a valid
    // deep-sleep GPIO wake source. Its idle-high level is held by the
    // external 10 kΩ pull-up to 3V3 (R8) — no internal pull-up needed,
    // and 3V3 stays powered through deep sleep so wake-on-LOW works.
    esp_deep_sleep_start();
    while (true) {}  // unreachable; silences the compiler
}

// One LED cycle matching what USB mode would show right now, then sleep.
// If the button is (still) pressed at wake, show red until released;
// otherwise flash white briefly. Called from both battery-cold-boot and
// USB-unplugged-mid-run.
[[noreturn]] void runBatteryCycle() {
    if (digitalRead(BUTTON_PIN) == LOW) {
        g_led->set(Led::RED);
        while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    } else {
        g_led->set(Led::WHITE);
        delay(BLINK_ON_MS);
    }
    sleepUntilNextCycle();
}

// If USB is present, return and let the caller continue. Otherwise
// enter the battery cycle (LED work + deep sleep) — never returns.
void stayAwakeOrRunBatteryCycle() {
    if (g_battery->usbConnected()) return;
    runBatteryCycle();
}

}  // namespace

void app::setup() {
    g_battery = std::make_unique<Battery>(BAT_CONFIG);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    g_led = std::make_unique<Led>(LED_PINS);

    // On battery, do one LED cycle here and sleep (never returns).
    // On USB, fall through to loop() for the continuous polling variant.
    stayAwakeOrRunBatteryCycle();
}

void app::loop() {
    // If USB gets unplugged mid-run, drop into the battery cycle.
    stayAwakeOrRunBatteryCycle();

    if (digitalRead(BUTTON_PIN) == LOW) {
        g_led->set(Led::RED);
        return;
    }

    // Not pressed: white blink using millis-based timing so we keep
    // polling the button responsively.
    uint32_t phase = millis() % BLINK_PERIOD_MS;
    if (phase < BLINK_PERIOD_MS - BLINK_ON_MS) {
        g_led->set(USB_IDLE);
    } else {
        g_led->set(Led::WHITE);
    }
}
