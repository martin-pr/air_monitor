#include "button.h"

#include <esp_sleep.h>

Button::Button(int pin) : _pin(pin) {
    pinMode(_pin, INPUT_PULLUP);
}

bool Button::isPressed() const {
    return digitalRead(_pin) == LOW;
}

void Button::waitForRelease() const {
    while (isPressed()) delay(10);
}

bool Button::heldFor(uint32_t ms) const {
    uint32_t start = millis();
    while (isPressed()) {
        if (millis() - start >= ms) return true;
        delay(10);
    }
    return false;
}

void Button::enableWakeup() const {
    // The wake mask is over raw GPIO numbers; on the XIAO C3 core the Dx pin
    // macros equal their GPIO numbers, so 1 << _pin is the right bit.
    esp_deep_sleep_enable_gpio_wakeup(1ULL << _pin, ESP_GPIO_WAKEUP_GPIO_LOW);
}
