#include "sensor.h"

#include <Arduino.h>
#include <Wire.h>

Sensor::Sensor(bool firstBoot) {
    Wire.begin();
    _sensor.begin(Wire, SCD41_I2C_ADDR_62);
    if (firstBoot) {
        _sensor.stopPeriodicMeasurement();
        delay(500);
    } else {
        _sensor.wakeUp();
        delay(30);  // datasheet: 30ms after wakeUp before issuing commands
    }
}

Sensor::~Sensor() {
    _sensor.powerDown();
    Wire.end();
}

void Sensor::startMeasurement() {
    _sensor.measureSingleShot();
}

Sensor::Reading Sensor::read() {
    Reading r{ 0, 0.0f, 0.0f };
    _sensor.readMeasurement(r.co2, r.temperature, r.humidity);
    return r;
}

