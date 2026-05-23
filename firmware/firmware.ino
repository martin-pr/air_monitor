#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#define SERVICE_UUID        "f59c6ce6-b894-4e87-9c5b-b347b72c7e93"
#define CHARACTERISTIC_UUID "3d455d99-f31a-4826-bf25-7c5f23cedc49"

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);

    BLEDevice::init("Air Monitor");
    BLEServer *server = BLEDevice::createServer();
    BLEService *service = server->createService(SERVICE_UUID);
    BLECharacteristic *characteristic = service->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    characteristic->setValue("hello world");
    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();

    Serial.println("BLE advertising as 'Air Monitor'");
}

void loop() {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
    Serial.println("blink");
}
