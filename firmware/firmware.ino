#include <array>
#include <esp_system.h>
#include <esp_sleep.h>


#include <BLEDevice.h>
#include <BLEAdvertising.h>

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "battery.h"
#include "sensor.h"

// Beacon advertisement — manufacturer-specific data (10 bytes, little-endian):
//   [0-1]  company ID: 0x41 0x4D ('AM')
//   [2]    protocol version (BEACON_VERSION); bump when layout changes
//   [3]    status: esp_reset_reason_t as uint8 (8 = ESP_RST_DEEPSLEEP = normal wake;
//          anything else means the chip cold-booted — power-on, brownout, panic, wdt, etc.)
//   [4-5]  CO2 in ppm (uint16)
//   [6-7]  temperature in 0.01 °C (int16)
//   [8]    relative humidity in % (uint8)
//   [9]    battery percent (uint8); 0xFF = charging (battery reading is meaningless during charge)
//
// Parsing rule: read [2] first; only parse further fields if version is known.
// Build-time toggle for the e-paper display. Set to false for sensor-only
// builds (no display wired). When false, all display code is elided and
// display.init() — which would otherwise block on a floating BUSY pin — is
// skipped entirely.
constexpr bool DISPLAY_ENABLED = true;

constexpr uint16_t BEACON_COMPANY_ID  = 0x4D41;   // 'AM' (Air Monitor), LE bytes: 0x41 0x4D
constexpr uint8_t  BEACON_VERSION     = 1;

// Byte offsets within the 7-byte payload that follows the 2-byte company ID
constexpr size_t  BOFF_VERSION = 0;  // uint8
constexpr size_t  BOFF_STATUS  = 1;  // uint8,  esp_reset_reason_t (8 = deep sleep wake = normal)
constexpr size_t  BOFF_CO2     = 2;  // uint16 LE, ppm
constexpr size_t  BOFF_TEMP    = 4;  // int16  LE, 0.01 °C
constexpr size_t  BOFF_RH      = 6;  // uint8,  RH%
constexpr size_t  BOFF_BAT     = 7;  // uint8,  % (0xFF = charging)
constexpr uint8_t BAT_CHARGING_SENTINEL = 0xFF;
constexpr size_t  BEACON_PAYLOAD_LEN = 8;  // bytes after company ID

constexpr uint32_t ADV_DURATION_MS    = 5000;
constexpr uint64_t SLEEP_DURATION_US  = 5ULL * 60 * 1000000;  // 5-minute cycle

// ePaper pins (XIAO ESP32-C3)
constexpr int EPD_CS   = 5;   // D3
constexpr int EPD_DC   = 4;   // D2
constexpr int EPD_RST  = 3;   // D1
constexpr int EPD_BUSY = 21;  // D6
constexpr int SPI_SCK  = 8;   // D8
constexpr int SPI_MOSI = 10;  // D10

// ePaper layout (200×200 square panel)
constexpr int EPD_W        = 200;
constexpr int EPD_H        = 200;
constexpr int EPD_MARGIN   = 4;             // inset from all edges
constexpr int EPD_TOP_Y    = 22;            // baseline for top-row labels
constexpr int EPD_BOTTOM_Y = EPD_H - EPD_MARGIN;  // baseline for bottom-row label
constexpr int EPD_CO2_Y    = 103;           // vertical centre target for large CO2 number
constexpr int STATUS_FIRST_Y = 20;
constexpr int STATUS_LINE_STEP = 29;        // FreeSans12pt7b yAdvance
constexpr uint32_t STATUS_STEP_DELAY_MS = 500;  // required to avoid power spikes
constexpr size_t MAX_STATUS_LINES = 8;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void showStatus(const char* msg) {
    if constexpr (!DISPLAY_ENABLED) return;
    static std::array<std::string, MAX_STATUS_LINES> lines;
    static size_t lineCount = 0;

    if (lineCount < MAX_STATUS_LINES) {
        lines[lineCount++] = msg;
    }

    display.init(115200, false);
    display.setRotation(1);

    uint16_t wh = STATUS_FIRST_Y + lineCount * STATUS_LINE_STEP;
    if (wh > EPD_H) {
        wh = EPD_H;
    }
    display.setPartialWindow(0, 0, EPD_W, wh);

    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        for (size_t i = 0; i < lineCount; i++) {
            display.setCursor(EPD_MARGIN, STATUS_FIRST_Y + i * STATUS_LINE_STEP);
            display.print(lines[i].c_str());
        }
    } while (display.nextPage());
    delay(STATUS_STEP_DELAY_MS);
    display.hibernate();
}

void updateDisplay(uint16_t co2, float temperature, float humidity, uint8_t batPct) {
    if constexpr (!DISPLAY_ENABLED) return;
    char co2Str[8], tempStr[8], rhStr[12], batStr[6];
    snprintf(co2Str, sizeof(co2Str), "%d", co2);
    snprintf(tempStr, sizeof(tempStr), "%.1f", temperature);
    snprintf(rhStr,  sizeof(rhStr),  "%.1f%%", humidity);
    snprintf(batStr, sizeof(batStr), "%d%%", batPct);

    int16_t x1, y1;
    uint16_t tw, th;

    display.init(115200, false);
    display.setRotation(1);
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);

        // Top-left: temperature + degree circle + C
        // FreeSans doesn't include 0xB0 (degree); draw a small circle instead
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        display.setCursor(EPD_MARGIN, EPD_TOP_Y);
        display.print(tempStr);
        int16_t cx = display.getCursorX();
        display.drawCircle(cx + 3, 8, 2, GxEPD_BLACK);
        display.setCursor(cx + 7, EPD_TOP_Y);
        display.print("C");

        // Top-right: humidity, right-aligned
        display.getTextBounds(rhStr, 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(EPD_W - EPD_MARGIN - (int16_t)tw, EPD_TOP_Y);
        display.print(rhStr);

        // Centre: large CO2 number (24pt × 2 via setTextSize)
        display.setFont(&FreeSansBold24pt7b);
        display.setTextSize(2);
        display.getTextBounds(co2Str, 0, 0, &x1, &y1, &tw, &th);
        display.setCursor((EPD_W - (int16_t)tw) / 2 - x1,
                          EPD_CO2_Y - y1 - (int16_t)th / 2);
        display.print(co2Str);

        // Bottom-left: battery %
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        display.setCursor(EPD_MARGIN, EPD_BOTTOM_Y);
        display.print(batStr);

        // Bottom-right: unit label
        display.getTextBounds("CO2 (ppm)", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(EPD_W - EPD_MARGIN - (int16_t)tw, EPD_BOTTOM_Y);
        display.print("CO2 (ppm)");

    } while (display.nextPage());
    display.hibernate();
}

