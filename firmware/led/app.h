#pragma once

namespace app {

// Called once at boot. Drives the RGB LED pins low so the LED starts dark.
void setup();

// Called repeatedly after setup(). Currently: 5 s dark, 0.5 s blue.
void loop();

}  // namespace app
