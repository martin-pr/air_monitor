#include "app.h"

#include <esp_system.h>
#include <esp_sleep.h>

#include <battery.h>
#include <ble.h>
#include <reset_reason.h>
#include <sensor.h>

#include "display.h"

// Battery ADC pin: D0/GPIO2, tapped through a 220k+220k divider (ratio 1:2).
// No VBUS-sense wire on v1 — charging is inferred from Vbat.
constexpr Battery::Config BAT_CONFIG { .batteryPin = A0 };

// SCD41 I2C bus routing: D4/GPIO6 (SDA), D5/GPIO7 (SCL) — board defaults.
constexpr Sensor::Config SENSOR_PINS { SDA, SCL };

// SSD1681 e-paper + its SPI bus.
constexpr Display::Config DISPLAY_PINS {
    .cs   = D3,
    .dc   = D2,
    .rst  = D1,
    .busy = D6,
    .sck  = D8,
    .mosi = D10,
};

constexpr uint64_t SLEEP_DURATION_US = 5ULL * 60 * 1000000;  // 5-minute cycle

void app::setup() {
    setCpuFrequencyMhz(80);
    Serial.begin(115200);

    esp_reset_reason_t resetReason = esp_reset_reason();
    bool firstBoot = (resetReason != ESP_RST_DEEPSLEEP);

    {
        Display display(firstBoot, DISPLAY_PINS);

        if (firstBoot) {
            display.showStatus(resetReasonMessage(resetReason));
            display.showStatus("Display OK");
            display.showStatus("Sensor init...");
        }

        Sensor::Reading reading;
        {
            Sensor sensor(firstBoot, SENSOR_PINS);
            if (firstBoot) display.showStatus("Measuring...");

            sensor.startMeasurement();
            esp_sleep_enable_timer_wakeup(Sensor::MEASURE_MS * 1000ULL);
            esp_light_sleep_start();

            reading = sensor.read();
        }

        Battery battery(BAT_CONFIG);
        Battery::Status bat = battery.read();

        Serial.printf("co2=%d temp=%.1f rh=%.1f bat=%d%% charging=%d rst=%d\n",
                      reading.co2, reading.temperature, reading.humidity,
                      bat.pct, bat.charging ? 1 : 0, (int)resetReason);

        display.showReading(reading, bat);
        Ble::advertise(reading, bat, resetReason);
    }  // ~Display() releases SPI and floats MOSI

    esp_deep_sleep(SLEEP_DURATION_US);
}
