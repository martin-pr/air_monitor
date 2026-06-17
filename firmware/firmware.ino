#include <array>
#include <string_view>
#include <esp_system.h>

#include <ArduinoJson.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>

#include <WiFi.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_pm.h>

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

constexpr bool BLE_ENABLED = true;

constexpr std::string_view SERVICE_UUID        = "f59c6ce6-b894-4e87-9c5b-b347b72c7e93";  // randomly generated
constexpr std::string_view CHARACTERISTIC_UUID = "3d455d99-f31a-4826-bf25-7c5f23cedc49";  // randomly generated

constexpr uint32_t NOTIFY_INTERVAL_MS   = 30000;  // how often to push sensor data to the client
constexpr uint16_t BLE_CONN_INTERVAL_MIN    = 400;  // units of 1.25ms = 500ms minimum connection interval
constexpr uint16_t BLE_CONN_INTERVAL_MAX    = 800;  // units of 1.25ms = 1s maximum connection interval
constexpr uint16_t BLE_SUPERVISION_TIMEOUT  = 500;  // units of 10ms; only fires on unclean drops — clean disconnects are immediate
constexpr uint32_t SUBSCRIBE_DELAY_MS   = 5000;   // wait after connect before first notify; must cover CCCD write at 1s connection interval
constexpr size_t   JSON_BUF_SIZE        = 64;     // max bytes for serialized JSON payload
constexpr uint16_t BLE_MTU              = 512;    // requested ATT MTU; negotiated with client at connect time

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

// SCD41 data-ready polling
constexpr uint32_t SCD4X_READY_TIMEOUT_MS = 2000;  // max wait per cycle for a fresh measurement
constexpr uint32_t SCD4X_READY_POLL_MS    = 100;

// Loop timing
constexpr uint32_t LOOP_TICK_MS         = 100;
constexpr uint32_t RESTART_ADV_DELAY_MS = 500;

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

BLECharacteristic *characteristic;
BLEServer *bleServer;

volatile bool     deviceConnected    = false;
volatile bool     restartAdvertising = false;

volatile uint32_t connectedAt        = 0;
uint32_t lastNotify = 0;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);


class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onStatus(BLECharacteristic *c, Status s, uint32_t code) {
        if (s == SUCCESS_NOTIFY) {
            connectedAt = 0;
            lastNotify  = millis();
        } else if (s == ERROR_NO_SUBSCRIBER) {
            connectedAt = millis();  // retry after SUBSCRIBE_DELAY_MS
        } else if (s == ERROR_GATT || s == ERROR_NO_CLIENT) {
            deviceConnected    = false;
            restartAdvertising = true;
        }
    }
};

class CCCDCallbacks : public BLEDescriptorCallbacks {
    void onWrite(BLEDescriptor *desc) {
        uint8_t *val = desc->getValue();
        if (val && val[0] == 0x01) {  // client enabled notifications
            connectedAt = millis();
        }
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) {
        deviceConnected = true;
        connectedAt     = millis();
        bleServer->requestConnParams(bleServer->getConnId(), BLE_CONN_INTERVAL_MIN, BLE_CONN_INTERVAL_MAX, 0, BLE_SUPERVISION_TIMEOUT);
    }
    void onDisconnect(BLEServer *server) { deviceConnected = false; restartAdvertising = true; }
};

void showStatus(const char* msg) {
    static std::array<std::string, MAX_STATUS_LINES> lines;
    static size_t lineCount = 0;

    if (lineCount < MAX_STATUS_LINES) {
        lines[lineCount++] = msg;
    }

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
}

void updateDisplay(uint16_t co2, float temperature, float humidity, uint8_t batPct) {
    char co2Str[8], tempStr[8], rhStr[12], batStr[6];
    snprintf(co2Str, sizeof(co2Str), "%d", co2);
    snprintf(tempStr, sizeof(tempStr), "%.1f", temperature);
    snprintf(rhStr,  sizeof(rhStr),  "%.1f%%", humidity);
    snprintf(batStr, sizeof(batStr), "%d%%", batPct);

    int16_t x1, y1;
    uint16_t tw, th;

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
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
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
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        display.getTextBounds("CO2 (ppm)", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(EPD_W - EPD_MARGIN - (int16_t)tw, EPD_BOTTOM_Y);
        display.print("CO2 (ppm)");

    } while (display.nextPage());
}

