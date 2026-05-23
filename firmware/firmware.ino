#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID        "f59c6ce6-b894-4e87-9c5b-b347b72c7e93"
#define CHARACTERISTIC_UUID "3d455d99-f31a-4826-bf25-7c5f23cedc49"

#define NOTIFY_INTERVAL_MS 2000
#define BLINK_MS           50

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

    BLEDevice::init("Air Monitor");
    BLEServer *server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    BLEService *service = server->createService(SERVICE_UUID);
    characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    characteristic->addDescriptor(new BLE2902());
    service->start();

    BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
    BLEDevice::getAdvertising()->start();

    Serial.println("BLE advertising as 'Air Monitor'");
}

void loop() {
    static uint32_t lastNotify = 0;
    uint32_t now = millis();

    if (deviceConnected && now - lastNotify >= NOTIFY_INTERVAL_MS) {
        lastNotify = now;

        char buf[32];
        snprintf(buf, sizeof(buf), "hello world %lu", counter++);
        characteristic->setValue((uint8_t *)buf, strlen(buf));
        characteristic->notify();
        Serial.println(buf);

        digitalWrite(LED_BUILTIN, LOW);
        delay(BLINK_MS);
        digitalWrite(LED_BUILTIN, HIGH);
    }
}
