#include <array>
#include <string_view>

#include <ArduinoJson.h>
#include <Wire.h>
#include <SensirionI2cScd4x.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_pm.h>

#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

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

void updateDisplay(uint16_t co2, float temperature, float humidity) {
    char co2Str[8], tempStr[8], rhStr[12];
    snprintf(co2Str, sizeof(co2Str), "%d", co2);
    snprintf(tempStr, sizeof(tempStr), "%.1f", temperature);
    snprintf(rhStr,  sizeof(rhStr),  "%.1f%%", humidity);

    int16_t x1, y1;
    uint16_t tw, th;

    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);

        // Top-left: temperature + degree circle + C
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        display.setCursor(4, 22);
        display.print(tempStr);
        int16_t cx = display.getCursorX();
        display.drawCircle(cx + 3, 8, 2, GxEPD_BLACK);  // degree symbol
        display.setCursor(cx + 7, 22);
        display.print("C");

        // Top-right: humidity, right-aligned
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        display.getTextBounds(rhStr, 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(196 - (int16_t)tw, 22);
        display.print(rhStr);

        // Center: large CO2 number (24pt × 2)
        display.setFont(&FreeSansBold24pt7b);
        display.setTextSize(2);
        display.getTextBounds(co2Str, 0, 0, &x1, &y1, &tw, &th);
        display.setCursor((200 - (int16_t)tw) / 2 - x1,
                          103 - y1 - (int16_t)th / 2);
        display.print(co2Str);

        // Bottom-right: "CO2 (ppm)" label
        display.setFont(&FreeSans12pt7b);
        display.setTextSize(1);
        display.getTextBounds("CO2 (ppm)", 0, 0, &x1, &y1, &tw, &th);
        display.setCursor(196 - (int16_t)tw, 196);
        display.print("CO2 (ppm)");

    } while (display.nextPage());
}

void sendNotification() {
    uint16_t co2 = 0;
    float temperature = 0.0f, humidity = 0.0f;
    bool dataReady = false;
    scd4x.getDataReadyStatus(dataReady);
    if (dataReady) {
        scd4x.readMeasurement(co2, temperature, humidity);
    }
    Serial.printf("scd4x: co2=%d temp=%.1f rh=%.1f\n", co2, temperature, humidity);

    JsonDocument doc;
    doc["co2"]  = co2;
    doc["temp"] = serialized(String(temperature, 1));
    doc["rh"]   = serialized(String(humidity, 1));

    static std::array<char, JSON_BUF_SIZE> buf;
    serializeJson(doc, buf.data(), buf.size());

    characteristic->setValue((uint8_t *)buf.data(), strlen(buf.data()));
    characteristic->notify();
    Serial.println(buf.data());

    updateDisplay(co2, temperature, humidity);
}

void setup() {
    Serial.begin(115200);

    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.stopPeriodicMeasurement();  // clear any leftover state from before power cycle
    scd4x.startLowPowerPeriodicMeasurement();

    SPI.begin(SPI_SCK, /*MISO=*/-1, SPI_MOSI, EPD_CS);
    display.init(115200);
    display.setRotation(1);

    // Splash screen while BLE initialises
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSansBold24pt7b);
        display.setTextSize(1);
        display.setCursor(10, 110);
        display.print("Air");
        display.setCursor(10, 150);
        display.print("Monitor");
    } while (display.nextPage());

    BLEDevice::setMTU(BLE_MTU);
    BLEDevice::init("Air Monitor");
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

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

    BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID.data());
    BLEDevice::getAdvertising()->start();

    esp_pm_config_t pm = {
        .max_freq_mhz       = 80,   // reduce from 240MHz — plenty for BLE
        .min_freq_mhz       = 40,
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm);

    Serial.println("BLE advertising as 'Air Monitor'");
}

void loop() {
    if (restartAdvertising) {
        restartAdvertising = false;
        delay(500);
        BLEDevice::getAdvertising()->start();
    }

    delay(100);

    if (deviceConnected) {
        uint32_t now2 = millis();
        if (connectedAt > 0 && now2 - connectedAt >= SUBSCRIBE_DELAY_MS) {
            sendNotification();
        } else if (connectedAt == 0 && now2 - lastNotify >= NOTIFY_INTERVAL_MS) {
            lastNotify = now2;
            sendNotification();
        }
    }
}
