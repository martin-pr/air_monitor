#include "app.h"

#include <Arduino.h>
#include <esp_sleep.h>

// Stub LED variant. No hardware wired yet — just deep-sleeps.
void app::setup() {
    Serial.begin(115200);
    Serial.println("led variant stub");
    esp_deep_sleep(60ULL * 1000000);  // 60 s
}
