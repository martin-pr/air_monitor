#include "app.h"

#include <memory>

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include <battery.h>
#include <ble.h>
#include <sensor.h>

#include "button.h"
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

// Holding the button at least this long triggers a forced recalibration
// instead of a normal beacon cycle.
constexpr uint32_t CALIBRATION_HOLD_MS = 1000;

// CO2 the sensor is told it's seeing during calibration: fresh outdoor air.
// Change this if you recalibrate against a different known reference.
constexpr uint16_t CALIBRATION_PPM = 420;

namespace {

std::unique_ptr<Led>     g_led;
std::unique_ptr<Battery> g_battery;
std::unique_ptr<Button>  g_button;

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
    g_led->set(Led::OFF);  // dark during the measurement

    Sensor::Reading reading;
    {
        Sensor sensor(firstBoot, SENSOR_PINS);
        sensor.startMeasurement();  // blocks ~MEASURE_MS inside the driver
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

// Forced recalibration, triggered by a long button press. Holds the LED white
// while the sensor takes a reading and rebases its calibration, then flashes
// the outcome (green = accepted, red = rejected). The sensor should have been
// sitting in stable air at CALIBRATION_PPM for a few minutes beforehand.
void runCalibration(bool firstBoot) {
    bool ok;
    {
        Sensor sensor(firstBoot, SENSOR_PINS);
        sensor.startMeasurement();  // one reading in the current air first
        sensor.read();

        g_led->set(Led::WHITE);
        ok = sensor.calibrate(CALIBRATION_PPM);
    }

    Serial.printf("calibrate ref=%d ok=%d\n", CALIBRATION_PPM, ok ? 1 : 0);

    g_led->set(ok ? Led::GREEN : Led::RED);
    delay(FLASH_MS);
    g_led->set(Led::OFF);
}

// Battery path: deep-sleep until the timer fires or the button is pressed.
// Either wake source re-boots the chip straight back into setup(). Never
// returns — the chip resets on wake.
[[noreturn]] void deepSleepUntilNextCycle() {
    g_led->set(Led::OFF);
    // Wait for release first: the GPIO wake below is level-triggered, so a
    // still-held button would fire it immediately and run a second cycle.
    g_button->waitForRelease();
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
    g_button->enableWakeup();
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
    g_button  = std::make_unique<Button>(BUTTON_PIN);

    esp_reset_reason_t resetReason = esp_reset_reason();
    // Both a timer wake and a button wake come back as ESP_RST_DEEPSLEEP, so
    // firstBoot is false on either — the sensor was left in power-down.
    bool firstBoot = (resetReason != ESP_RST_DEEPSLEEP);

    // A >1s hold (on a battery button-wake, or a cold boot with the button
    // down) calibrates instead of running a normal cycle.
    if (g_button->isPressed() && g_button->heldFor(CALIBRATION_HOLD_MS)) {
        runCalibration(firstBoot);
    } else {
        runCycle(resetReason, firstBoot);
    }

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
    bool buttonPressed = g_button->isPressed();
    if (!buttonPressed && (millis() - lastCycle) < SLEEP_DURATION_MS) {
        return;
    }

    // A held button calibrates; a short press (or the timer) runs a cycle.
    if (buttonPressed && g_button->heldFor(CALIBRATION_HOLD_MS)) {
        runCalibration(false);
    } else {
        runCycle(esp_reset_reason(), false);
    }
    lastCycle = millis();

    // One press = one action: wait for release so holding the button (or a
    // contact still low after the cycle) doesn't re-trigger.
    g_button->waitForRelease();
    g_led->set(USB_IDLE);
}
