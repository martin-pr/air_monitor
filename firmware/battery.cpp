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

// Without a VBUS sense wire we infer charging from battery voltage. The on-board
// charge IC holds Vbat near 4.20 V during the constant-voltage tail of a charge,
// so anything above 4.10 V is "charging or topped off". False negatives during
// the early/mid constant-current phase are unavoidable with this approach.
constexpr float CHARGING_THRESHOLD_V = 4.10f;

}  // namespace

Battery::Battery(int pin) : _pin(pin) {
    // 0–3.9 V input range at the pin; covers the 1.5–2.1 V range the divider produces.
    analogSetPinAttenuation(_pin, ADC_11db);
}

Battery::Status Battery::read() const {
    float vbat = analogReadMilliVolts(_pin) * 2.0f / 1000.0f;
    bool charging = vbat > CHARGING_THRESHOLD_V;
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
