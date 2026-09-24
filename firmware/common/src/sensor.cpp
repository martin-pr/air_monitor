#include "sensor.h"

#include <Arduino.h>
#include <Wire.h>

Sensor::Sensor(bool firstBoot, const Config& config) {
    Wire.begin(config.sdaPin, config.sclPin);
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

bool Sensor::calibrate(uint16_t referenceCo2) {
    // FRC is only valid in idle mode; make sure no periodic measurement is
    // running before issuing it (datasheet: stop, then recalibrate).
    _sensor.stopPeriodicMeasurement();
    delay(500);

    uint16_t frcCorrection = 0;
    int16_t error = _sensor.performForcedRecalibration(referenceCo2, frcCorrection);
    // 0xFFFF in the correction word means the sensor rejected the FRC because
    // it hadn't been operated in a stable environment beforehand.
    return error == 0 && frcCorrection != 0xFFFF;
}

