#pragma once

namespace app {

// One boot cycle: bring up hardware, measure, flash the CO2 colour, advertise.
// On battery this then deep-sleeps and never returns; on USB it returns so
// loop() can hold the idle state and re-run the cycle.
void setup();

// USB-only cadence: holds the idle blue and re-runs the cycle every 5 min or
// on a button press. Never reached on battery (setup() deep-sleeps instead).
void loop();

}  // namespace app
