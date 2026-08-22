#pragma once

#include <Arduino.h>

// Common-cathode 5050 RGB LED driven by three LEDC PWM channels.
// The per-channel drive current is set by the series resistors on
// the board; PWM duty within this class is chosen to perceptually
// balance the mix — see README for values.
class Led {
public:
    // GPIOs the R/G/B anodes are wired to, per the v2.1 schematic:
    // red on D10 (220 Ω), green on D7 (47 Ω), blue on D2 (47 Ω).
    struct Config {
        int red   = D10;
        int green = D7;
        int blue  = D2;
    };

    // Attaches LEDC PWM to the three pins and starts with the LED off.
    Led(const Config& config);

    struct Color {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    static constexpr Color WHITE { 255, 255, 255 };
    static constexpr Color RED   { 255, 0, 0 };
    static constexpr Color OFF   { 0, 0, 0 };

    // Detaches LEDC and floats the pins.
    ~Led();

    void set(const Color& color);

private:
    Config _config;
};
