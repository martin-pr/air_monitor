#include <array>
#include <esp_system.h>
#include <esp_sleep.h>

#include <Wire.h>
#include <SensirionI2cScd4x.h>

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// Beacon advertisement — manufacturer-specific data (9 bytes, little-endian):
//   [0-1]  company ID: 0x41 0x4D ('AM')
//   [2]    protocol version (BEACON_VERSION); bump when layout changes
//   [3-4]  CO2 in ppm (uint16)
//   [5-6]  temperature in 0.01 °C (int16)
//   [7]    relative humidity in % (uint8)
//   [8]    battery percent (uint8)
//
// Parsing rule: read [2] first; only parse further fields if version is known.
// New fields must be appended so older parsers can still read [3-8].
constexpr uint16_t BEACON_COMPANY_ID  = 0x4D41;   // 'AM' (Air Monitor), LE bytes: 0x41 0x4D
constexpr uint8_t  BEACON_VERSION     = 1;

// Byte offsets within the 7-byte payload that follows the 2-byte company ID
constexpr size_t BOFF_VERSION = 0;  // uint8
constexpr size_t BOFF_CO2     = 1;  // uint16 LE, ppm
constexpr size_t BOFF_TEMP    = 3;  // int16  LE, 0.01 °C
constexpr size_t BOFF_RH      = 5;  // uint8,  RH%
constexpr size_t BOFF_BAT     = 6;  // uint8,  %
constexpr size_t BEACON_PAYLOAD_LEN = 7;  // bytes after company ID

constexpr uint32_t ADV_DURATION_MS    = 5000;
constexpr uint32_t SCD41_MEASURE_MS   = 5000;   // single-shot measurement time per datasheet
constexpr uint64_t SLEEP_DURATION_US  = 1ULL * 60 * 1000000;  // 1-minute cycle

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

// Battery ADC
constexpr int BAT_PIN = A0;  // D0/GPIO2; reads through 220k+220k divider (ratio 1:2)

struct BatPoint { float voltage; uint8_t pct; };
constexpr std::array<BatPoint, 4> BAT_CURVE = {{
    {4.20f, 100},
    {3.98f,  80},
    {3.52f,  20},
    {3.00f,   0},
}};

uint8_t readBatteryPercent() {
    float vbat = analogReadMilliVolts(BAT_PIN) * 2.0f / 1000.0f;
    if (vbat >= BAT_CURVE.front().voltage) return 100;
    if (vbat <= BAT_CURVE.back().voltage)  return 0;
    for (size_t i = 0; i < BAT_CURVE.size() - 1; i++) {
        if (vbat >= BAT_CURVE[i + 1].voltage) {
            float t = (vbat - BAT_CURVE[i + 1].voltage) /
                      (BAT_CURVE[i].voltage - BAT_CURVE[i + 1].voltage);
            return (uint8_t)(BAT_CURVE[i + 1].pct + t * (BAT_CURVE[i].pct - BAT_CURVE[i + 1].pct));
        }
    }
    return 0;
}

SensirionI2cScd4x scd4x;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void showStatus(const char* msg) {
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

void advertise(uint16_t co2, float temperature, float humidity, uint8_t batPct) {
    int16_t tempCdeg = (int16_t)(temperature * 100.0f);
    uint8_t mfr[2 + BEACON_PAYLOAD_LEN];
    mfr[0] = BEACON_COMPANY_ID & 0xFF;
    mfr[1] = BEACON_COMPANY_ID >> 8;
    mfr[2 + BOFF_VERSION] = BEACON_VERSION;
    mfr[2 + BOFF_CO2]     = co2 & 0xFF;
    mfr[2 + BOFF_CO2 + 1] = co2 >> 8;
    mfr[2 + BOFF_TEMP]    = (uint8_t)(tempCdeg & 0xFF);
    mfr[2 + BOFF_TEMP + 1]= (uint8_t)(tempCdeg >> 8);
    mfr[2 + BOFF_RH]      = (uint8_t)(humidity);
    mfr[2 + BOFF_BAT]     = batPct;

    WiFi.mode(WIFI_OFF);
    delay(200);
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
    analogSetAttenuation(ADC_11db);  // 0–3.9 V range; covers 1.5–2.1 V from the battery divider

    SPI.begin(SPI_SCK, /*MISO=*/-1, SPI_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(1);

    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);

    bool firstBoot = (esp_reset_reason() != ESP_RST_DEEPSLEEP);

    if (firstBoot) {
        display.setFullWindow();
        display.firstPage();
        do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
        display.epd2.writeScreenBufferAgain();  // sync SSD1681 current/previous RAM after clear
        display.hibernate();

        const char* rstMsg = "RST: poweron/pin";
        switch (esp_reset_reason()) {
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
        scd4x.stopPeriodicMeasurement();
        delay(500);
        showStatus("Measuring...");
    } else {
        scd4x.wakeUp();
        delay(30);  // SCD41 requires 30ms after wakeUp before issuing commands
    }

    scd4x.measureSingleShot();
    esp_sleep_enable_timer_wakeup(SCD41_MEASURE_MS * 1000ULL);
    esp_light_sleep_start();

    uint16_t co2 = 0;
    float temperature = 0.0f, humidity = 0.0f;
    scd4x.readMeasurement(co2, temperature, humidity);
    scd4x.powerDown();

    uint8_t batPct = readBatteryPercent();

    Serial.printf("co2=%d temp=%.1f rh=%.1f bat=%d%%\n", co2, temperature, humidity, batPct);

    updateDisplay(co2, temperature, humidity, batPct);
    advertise(co2, temperature, humidity, batPct);
    Wire.end();
    SPI.end();
    pinMode(SPI_MOSI, INPUT);  // GPIO10 = XIAO user LED (active low); float to reduce sleep current
    esp_deep_sleep(SLEEP_DURATION_US);
}

void loop() {}
