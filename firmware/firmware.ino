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

constexpr std::string_view SERVICE_UUID        = "f59c6ce6-b894-4e87-9c5b-b347b72c7e93";  // randomly generated
constexpr std::string_view CHARACTERISTIC_UUID = "3d455d99-f31a-4826-bf25-7c5f23cedc49";  // randomly generated

constexpr uint32_t NOTIFY_INTERVAL_MS   = 30000;  // how often to push sensor data to the client
constexpr uint16_t BLE_CONN_INTERVAL_MIN    = 400;  // units of 1.25ms = 500ms minimum connection interval
constexpr uint16_t BLE_CONN_INTERVAL_MAX    = 800;  // units of 1.25ms = 1s maximum connection interval
constexpr uint16_t BLE_SUPERVISION_TIMEOUT  = 500;  // units of 10ms; only fires on unclean drops — clean disconnects are immediate
constexpr uint32_t SUBSCRIBE_DELAY_MS   = 5000;   // wait after connect before first notify; must cover CCCD write at 1s connection interval
constexpr size_t   JSON_BUF_SIZE        = 64;     // max bytes for serialized JSON payload
constexpr uint16_t BLE_MTU              = 512;    // requested ATT MTU; negotiated with client at connect time

SensirionI2cScd4x scd4x;

BLECharacteristic *characteristic;
BLEServer *bleServer;

volatile bool     deviceConnected    = false;
volatile bool     restartAdvertising = false;

volatile uint32_t connectedAt        = 0;
uint32_t lastNotify = 0;


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
}

void setup() {
    Serial.begin(115200);

    Wire.begin();
    scd4x.begin(Wire, SCD41_I2C_ADDR_62);
    scd4x.stopPeriodicMeasurement();  // clear any leftover state from before power cycle
    scd4x.startLowPowerPeriodicMeasurement();

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
