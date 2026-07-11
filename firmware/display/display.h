#pragma once

#include <GxEPD2_BW.h>

#include <battery.h>
#include <sensor.h>

class Display {
public:
    // GPIOs the SSD1681 panel and its SPI bus are routed to. Defaults are
    // the pin choices baked into the v1 wiring (see README for the pinout).
    struct Config {
        int cs   = D3;
        int dc   = D2;
        int rst  = D1;
        int busy = D6;
        int sck  = D8;
        int mosi = D10;
    };

    // Brings up SPI and the SSD1681. When firstBoot is true, also does the
    // full-window clear + previous-buffer sync needed to make subsequent
    // partial refreshes coherent.
    Display(bool firstBoot, const Config& config);

    // Releases SPI and floats MOSI to reduce sleep leakage.
    ~Display();

    // Append msg to the rolling boot-diagnostic column and redraw the
    // top of the display.
    void showStatus(const char* msg);

    // Full-window render of the main dashboard.
    void showReading(const Sensor::Reading& reading,
                     const Battery::Status& battery);

private:
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> _panel;
    Config _config;
};
