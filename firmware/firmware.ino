#include <memory>

#include <esp_system.h>
#include <esp_sleep.h>

#include "battery.h"
#include "ble.h"
#include "display.h"
#include "sensor.h"

// Build-time toggle for the e-paper display. Set to false for sensor-only
// builds (no display wired). When false, no Display is ever constructed
// and every display use is elided at compile time.
constexpr bool DISPLAY_ENABLED = true;

// Battery ADC pin: D0/GPIO2, tapped through a 220k+220k divider (ratio 1:2).
constexpr int BAT_PIN = A0;

constexpr uint64_t SLEEP_DURATION_US = 5ULL * 60 * 1000000;  // 5-minute cycle

void setup() {
    setCpuFrequencyMhz(80);
    Serial.begin(115200);

    esp_reset_reason_t resetReason = esp_reset_reason();
    bool firstBoot = (resetReason != ESP_RST_DEEPSLEEP);

    {
        std::unique_ptr<Display> display;
        if constexpr (DISPLAY_ENABLED) {
            display = std::make_unique<Display>(firstBoot);
        }

        if (firstBoot) {
            const char* rstMsg = "RST: poweron/pin";
            switch (resetReason) {
                case ESP_RST_BROWNOUT: rstMsg = "RST: brownout";   break;
                case ESP_RST_PANIC:    rstMsg = "RST: panic";      break;
                case ESP_RST_TASK_WDT: rstMsg = "RST: task wdt";   break;
                case ESP_RST_INT_WDT:  rstMsg = "RST: int wdt";    break;
                case ESP_RST_WDT:      rstMsg = "RST: rtc wdt";    break;
                case ESP_RST_SW:       rstMsg = "RST: software";   break;
                default:                                             break;
            }
            if (display) {
                display->showStatus(rstMsg);
                display->showStatus("Display OK");
                display->showStatus("Sensor init...");
            }
        }

        Sensor::Reading reading;
        {
            Sensor sensor(firstBoot);
            if (firstBoot && display) display->showStatus("Measuring...");

            sensor.startMeasurement();
            esp_sleep_enable_timer_wakeup(Sensor::MEASURE_MS * 1000ULL);
            esp_light_sleep_start();

            reading = sensor.read();
        }

        Battery battery(BAT_PIN);
        Battery::Status bat = battery.read();

        Serial.printf("co2=%d temp=%.1f rh=%.1f bat=%d%% charging=%d rst=%d\n",
                      reading.co2, reading.temperature, reading.humidity,
                      bat.pct, bat.charging ? 1 : 0, (int)resetReason);

        if (display) {
            display->showReading(reading, bat);
        }
        Ble::advertise(reading, bat, resetReason);
    }  // ~Display() releases SPI and floats MOSI

    esp_deep_sleep(SLEEP_DURATION_US);
}

void loop() {}
