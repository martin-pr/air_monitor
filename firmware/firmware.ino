#include <array>
#include <string_view>

#include <ArduinoJson.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

constexpr std::string_view SERVICE_UUID        = "f59c6ce6-b894-4e87-9c5b-b347b72c7e93";  // randomly generated
constexpr std::string_view CHARACTERISTIC_UUID = "3d455d99-f31a-4826-bf25-7c5f23cedc49";  // randomly generated

constexpr uint32_t NOTIFY_INTERVAL_MS   = 30000;  // how often to push sensor data to the client
constexpr uint16_t BLE_SUPERVISION_TIMEOUT = 500; // units of 10ms; only fires on unclean drops — clean disconnects are immediate
constexpr uint32_t CONNECT_DELAY_MS     = 1000;   // wait after connect before first notify, to allow client to subscribe
constexpr uint32_t BLINK_MS             = 50;     // LED on duration per notification blink
constexpr uint32_t ADVERTISE_BLINK_MS   = 100;    // LED on duration per advertising blink
constexpr uint32_t ADVERTISE_PERIOD_MS  = 1000;   // how often to blink while advertising
constexpr size_t   JSON_BUF_SIZE        = 64;     // max bytes for serialized JSON payload
constexpr uint16_t BLE_MTU              = 512;    // requested ATT MTU; negotiated with client at connect time

BLECharacteristic *characteristic;
BLEServer *bleServer;

volatile bool     deviceConnected    = false;
volatile bool     restartAdvertising = false;

volatile uint32_t connectedAt        = 0;
uint32_t lastNotify = 0;

uint32_t counter    = 0;

class CharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onStatus(BLECharacteristic *c, Status s, uint32_t code) {
        if (s == ERROR_GATT || s == ERROR_NO_CLIENT) {
            deviceConnected    = false;
            restartAdvertising = true;
        }
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *server) {
        deviceConnected = true;
        connectedAt     = millis();
        bleServer->requestConnParams(bleServer->getConnId(), 6, 12, 0, BLE_SUPERVISION_TIMEOUT);
    }
    void onDisconnect(BLEServer *server) { deviceConnected = false; restartAdvertising = true; }
};

void sendNotification() {
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

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.begin(115200);

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
    characteristic->addDescriptor(new BLE2902());
    service->start();

    BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID.data());
    BLEDevice::getAdvertising()->start();

    Serial.println("BLE advertising as 'Air Monitor'");
}

void loop() {
    uint32_t now = millis();

    if (restartAdvertising) {
        restartAdvertising = false;
        delay(500);
        BLEDevice::getAdvertising()->start();
    }

    if (!deviceConnected) {
        static uint32_t lastAdvertiseBlink = 0;
        if (now - lastAdvertiseBlink >= ADVERTISE_PERIOD_MS) {
            lastAdvertiseBlink = now;
            digitalWrite(LED_BUILTIN, LOW);
            delay(ADVERTISE_BLINK_MS);
            digitalWrite(LED_BUILTIN, HIGH);
        }
    }

    delay(10);

    if (deviceConnected) {
        uint32_t now2 = millis();
        if (connectedAt > 0 && now2 - connectedAt >= CONNECT_DELAY_MS) {
            connectedAt = 0;
            lastNotify  = now2;
            sendNotification();
        } else if (connectedAt == 0 && now2 - lastNotify >= NOTIFY_INTERVAL_MS) {
            lastNotify = now2;
            sendNotification();
        }
    }
}
