#include "sensor.h"

#include <Arduino.h>
#include <Wire.h>

Sensor::Sensor(bool firstBoot, const Config& config) {
    Wire.begin(config.sdaPin, config.sclPin);
    _sensor.begin(Wire, SCD41_I2C_ADDR_62);
    if (firstBoot) {
        _sensor.stopPeriodicMeasurement();
        delay(500);
        disableAutoCalibration();  // idle mode required — we're stopped here
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

void Sensor::disableAutoCalibration() {
    // ASC assumes the sensor regularly sees fresh ~400ppm air and rebaselines
    // toward it. In our infrequent single-shot use that assumption fails and
    // the reading drifts, so we keep ASC off and rely on manual calibrate()
    // (FRC) instead. Persisted to EEPROM so it survives power cycles; only
    // rewritten when actually still on, to spare the EEPROM's write endurance.
    uint16_t enabled = 1;
    if (_sensor.getAutomaticSelfCalibrationEnabled(enabled) != 0) return;
    if (!enabled) return;  // already off — don't burn an EEPROM write

    _sensor.setAutomaticSelfCalibrationEnabled(0);
    _sensor.persistSettings();  // writes EEPROM; the driver waits out the ~800ms
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

