#pragma once

#include <cstdint>

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

    // Datasheet: a single-shot measurement takes ~5s.
    static constexpr uint32_t MEASURE_MS = 5000;

    // firstBoot=true  → clean up any leftover state from before the power cycle.
    // firstBoot=false → deep-sleep wake path; the sensor was in power-down.
    explicit Sensor(bool firstBoot);
    ~Sensor();

    // Kicks off a measurement. Returns immediately; caller must wait
    // MEASURE_MS before calling read().
    void startMeasurement();

    Reading read();

private:
    SensirionI2cScd4x _sensor;
};
