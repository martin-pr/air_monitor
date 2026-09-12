#pragma once

#include <Arduino.h>

// Common-cathode 5050 RGB LED driven by three LEDC PWM channels.
// The per-channel drive current is set by the series resistors on
// the board; PWM duty within this class is chosen to perceptually
// balance the mix — see README for values.
class Led {
public:
    // GPIOs the R/G/B anodes are wired to. Red on D10 (220 Ω); the two 47 Ω
    // channels are on D2 and D7. The physical 5050 part has its green and blue
    // dies swapped relative to the schematic's GRN-A/BLU-A pads, so on the
    // assembled board green is D2 and blue is D7 (verified by eye).
    struct Config {
        int red   = D10;
        int green = D2;
        int blue  = D7;
    };

    // Attaches LEDC PWM to the three pins and starts with the LED off.
    Led(const Config& config);

    struct Color {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
    };

    // Logical RGB (pre-scale). The per-channel scaling in led.cpp makes an
    // equal-RGB value perceive as white, so these mix as expected on the eye.
    static constexpr Color WHITE  { 255, 255, 255 };
    static constexpr Color GREEN  {   0, 255,   0 };
    static constexpr Color YELLOW { 255, 255,   0 };
    static constexpr Color ORANGE { 255, 128,   0 };
    static constexpr Color RED    { 255,   0,   0 };
    static constexpr Color OFF    {   0,   0,   0 };

    // Detaches LEDC and floats the pins.
    ~Led();

    void set(const Color& color);

private:
    Config _config;
};
