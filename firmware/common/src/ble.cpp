#include "ble.h"

#include <Arduino.h>
#include <cstring>

#include <BLEDevice.h>
#include <BLEAdvertising.h>

namespace {

constexpr uint16_t BEACON_COMPANY_ID     = 0x4D41;  // 'AM' (Air Monitor), LE bytes: 0x41 0x4D
constexpr uint8_t  BEACON_VERSION        = 1;

// Byte offsets within the payload that follows the 2-byte company ID.
constexpr size_t   BOFF_VERSION          = 0;
constexpr size_t   BOFF_STATUS           = 1;
constexpr size_t   BOFF_CO2              = 2;
constexpr size_t   BOFF_TEMP             = 4;
constexpr size_t   BOFF_RH               = 6;
constexpr size_t   BOFF_BAT              = 7;
constexpr size_t   BEACON_PAYLOAD_LEN    = 8;

constexpr uint8_t  BAT_CHARGING_SENTINEL = 0xFF;

constexpr uint32_t ADV_DURATION_MS       = 5000;

}  // namespace

void Ble::advertise(const Sensor::Reading& reading,
                    const Battery::Status& battery,
                    esp_reset_reason_t     status) {
    int16_t tempCdeg = (int16_t)(reading.temperature * 100.0f);
    uint8_t mfr[2 + BEACON_PAYLOAD_LEN];
    mfr[0] = BEACON_COMPANY_ID & 0xFF;
    mfr[1] = BEACON_COMPANY_ID >> 8;
    mfr[2 + BOFF_VERSION]  = BEACON_VERSION;
    mfr[2 + BOFF_STATUS]   = (uint8_t)status;
    mfr[2 + BOFF_CO2]      = reading.co2 & 0xFF;
    mfr[2 + BOFF_CO2 + 1]  = reading.co2 >> 8;
    mfr[2 + BOFF_TEMP]     = (uint8_t)(tempCdeg & 0xFF);
    mfr[2 + BOFF_TEMP + 1] = (uint8_t)(tempCdeg >> 8);
    mfr[2 + BOFF_RH]       = (uint8_t)(reading.humidity);
    mfr[2 + BOFF_BAT]      = battery.charging ? BAT_CHARGING_SENTINEL : battery.pct;

    // Initialise the BLE stack once per power session. Re-initialising it every
    // cycle (init → deinit → init) panics the Bluedroid stack. The LED variant
    // hit this on its second advertise because it stays awake and advertises
    // repeatedly; the display variant never did, since it deep-sleeps (a full
    // reset) after each single advertise.
    static bool initialized = false;
    if (!initialized) {
        BLEDevice::init("Air Monitor");
        initialized = true;
    }
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    BLEAdvertisementData advData;
    // Build manufacturer-specific AD structure manually to handle binary data safely:
    // [length][0xFF = mfr type][company_id lo][company_id hi][payload...]
    uint8_t ad[2 + sizeof(mfr)];
    ad[0] = 1 + sizeof(mfr);
    ad[1] = 0xFF;
    memcpy(ad + 2, mfr, sizeof(mfr));
    advData.addData((char*)ad, sizeof(ad));
    adv->setAdvertisementData(advData);
    adv->setMinInterval(160);  // 100ms (units of 0.625ms)
    adv->setMaxInterval(160);
    adv->start();

    delay(ADV_DURATION_MS);

    adv->stop();
    // Deliberately not calling BLEDevice::deinit() — the stack stays up for the
    // next advertise. Deep-sleep (display variant) resets it anyway.
}
