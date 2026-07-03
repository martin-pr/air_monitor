#pragma once

#include <cstdint>

#include <esp_system.h>

#include "battery.h"
#include "sensor.h"

// Beacon advertisement — manufacturer-specific data (10 bytes, little-endian):
//   [0-1]  company ID: 0x41 0x4D ('AM')
//   [2]    protocol version; bump when layout changes
//   [3]    status: esp_reset_reason_t as uint8 (8 = ESP_RST_DEEPSLEEP = normal wake;
//          anything else means the chip cold-booted — power-on, brownout, panic, wdt, etc.)
//   [4-5]  CO2 in ppm (uint16)
//   [6-7]  temperature in 0.01 °C (int16)
//   [8]    relative humidity in % (uint8)
//   [9]    battery percent (uint8); 0xFF = charging (battery reading is meaningless during charge)
//
// Parsing rule: read [2] first; only parse further fields if version is known.
class Ble {
public:
    // Emit one beacon cycle for the given reading. Blocks for ADV_DURATION_MS.
    // Brings BLE up on entry and tears it down on return.
    static void advertise(const Sensor::Reading& reading,
                          const Battery::Status& battery,
                          esp_reset_reason_t     status);
};
