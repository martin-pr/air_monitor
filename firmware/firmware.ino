#include <array>
#include <string_view>

#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

constexpr std::string_view SERVICE_UUID        = "f59c6ce6-b894-4e87-9c5b-b347b72c7e93";
constexpr std::string_view CHARACTERISTIC_UUID = "3d455d99-f31a-4826-bf25-7c5f23cedc49";

constexpr uint32_t NOTIFY_INTERVAL_MS = 2000;
constexpr uint32_t BLINK_MS           = 50;
constexpr size_t   JSON_BUF_SIZE      = 64;
constexpr uint16_t BLE_MTU            = 512;

BLECharacteristic *characteristic;
bool deviceConnected = false;
uint32_t counter = 0;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) { deviceConnected = true; }
    void onDisconnect(BLEServer *server) {
        deviceConnected = false;
        BLEDevice::getAdvertising()->start();
    }
};

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.begin(115200);

    BLEDevice::setMTU(BLE_MTU);
    BLEDevice::init("Air Monitor");
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService *service = server->createService(SERVICE_UUID.data());
    characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID.data(),
        BLECharacteristic::PROPERTY_NOTIFY
    );
    characteristic->addDescriptor(new BLE2902());
    service->start();

    BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID.data());
    BLEDevice::getAdvertising()->start();

    Serial.println("BLE advertising as 'Air Monitor'");
}

void loop() {
    static uint32_t lastNotify = 0;
    uint32_t now = millis();

    if (deviceConnected && now - lastNotify >= NOTIFY_INTERVAL_MS) {
        lastNotify = now;

        JsonDocument doc;
        doc["id"] = counter++;
        doc["message"] = "hello world";

        static std::array<char, JSON_BUF_SIZE> buf;
        serializeJson(doc, buf.data(), buf.size());

        characteristic->setValue((uint8_t *)buf.data(), strlen(buf.data()));
        characteristic->notify();
        Serial.println(buf.data());

        digitalWrite(LED_BUILTIN, LOW);
        delay(BLINK_MS);
        digitalWrite(LED_BUILTIN, HIGH);
    }
}
