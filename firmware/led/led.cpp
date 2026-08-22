#include "led.h"

namespace {

constexpr uint32_t PWM_FREQ_HZ  = 5000;
constexpr uint8_t  PWM_RES_BITS = 8;

// Per-channel scaling to compensate for asymmetric series resistors and
// per-wavelength eye response. Input Color is in "logical" 0..255 space
// (so Led::WHITE = {255,255,255} means "full white"); actual PWM duty is
// input * scale / 255. Red is at full scale because it's the dimmest per
// mA on this board; green and blue are cut back so an equal-RGB input
// perceives as white instead of cyan.
constexpr uint8_t RED_SCALE   = 255;
constexpr uint8_t GREEN_SCALE = 90;
constexpr uint8_t BLUE_SCALE  = 130;

inline uint8_t scale(uint8_t v, uint8_t s) {
    return static_cast<uint8_t>((static_cast<uint16_t>(v) * s) / 255);
}

}  // namespace

Led::Led(const Config& config) : _config(config) {
    ledcAttach(_config.red,   PWM_FREQ_HZ, PWM_RES_BITS);
    ledcAttach(_config.green, PWM_FREQ_HZ, PWM_RES_BITS);
    ledcAttach(_config.blue,  PWM_FREQ_HZ, PWM_RES_BITS);
    set(OFF);
}

Led::~Led() {
    set(OFF);
    ledcDetach(_config.red);
    ledcDetach(_config.green);
    ledcDetach(_config.blue);
    pinMode(_config.red,   INPUT);
    pinMode(_config.green, INPUT);
    pinMode(_config.blue,  INPUT);
}

void Led::set(const Color& color) {
    ledcWrite(_config.red,   scale(color.red,   RED_SCALE));
    ledcWrite(_config.green, scale(color.green, GREEN_SCALE));
    ledcWrite(_config.blue,  scale(color.blue,  BLUE_SCALE));
}
