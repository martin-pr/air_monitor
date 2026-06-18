# Power Reduction Plan

## Target

- Battery: 450 mAh
- Required runtime: 2 weeks
- Average current budget: **1.34 mA**

## Why the current architecture cannot meet the target

The device currently averages ~50 mA with BLE always advertising. Even with every available always-on optimization, the math does not work:

| Architecture | Avg current | Battery life |
|---|---|---|
| Current (always-on BLE, no sleep) | 50 mA | ~9 hours |
| BLE modem sleep, fully tuned | 5-8 mA | 2-3 days |
| Deep sleep, 30s wake interval | ~4 mA | ~4 days |
| Deep sleep, 2 min wake interval | ~2 mA | ~9 days |
| Deep sleep, 5 min wake interval | ~1 mA | ~18 days |

The BLE controller on ESP32-C3 holds a `NO_LIGHT_SLEEP` pm lock while active, so any architecture that keeps BLE up continuously is bounded by the radio's idle current. Deep sleep between measurements is the only path that meets the budget.

Confirmed empirically: with `BLE_ENABLED = false` the device draws 22 mA (light sleep not engaging due to USB-JTAG pm lock); with BLE on, 50 mA. Neither is close to 1.34 mA.

## Chosen architecture: deep sleep + BLE beacon

1. Wake from deep sleep
2. Take SCD41 single-shot measurement (~5s)
3. Update e-paper display (~1.5s)
4. Emit advertising packets with reading encoded in manufacturer data (~5s)
5. Deep sleep for the remainder of the cycle (~5 µA)

Reading carried in the advertisement payload — no GATT connection needed. Phone scans and parses.

## Existing work: `beacon_mode` branch

This architecture is mostly implemented on the `beacon_mode` branch:

- `esp_deep_sleep()` with 1-minute cycles
- SCD41 single-shot mode with proper wake/measure/powerDown sequencing
- Manufacturer-data beacon: company ID `0x4D41` ('AM'), 7-byte versioned payload (version, CO2, temp×100, RH, battery%)
- 5-second advertising window at 100 ms interval
- `Wire.end()`, `SPI.end()`, GPIO10 floated before sleep
- Android app rewritten to scan-and-parse instead of connect-and-subscribe

## Open issues to resolve

### 1. Display bug: empty after 2nd refresh

Already fixed on `main` via `display.epd2.writeScreenBufferAgain()` after the initial clear, plus the buffered `showStatus` rewrite. Merge the relevant changes from `main` into `beacon_mode`.

### 2. Android background reception unreliable

The core blocker. A normal `startScan()` callback does not fire when the screen is off because of Android's Doze and background scan throttling. Three paths forward, in order of preference for a widget use case:

**Option A — PendingIntent-based scanning (API 26+).** Use the `BluetoothLeScanner.startScan(filters, settings, callbackIntent)` overload. The system holds the filter, scans on its own schedule, and wakes the app via the intent on a match — including from Doze.
- Pros: no foreground service, no persistent notification, designed for exactly this case.
- Cons: requires `ACCESS_BACKGROUND_LOCATION` on Android 10+. Some OEM power managers (Xiaomi, Huawei) kill it.
- Phone battery impact: **~0.5-1% per day**. The system batches scans and the app only wakes on match (once per sensor cycle); each wake is ~50-100 ms of CPU plus a brief intent dispatch.

**Option B — Foreground service with persistent notification.** A foreground service is exempt from Doze scanning restrictions; `startScan` callback runs continuously.
- Pros: most reliable across all OEMs.
- Cons: permanent status-bar notification.
- Phone battery impact: **~5-15% per day**. Continuous BLE scanning is expensive (BT subsystem stays in the 3-15 mA range depending on scan mode), and the service prevents the app process from being killed, so any callbacks/work executed per scan result accumulate.

**Option C — Connection-oriented with `autoConnect=true`.** Device wakes, advertises, accepts a short GATT connection, sends the reading, disconnects, sleeps. Phone uses `connectGatt(autoConnect=true)` which makes Android handle the wait-and-connect at the system level.
- Pros: very reliable on Android.
- Cons: device awake longer (~10-15s instead of ~5s), modestly higher average current.
- Phone battery impact: **~1-3% per day**. The system handles the pending-connection efficiently; brief radio activity (~1-2s per sensor cycle) when the device shows up.

Numbers are rough — actual impact varies with phone hardware, Android version, and other background scanners. For reference, a typical phone uses 0.5-1% per hour in standby, so option A or C adds noise-level overhead while option B is genuinely noticeable.

Decision pending. Check current `BleWidgetService.java` implementation first to see which API it uses.

## Next steps

1. Inspect `app/.../BleWidgetService.java` on `beacon_mode` to identify current Android API usage.
2. Pick A / B / C for Android reception.
3. Merge display fixes from `main` into `beacon_mode`.
4. Measure actual deep-sleep current draw on hardware to validate the architecture math.
5. Tune wake interval based on measured per-cycle energy and the 1.34 mA budget.
