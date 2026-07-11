#pragma once

#include "battery.h"
#include "sensor.h"

class Display {
public:
    // Brings up SPI and the SSD1681. When firstBoot is true, also does the
    // full-window clear + previous-buffer sync needed to make subsequent
    // partial refreshes coherent.
    explicit Display(bool firstBoot);

    // Releases SPI and floats MOSI to reduce sleep leakage.
    ~Display();

    // Append msg to the rolling boot-diagnostic column and redraw the
    // top of the display.
    void showStatus(const char* msg);

    // Full-window render of the main dashboard.
    void showReading(const Sensor::Reading& reading,
                     const Battery::Status& battery);
};
