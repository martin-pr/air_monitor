#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSans9pt7b.h>

#define EPD_CS   5
#define EPD_DC   4
#define EPD_RST  3
#define EPD_BUSY 2
#define SPI_SCK  8
#define SPI_MOSI 10
#define SPI_MISO 9

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

void drawTest(const char *label) {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setCursor(10, 60);
        display.print("GxEPD2_154_D67");
        display.setCursor(10, 90);
        display.print(label);
        display.fillRect(10, 110, 180, 40, GxEPD_BLACK);
    } while (display.nextPage());
}

void setup() {
    Serial.begin(115200);
    delay(3000);  // wait for USB-JTAG to attach
    Serial.println("=== ePaper probe start ===");

    Serial.printf("BUSY pin %d state before init: %d\n", EPD_BUSY, digitalRead(EPD_BUSY));

    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, EPD_CS);
    Serial.println("SPI.begin done");

    display.init(115200, true, 10, false);
    Serial.println("display.init done");

    Serial.printf("BUSY pin %d state after init: %d\n", EPD_BUSY, digitalRead(EPD_BUSY));

    Serial.println("drawing test frame...");
    drawTest("Hello!");
    Serial.println("draw done");
}

void loop() {
    delay(10000);
    Serial.println("still alive");
}
