#pragma once

#include <cstdint>

// LiPo battery on a fixed 1:2 voltage divider. The class owns:
//   - the divider tap pin (ADC-capable)
//   - the LiPo discharge curve used to convert Vbat to a percentage
//   - the "is charging?" heuristic (no VBUS sense wire on v1)
class Battery {
public:
    struct Status {
        uint8_t pct;      // 0–100
        bool    charging; // Vbat > threshold; unreliable during early/mid CC
    };

    explicit Battery(int pin);

    Status read() const;

private:
    int _pin;
};
