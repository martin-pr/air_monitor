#pragma once

#include <cstdint>

// LiPo battery on a fixed 1:2 voltage divider. The class owns:
//   - the divider tap pin (ADC-capable)
//   - the LiPo discharge curve used to convert Vbat to a percentage
//   - the "is charging?" determination
//
// If a VBUS-sense pin is configured, charging is read directly from it.
// Otherwise it's inferred from Vbat > threshold (unreliable during
// early/mid CC phase but the only option without a VBUS wire — v1).
class Battery {
public:
    struct Config {
        int batteryPin;         // ADC pin on the divider midpoint
        int vbusPin = -1;       // ADC pin on the VBUS divider; -1 = infer from Vbat
    };

    struct Status {
        uint8_t pct;      // 0–100
        bool    charging;
    };

    explicit Battery(const Config& config);

    Status read() const;

    // Fast USB-present check. Reads only the VBUS pin if configured,
    // otherwise falls back to the same Vbat-threshold inference used by
    // Status.charging.
    bool usbConnected() const;

private:
    Config _config;
};