void sendNotification() {
    // Statics retain the last good reading so the display never shows zeros
    // after the first successful measurement.
    static uint16_t co2 = 0;
    static float temperature = 0.0f, humidity = 0.0f;
    uint8_t batPct = readBatteryPercent();

    // The SCD41 runs on its own autonomous 30s clock, independent of our
    // notify interval. Poll briefly so a small phase offset doesn't cause
    // us to arrive just before data is ready.
    bool dataReady = false;
    uint32_t start = millis();
    while (!dataReady && millis() - start < SCD4X_READY_TIMEOUT_MS) {
        scd4x.getDataReadyStatus(dataReady);
        if (!dataReady) delay(SCD4X_READY_POLL_MS);
    }
    if (dataReady) {
        scd4x.readMeasurement(co2, temperature, humidity);
    }
    Serial.printf("scd4x: co2=%d temp=%.1f rh=%.1f%s\n", co2, temperature, humidity,
                  dataReady ? "" : " [cached]");

    JsonDocument doc;
    doc["co2"]  = co2;
    doc["temp"] = round(temperature * 10) / 10.0f;
    doc["rh"]   = round(humidity * 10) / 10.0f;
    doc["bat"]  = batPct;

    static std::array<char, JSON_BUF_SIZE> buf;
    serializeJson(doc, buf.data(), buf.size());

    if (BLE_ENABLED) {
        characteristic->setValue((uint8_t *)buf.data(), strlen(buf.data()));
        characteristic->notify();
    }
    Serial.println(buf.data());

    updateDisplay(co2, temperature, humidity, batPct);
}

void setup() {
    Serial.begin(115200);

    analogSetAttenuation(ADC_11db);  // 0–3.9 V range; covers 1.5–2.1 V from the battery divider

    SPI.begin(SPI_SCK, /*MISO=*/-1, SPI_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(1);

    display.setFullWindow();
    display.firstPage();
    do { display.fillScreen(GxEPD_WHITE); } while (display.nextPage());
    display.epd2.writeScreenBufferAgain();  // keep SSD1681 current/previous RAM in sync after clear
    delay(STATUS_STEP_DELAY_MS);

    // Show reset reason so we know if it's brownout, crash, or watchdog
    switch (esp_reset_reason()) {
        case ESP_RST_BROWNOUT: showStatus("RST: brownout");   break;
        case ESP_RST_PANIC:    showStatus("RST: panic");      break;
        case ESP_RST_TASK_WDT: showStatus("RST: task wdt");   break;
        case ESP_RST_INT_WDT:  showStatus("RST: int wdt");    break;
        case ESP_RST_WDT:      showStatus("RST: rtc wdt");    break;
        case ESP_RST_SW:       showStatus("RST: software");   break;
        default:               showStatus("RST: poweron/pin"); break;
    }

    showStatus("1: display ok");

    // Bring the display up before starting the SCD41 measurement cycle, so
    // boot diagnostics remain visible even if sensor startup stresses power.
    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.stopPeriodicMeasurement();  // clear any leftover state from before power cycle
    scd4x.startLowPowerPeriodicMeasurement();

    if (BLE_ENABLED) {
        showStatus("2: wifi off...");
        WiFi.mode(WIFI_OFF);
        delay(200);

        showStatus("3: ble init...");
        BLEDevice::setMTU(BLE_MTU);
        BLEDevice::init("Air Monitor");

        showStatus("4: ble server...");
        bleServer = BLEDevice::createServer();
        bleServer->setCallbacks(new ServerCallbacks());

        showStatus("5: gatt service...");
        BLEService *service = bleServer->createService(SERVICE_UUID.data());
        characteristic = service->createCharacteristic(
            CHARACTERISTIC_UUID.data(),
            BLECharacteristic::PROPERTY_NOTIFY
        );
        characteristic->setCallbacks(new CharacteristicCallbacks());
        BLE2902 *cccd = new BLE2902();
        cccd->setCallbacks(new CCCDCallbacks());
        characteristic->addDescriptor(cccd);
        service->start();

        showStatus("6: advertising...");
        BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID.data());
        BLEDevice::getAdvertising()->setMinInterval(1600);  // 1s (units of 0.625ms)
        BLEDevice::getAdvertising()->setMaxInterval(2000);  // 1.25s
        BLEDevice::getAdvertising()->start();

        showStatus("7: ready");
    }

    esp_pm_config_t pm = {
        .max_freq_mhz       = 80,   // reduce from 240MHz — plenty for BLE
        .min_freq_mhz       = 40,
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm);

    connectedAt = millis();
    lastNotify = millis();
    Serial.println("BLE advertising as 'Air Monitor'");
}

void loop() {
    if (BLE_ENABLED && restartAdvertising) {
        restartAdvertising = false;
        delay(RESTART_ADV_DELAY_MS);
        BLEDevice::getAdvertising()->start();
    }

    delay(LOOP_TICK_MS);

    uint32_t now = millis();
    if (BLE_ENABLED && deviceConnected) {
        if (connectedAt > 0 && now - connectedAt >= SUBSCRIBE_DELAY_MS) {
            sendNotification();
        } else if (connectedAt == 0 && now - lastNotify >= NOTIFY_INTERVAL_MS) {
            lastNotify = now;
            sendNotification();
        }
    } else if (now - lastNotify >= NOTIFY_INTERVAL_MS) {
        lastNotify = now;
        sendNotification();
    }
}
