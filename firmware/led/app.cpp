#include "app.h"

#include <memory>

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include <battery.h>
#include <ble.h>
#include <sensor.h>

#include "led.h"

// LED pins per the v2.1 schematic (see led.h). Button on D3/GPIO5, an
// RTC-capable pin, pulled up to 3V3 by R8 (10 kΩ) and shorted to GND when
// pressed — valid as a deep-sleep GPIO wake source.
constexpr Led::Config LED_PINS {};
constexpr int         BUTTON_PIN = D3;

// Battery ADC on D0, VBUS-detect divider on D1.
constexpr Battery::Config BAT_CONFIG {
    .batteryPin = A0,
    .vbusPin    = D1,
};

// SCD41 I2C routing: D4/GPIO6 (SDA), D5/GPIO7 (SCL) — board defaults.
constexpr Sensor::Config SENSOR_PINS { SDA, SCL };

// 5-minute beacon interval (matches the display variant).
constexpr uint64_t SLEEP_DURATION_US = 5ULL * 60 * 1000000;
constexpr uint32_t SLEEP_DURATION_MS = 5 * 60 * 1000;

// How long the CO2 colour is shown each cycle before going dark. The LED
// can't persist through sleep like e-paper, so it's a brief flash.
constexpr uint32_t FLASH_MS = 1000;

// Dim blue held continuously while on USB, so an idle powered device is
// visibly distinct from one asleep on battery.
constexpr Led::Color USB_IDLE { 0, 0, 20 };

namespace {

std::unique_ptr<Led>     g_led;
std::unique_ptr<Battery> g_battery;

// CO2 → colour: 400–600 green, 600–800 yellow, 800–1200 orange, >1200 red.
Led::Color colorForCo2(uint16_t co2) {
    if (co2 < 600)  return Led::GREEN;
    if (co2 < 800)  return Led::YELLOW;
    if (co2 <= 1200) return Led::ORANGE;
    return Led::RED;
}

// One beacon cycle, identical in effect to the display variant's setup()
// body: measure the SCD41, read the battery, flash the CO2 colour, then
// advertise the reading. Runs on both the battery and USB paths.
void runCycle(esp_reset_reason_t resetReason, bool firstBoot) {
    g_led->set(Led::OFF);  // dark during the measurement (LEDC pauses in light sleep)

    Sensor::Reading reading;
    {
        Sensor sensor(firstBoot, SENSOR_PINS);
        sensor.startMeasurement();
        esp_sleep_enable_timer_wakeup(Sensor::MEASURE_MS * 1000ULL);
        esp_light_sleep_start();
        reading = sensor.read();
    }  // ~Sensor() powers the SCD41 down (preserves ASC across cycles)

    Battery::Status bat = g_battery->read();

    Serial.printf("co2=%d temp=%.1f rh=%.1f bat=%d%% charging=%d rst=%d\n",
                  reading.co2, reading.temperature, reading.humidity,
                  bat.pct, bat.charging ? 1 : 0, (int)resetReason);

    g_led->set(colorForCo2(reading.co2));
    delay(FLASH_MS);
    g_led->set(Led::OFF);

    Ble::advertise(reading, bat, resetReason);  // blocks ~5 s
}

// Battery path: deep-sleep until the timer fires or the button is pressed.
// Either wake source re-boots the chip straight back into setup(). Never
// returns — the chip resets on wake.
[[noreturn]] void deepSleepUntilNextCycle() {
    g_led->set(Led::OFF);
    // Wait for release first: the GPIO wake below is level-triggered, so a
    // still-held button would fire it immediately and run a second cycle.
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
    esp_deep_sleep_enable_gpio_wakeup(1ULL << BUTTON_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_start();
    while (true) {}  // unreachable; silences the compiler
}

}  // namespace

void app::setup() {
    // Drive the LED pins low first thing: the blue channel (D7/GPIO20) glows
    // faintly between reset and LED setup on a battery wake.
    for (int pin : {LED_PINS.red, LED_PINS.green, LED_PINS.blue}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    setCpuFrequencyMhz(80);
    Serial.begin(115200);

    g_battery = std::make_unique<Battery>(BAT_CONFIG);
    g_led     = std::make_unique<Led>(LED_PINS);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    esp_reset_reason_t resetReason = esp_reset_reason();
    // Both a timer wake and a button wake come back as ESP_RST_DEEPSLEEP, so
    // firstBoot is false on either — the sensor was left in power-down.
    bool firstBoot = (resetReason != ESP_RST_DEEPSLEEP);

    runCycle(resetReason, firstBoot);

    if (!g_battery->usbConnected()) {
        deepSleepUntilNextCycle();  // battery: sleep here, never returns
    }

    // USB: stay awake, hold the idle blue, and let loop() drive the cadence.
    g_led->set(USB_IDLE);
}

void app::loop() {
    // Reached only on USB (the battery path never returns from setup()).
    // If USB was unplugged, switch to the battery deep-sleep behaviour.
    if (!g_battery->usbConnected()) {
        deepSleepUntilNextCycle();
    }

    // Re-run the cycle every 5 minutes, or immediately on a button press.
    static uint32_t lastCycle = millis();
    bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
    if (!buttonPressed && (millis() - lastCycle) < SLEEP_DURATION_MS) {
        return;
    }

    runCycle(esp_reset_reason(), false);
    lastCycle = millis();

    // One press = one cycle: wait for release so holding the button (or a
    // contact still low after the ~11 s cycle) doesn't re-trigger.
    while (digitalRead(BUTTON_PIN) == LOW) delay(10);
    g_led->set(USB_IDLE);
}
