#pragma once

namespace app {

// One boot cycle: bring up hardware, take a measurement, advertise, deep-sleep.
// Does not return — deep_sleep resets the chip.
[[noreturn]] void setup();

}  // namespace app
