#pragma once

#include <cstdint>

#include <Arduino.h>
#include <SensirionI2cScd4x.h>

// SCD41 wrapper. Constructor brings up the I2C bus and puts the sensor into
// a ready-to-measure state.
class Sensor {
public:
    struct Reading {
        uint16_t co2;         // ppm
        float    temperature; // °C, raw sensor reading
        float    humidity;    // % RH
    };

    // GPIOs the I2C peripheral is routed to. Any pair of I/O-capable pins
    // works — nothing is hard-wired on ESP32. Defaults are the board's SDA
    // and SCL macros (D4/GPIO6 and D5/GPIO7 on the XIAO ESP32-C3).
    struct Config {
        int sdaPin = SDA;
        int sclPin = SCL;
    };

    // Datasheet: a single-shot measurement takes ~5s.
    static constexpr uint32_t MEASURE_MS = 5000;

    // firstBoot=true  → clean up any leftover state from before the power cycle
    //                   (also ensures automatic self-calibration is disabled).
    // firstBoot=false → deep-sleep wake path; the sensor was in power-down.
    Sensor(bool firstBoot, const Config& config);
    ~Sensor();

    // Runs a single-shot measurement. Blocks ~MEASURE_MS (the driver waits
    // out the conversion internally), so read() can be called immediately after.
    void startMeasurement();

    Reading read();

    // Forced recalibration (FRC): tell the sensor the true CO2 concentration
    // right now (ppm) and rebase its calibration onto it. Per the SCD4x
    // datasheet the sensor must have been measuring for >=3 min in a stable
    // environment at `referenceCo2` beforehand, and must be in idle mode —
    // which it is after a single-shot read(). Returns true on success, false
    // if the sensor rejects the FRC (e.g. it hadn't been run long enough).
    bool calibrate(uint16_t referenceCo2);

private:
    // Turn off automatic self-calibration (ASC) and persist it to EEPROM.
    void disableAutoCalibration();

    SensirionI2cScd4x _sensor;
};
