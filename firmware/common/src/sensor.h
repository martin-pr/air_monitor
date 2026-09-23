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

    // firstBoot=true  → clean up any leftover state from before the power cycle.
    // firstBoot=false → deep-sleep wake path; the sensor was in power-down.
    Sensor(bool firstBoot, const Config& config);
    ~Sensor();

    // Runs a single-shot measurement. Blocks ~MEASURE_MS (the driver waits
    // out the conversion internally), so read() can be called immediately after.
    void startMeasurement();

    Reading read();

private:
    SensirionI2cScd4x _sensor;
};
