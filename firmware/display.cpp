#include "display.h"

#include <array>
#include <cstdio>
#include <string>

#include <Arduino.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

namespace {

// ePaper pins (XIAO ESP32-C3).
constexpr int EPD_CS   = 5;   // D3
constexpr int EPD_DC   = 4;   // D2
constexpr int EPD_RST  = 3;   // D1
constexpr int EPD_BUSY = 21;  // D6
constexpr int SPI_SCK  = 8;   // D8
constexpr int SPI_MOSI = 10;  // D10

// ePaper layout (200×200 square panel).
constexpr int EPD_W        = 200;
constexpr int EPD_H        = 200;
constexpr int EPD_MARGIN   = 4;             // inset from all edges
constexpr int EPD_TOP_Y    = 22;            // baseline for top-row labels
constexpr int EPD_BOTTOM_Y = EPD_H - EPD_MARGIN;  // baseline for bottom-row label
constexpr int EPD_CO2_Y    = 103;           // vertical centre target for large CO2 number

// Boot-status column.
constexpr int      STATUS_FIRST_Y       = 20;
constexpr int      STATUS_LINE_STEP     = 29;   // FreeSans12pt7b yAdvance
constexpr uint32_t STATUS_STEP_DELAY_MS = 500;  // required to avoid power spikes
constexpr size_t   MAX_STATUS_LINES     = 8;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> panel(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

}  // namespace

Display::Display(bool firstBoot) {

    SPI.begin(SPI_SCK, /*MISO=*/-1, SPI_MOSI, EPD_CS);
    panel.init(115200);
    panel.setRotation(1);

    if (firstBoot) {
        panel.setFullWindow();
        panel.firstPage();
        do { panel.fillScreen(GxEPD_WHITE); } while (panel.nextPage());
        // Sync SSD1681 current/previous RAM after the clear so the first
        // partial refresh has coherent state to diff against.
        panel.epd2.writeScreenBufferAgain();
        panel.hibernate();
    }
}

Display::~Display() {
    SPI.end();
    // GPIO10 = XIAO's SPI MOSI, float it before deep sleep to reduce leakage.
    pinMode(SPI_MOSI, INPUT);
}

void Display::showStatus(const char* msg) {

    static std::array<std::string, MAX_STATUS_LINES> lines;
    static size_t lineCount = 0;

    if (lineCount < MAX_STATUS_LINES) {
        lines[lineCount++] = msg;
    }

    panel.init(115200, false);
    panel.setRotation(1);

    uint16_t wh = STATUS_FIRST_Y + lineCount * STATUS_LINE_STEP;
    if (wh > EPD_H) {
        wh = EPD_H;
    }
    panel.setPartialWindow(0, 0, EPD_W, wh);

    panel.firstPage();
    do {
        panel.fillScreen(GxEPD_WHITE);
        panel.setTextColor(GxEPD_BLACK);
        panel.setFont(&FreeSans12pt7b);
        panel.setTextSize(1);
        for (size_t i = 0; i < lineCount; i++) {
            panel.setCursor(EPD_MARGIN, STATUS_FIRST_Y + i * STATUS_LINE_STEP);
            panel.print(lines[i].c_str());
        }
    } while (panel.nextPage());
    delay(STATUS_STEP_DELAY_MS);
    panel.hibernate();
}

void Display::showReading(const Sensor::Reading& reading,
                          const Battery::Status& battery) {

    char co2Str[8], tempStr[8], rhStr[12], batStr[6];
    snprintf(co2Str,  sizeof(co2Str),  "%d",     reading.co2);
    snprintf(tempStr, sizeof(tempStr), "%.1f",   reading.temperature);
    snprintf(rhStr,   sizeof(rhStr),   "%.1f%%", reading.humidity);
    snprintf(batStr,  sizeof(batStr),  "%d%%",   battery.pct);

    int16_t  x1, y1;
    uint16_t tw, th;

    panel.init(115200, false);
    panel.setRotation(1);
    panel.setFullWindow();
    panel.firstPage();
    do {
        panel.fillScreen(GxEPD_WHITE);
        panel.setTextColor(GxEPD_BLACK);

        // Top-left: temperature + degree circle + C
        // FreeSans doesn't include 0xB0 (degree); draw a small circle instead
        panel.setFont(&FreeSans12pt7b);
        panel.setTextSize(1);
        panel.setCursor(EPD_MARGIN, EPD_TOP_Y);
        panel.print(tempStr);
        int16_t cx = panel.getCursorX();
        panel.drawCircle(cx + 3, 8, 2, GxEPD_BLACK);
        panel.setCursor(cx + 7, EPD_TOP_Y);
        panel.print("C");

        // Top-right: humidity, right-aligned
        panel.getTextBounds(rhStr, 0, 0, &x1, &y1, &tw, &th);
        panel.setCursor(EPD_W - EPD_MARGIN - (int16_t)tw, EPD_TOP_Y);
        panel.print(rhStr);

        // Centre: large CO2 number (24pt × 2 via setTextSize)
        panel.setFont(&FreeSansBold24pt7b);
        panel.setTextSize(2);
        panel.getTextBounds(co2Str, 0, 0, &x1, &y1, &tw, &th);
        panel.setCursor((EPD_W - (int16_t)tw) / 2 - x1,
                        EPD_CO2_Y - y1 - (int16_t)th / 2);
        panel.print(co2Str);

        // Bottom-left: battery %
        panel.setFont(&FreeSans12pt7b);
        panel.setTextSize(1);
        panel.setCursor(EPD_MARGIN, EPD_BOTTOM_Y);
        panel.print(batStr);

        // Bottom-right: unit label
        panel.getTextBounds("CO2 (ppm)", 0, 0, &x1, &y1, &tw, &th);
        panel.setCursor(EPD_W - EPD_MARGIN - (int16_t)tw, EPD_BOTTOM_Y);
        panel.print("CO2 (ppm)");

    } while (panel.nextPage());
    panel.hibernate();
}
