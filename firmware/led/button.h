#pragma once

#include <Arduino.h>

// Momentary push button, active-low with an external pull-up (R8 on the v2.1
// board). Wraps the polling, release-wait, long-press timing, and deep-sleep
// wake wiring so the app logic doesn't repeat raw digitalRead loops.
class Button {
public:
    // Configures the pin with its internal pull-up (harmless alongside the
    // external one) so the line reads high when the button is up.
    explicit Button(int pin);

    // Instantaneous state: true while the button is held down.
    bool isPressed() const;

    // Block until the button is released; returns immediately if already up.
    void waitForRelease() const;

    // Block while the button stays held, up to `ms`. Returns true if it was
    // still down when `ms` elapsed (a long-press), false if released sooner.
    bool heldFor(uint32_t ms) const;

    // Arm the button as an active-low deep-sleep GPIO wake source. Relies on
    // the pin being an RTC-capable GPIO (D3/GPIO5 on the XIAO ESP32-C3).
    void enableWakeup() const;

private:
    int _pin;
};
