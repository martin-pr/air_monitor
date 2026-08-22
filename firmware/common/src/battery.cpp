#include "battery.h"

#include <Arduino.h>
#include <array>
#include <cstddef>

namespace {

struct Point { float voltage; uint8_t pct; };

// LiPo discharge curve — steeper drops at the ends, flat middle. Read at the
// battery terminal, i.e. after multiplying the divider tap by 2.
constexpr std::array<Point, 4> CURVE = {{
    {4.20f, 100},
    {3.98f,  80},
    {3.52f,  20},
    {3.00f,   0},
}};

// Fallback for boards without a VBUS-sense wire. The on-board charge IC holds
// Vbat near 4.20 V during the constant-voltage tail of a charge, so anything
// above 4.10 V is "charging or topped off". False negatives during the
// early/mid constant-current phase are unavoidable with this approach.
constexpr float CHARGING_THRESHOLD_V = 4.10f;

// VBUS divider midpoint reads ~2.5 V when USB is plugged in, 0 V otherwise.
// Threshold at ~1 V leaves plenty of headroom.
constexpr uint32_t VBUS_THRESHOLD_MV = 1000;

}  // namespace

Battery::Battery(const Config& config) : _config(config) {
    // 0–3.9 V input range at each pin; covers both the battery divider
    // (1.5–2.1 V) and the VBUS divider (~2.5 V when USB in).
    analogSetPinAttenuation(_config.batteryPin, ADC_11db);
    if (_config.vbusPin >= 0) {
        analogSetPinAttenuation(_config.vbusPin, ADC_11db);
    }
}

bool Battery::usbConnected() const {
    if (_config.vbusPin >= 0) {
        return analogReadMilliVolts(_config.vbusPin) > VBUS_THRESHOLD_MV;
    }
    float vbat = analogReadMilliVolts(_config.batteryPin) * 2.0f / 1000.0f;
    return vbat > CHARGING_THRESHOLD_V;
}

Battery::Status Battery::read() const {
    float vbat = analogReadMilliVolts(_config.batteryPin) * 2.0f / 1000.0f;
    bool charging = usbConnected();
    if (vbat >= CURVE.front().voltage) return { 100, charging };
    if (vbat <= CURVE.back().voltage)  return {   0, charging };
    for (size_t i = 0; i < CURVE.size() - 1; i++) {
        if (vbat >= CURVE[i + 1].voltage) {
            float t = (vbat - CURVE[i + 1].voltage) /
                      (CURVE[i].voltage - CURVE[i + 1].voltage);
            uint8_t pct = (uint8_t)(CURVE[i + 1].pct + t * (CURVE[i].pct - CURVE[i + 1].pct));
            return { pct, charging };
        }
    }
    return { 0, charging };
}