void advertise(uint16_t co2, float temperature, float humidity, uint8_t batPct, bool charging, uint8_t status) {
    int16_t tempCdeg = (int16_t)(temperature * 100.0f);
    uint8_t mfr[2 + BEACON_PAYLOAD_LEN];
    mfr[0] = BEACON_COMPANY_ID & 0xFF;
    mfr[1] = BEACON_COMPANY_ID >> 8;
    mfr[2 + BOFF_VERSION]  = BEACON_VERSION;
    mfr[2 + BOFF_STATUS]   = status;
    mfr[2 + BOFF_CO2]      = co2 & 0xFF;
    mfr[2 + BOFF_CO2 + 1]  = co2 >> 8;
    mfr[2 + BOFF_TEMP]     = (uint8_t)(tempCdeg & 0xFF);
    mfr[2 + BOFF_TEMP + 1] = (uint8_t)(tempCdeg >> 8);
    mfr[2 + BOFF_RH]       = (uint8_t)(humidity);
    mfr[2 + BOFF_BAT]      = charging ? BAT_CHARGING_SENTINEL : batPct;

    BLEDevice::init("Air Monitor");
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    BLEAdvertisementData advData;
    // Build manufacturer-specific AD structure manually to handle binary data safely:
    // [length][0xFF = mfr type][company_id lo][company_id hi][payload...]
    uint8_t ad[2 + sizeof(mfr)];
    ad[0] = 1 + sizeof(mfr);
    ad[1] = 0xFF;
    memcpy(ad + 2, mfr, sizeof(mfr));
    advData.addData((char*)ad, sizeof(ad));
    adv->setAdvertisementData(advData);
    adv->setMinInterval(160);  // 100ms (units of 0.625ms)
    adv->setMaxInterval(160);
    adv->start();

    delay(ADV_DURATION_MS);

    adv->stop();
    BLEDevice::deinit(true);
}

void setup() {
    setCpuFrequencyMhz(80);
    Serial.begin(115200);

    if constexpr (DISPLAY_ENABLED) {
        SPI.begin(SPI_SCK, /*MISO=*/-1, SPI_MOSI, EPD_CS);
        display.init(115200);
        display.setRotation(1);
    }

    esp_reset_reason_t resetReason = esp_reset_reason();
    bool firstBoot = (resetReason != ESP_RST_DEEPSLEEP);

    if (firstBoot) {
        if constexpr (DISPLAY_ENABLED) {
            display.setFullWindow();
            display.firstPage();
            do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
            display.epd2.writeScreenBufferAgain();  // sync SSD1681 current/previous RAM after clear
            display.hibernate();
        }

        const char* rstMsg = "RST: poweron/pin";
        switch (resetReason) {
            case ESP_RST_BROWNOUT: rstMsg = "RST: brownout";    break;
            case ESP_RST_PANIC:    rstMsg = "RST: panic";       break;
            case ESP_RST_TASK_WDT: rstMsg = "RST: task wdt";   break;
            case ESP_RST_INT_WDT:  rstMsg = "RST: int wdt";    break;
            case ESP_RST_WDT:      rstMsg = "RST: rtc wdt";    break;
            case ESP_RST_SW:       rstMsg = "RST: software";   break;
            default:                                             break;
        }
        showStatus(rstMsg);
        showStatus("Display OK");
        showStatus("Sensor init...");
    }

    // Sensor is scoped to the measurement path so its destructor (which
    // releases the I2C bus) runs before we prep for deep sleep.
    Sensor::Reading reading;
    {
        Sensor sensor(firstBoot);
        if (firstBoot) showStatus("Measuring...");

        sensor.startMeasurement();
        esp_sleep_enable_timer_wakeup(Sensor::MEASURE_MS * 1000ULL);
        esp_light_sleep_start();

        reading = sensor.read();
    }

    // Battery ADC: D0/GPIO2, tapped through a 220k+220k divider (ratio 1:2).
    Battery battery(A0);
    Battery::Status bat = battery.read();

    Serial.printf("co2=%d temp=%.1f rh=%.1f bat=%d%% charging=%d rst=%d\n",
                  reading.co2, reading.temperature, reading.humidity,
                  bat.pct, bat.charging ? 1 : 0, (int)resetReason);

    updateDisplay(reading.co2, reading.temperature, reading.humidity, bat.pct);
    advertise(reading.co2, reading.temperature, reading.humidity, bat.pct, bat.charging, (uint8_t)resetReason);
    if constexpr (DISPLAY_ENABLED) SPI.end();
    pinMode(SPI_MOSI, INPUT);  // GPIO10 = XIAO user LED (active low); float to reduce sleep current
    esp_deep_sleep(SLEEP_DURATION_US);
}

void loop() {}
