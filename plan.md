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

### 1. ~~Display bug: empty after 2nd refresh~~ (fixed)

Fixed via `display.epd2.writeScreenBufferAgain()` after the initial clear plus the buffered `showStatus` rewrite. Plus `showStatus(rstMsg)` so the reset reason becomes line 1 in the buffer rather than being clobbered by the first status update.

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

Decision pending. Current `BleWidgetService.java` (merged from beacon_mode) implements option B — foreground service with `SCAN_MODE_LOW_LATENCY` and `MATCH_MODE_AGGRESSIVE`. Confirmed unreliable when the screen is locked, so this implementation must change regardless. Option A or C is the way forward.

## Measured progress

All numbers measured on hardware: battery-powered, USB disconnected, average current over a steady-state 1-minute cycle (after first boot completes). `beacon_mode` has been merged into `main`.

| Firmware state | Avg current | Battery life | Per-cycle energy |
|---|---|---|---|
| Cherry-picked beacon_mode baseline | 8.8 mA | 2.1 days | 528 mAs |
| SCD41 5s wait → `esp_light_sleep_start()` (`df427b1`) | 7.74 mA | 2.4 days | 464 mAs |
| Drop `WiFi.mode(WIFI_OFF)` + 200ms delay | 7.33 mA | 2.55 days | 440 mAs |

Per-cycle active energy is ~380 mAs over a ~14 s active phase (about 27 mA average during active). The 5 s BLE advertising window is the largest single chunk.

### Sleep current is *not* negligible — 1.29 mA measured

Direct measurement of the deep-sleep floor: **1.29 mA**, three orders of magnitude higher than the ~5 µA the ESP32-C3 datasheet suggests. Isolation testing:

| Configuration | Sleep current |
|---|---|
| Full board (XIAO + display + SCD41 + battery divider) | 1.29 mA |
| Display module disconnected | 232 µA |
| → Display module contribution | **~1.06 mA** |

The e-paper module is drawing ~1 mA continuously during sleep despite `display.hibernate()` and `SPI.end()`. The SSD1681 chip itself in deep sleep is ~5 µA, so this is module-level circuitry on the breakout PCB (boost/charge-pump glue, level shifters, or leakage paths through floating data pins on the SSD1681). The remaining 232 µA floor (no display) is XIAO + SCD41 breakout + ESP32-C3's real-world deep-sleep being higher than datasheet — secondary concern.

### Battery-life projections must include sleep current

With sleep dominant at longer cycles, the math changes dramatically. Per-cycle energy = ~380 mAs active + sleep current × sleep duration:

| Cycle | Sleep at 1.29 mA (today) | Sleep at 232 µA (display power-gated) |
|---|---|---|
| 1 min | 7.33 mA → 2.55 d (measured) | ~6.5 mA → 2.9 d |
| 5 min | 2.50 mA → 7.5 d | 1.49 mA → 12.6 d |
| 6 min | 2.29 mA → 8.2 d | 1.28 mA → 14.7 d ✓ |
| 10 min | 1.89 mA → 9.9 d | 0.86 mA → 21.8 d |
| 15 min | 1.69 mA → 11.1 d | 0.65 mA → 28.8 d |
| ∞ (sleep only) | **1.29 mA → 14.5 d (ceiling)** | 0.232 mA → 80+ d |

**At 1.29 mA sleep, no cycle length reaches 2 weeks** — sleep current alone consumes the budget. The display power-gating must be solved before any cycle-tuning matters.

### Notes on optimizations attempted/considered

- `display.hibernate()` between updates: kept (still correct), but power impact was much smaller than the SSD1681 datasheet suggested. Most of the visible refresh time is the e-ink material settling at low current, not the voltage pump.
- Shortening `ADV_DURATION_MS` (5 s → 2 s): not yet applied — would save ~90 mAs/cycle but couples to the (still-undecided) Android reception strategy. Safe under foreground scanning (option B), risky under PendingIntent (option A) or short-window autoConnect (option C).
- CPU frequency dance (drop to 40 MHz outside the BLE phase): not applied. BLE requires ≥80 MHz on ESP32-C3; dropping for the non-BLE phase would save ~50 mAs/cycle but adds complexity (APB clock divides change, affecting SPI/I2C timing).
- `esp_bt_sleep_enable()` before advertising: not tried yet — unknown gain, low risk.

### Display sleep-current fix options

1. ~~**Software — float all display pins (CS, DC, RST, SCK, MOSI) before deep sleep.**~~ Tried, no effect (1.29 → 1.30 mA). Leakage is not through SSD1681 input pins.
2. ~~**Software — switch SSD1681 from deep sleep mode 1 to mode 2** (RAM not retained, deeper sleep).~~ Tried via raw SPI `0x10 0x11` after `hibernate()`, no effect (1.30 mA). Module-level leakage is independent of SSD1681 sleep depth.
3. ~~**Software — hold `EPD_RST` low through deep sleep via `gpio_hold_en()`**~~ on the chance the breakout's boost EN piggybacks off RST. Tried, *worse* (1.36 mA): the boost is not gated by RST, and holding the GPIO output + putting the chip in hardware reset both cost a small amount.

All three software paths confirmed dead. The module's quiescent draw is from its own onboard circuitry (boost converter / glue), independent of any SSD1681 pin or state. **Hardware power-gating is the only remaining option.**

4. **Hardware — high-side P-channel MOSFET load switch on the display VCC.** Gate driven by a GPIO; drive high to cut power before sleep, low to enable on wake. ~3 components (MOSFET + pull-up + GPIO). Fully power-gates the module → 0 µA from display in sleep. The only viable fix for the display variant.

5. **Build a no-display variant.** If the display isn't a hard requirement, omitting it sidesteps the problem entirely — no board revision needed. See below.

### Alternative: no-display variant meets the goal as-is

Removing the e-paper module entirely (or fitting a hardware variant without it) gives a 232 µA sleep floor with no further engineering. The active phase also drops by ~30–50 mAs/cycle (no display init / full-window refresh / hibernate).

Estimated active phase: ~340–360 mAs/cycle (vs ~380 mAs with display).

| Cycle | Avg current | Battery life |
|---|---|---|
| 1 min | ~6.0 mA | ~3 days |
| 5 min | ~1.40 mA | ~13.4 days |
| **6 min** | **~1.20 mA** | **~15.6 days ✓** |
| 10 min | ~0.81 mA | ~23 days |
| 15 min | ~0.60 mA | ~31 days |

A 6-minute cycle on the no-display variant hits 2 weeks with margin; 10-minute gives 3 weeks. Since there's no visual refresh, longer cycles are free — the phone gets readings from the beacon regardless of cadence.

The 232 µA floor still has ~225 µA of unexplained draw (vs ESP32-C3 datasheet ~5 µA deep sleep). Likely from the SCD41 breakout pull-ups and the C3's real-world deep sleep being higher than spec. Could be chased further but isn't needed for the 2-week target.

## Next steps

Two parallel paths depending on whether the display is a hard requirement:

**Display-required path:**
1. Design the MOSFET load switch on the next PCB revision (display VCC power-gated by a GPIO).
2. Once display sleep current is fixed, tune `SLEEP_DURATION_US` to the cycle length that meets the budget.

**No-display variant:**
1. Measure actual active-phase energy with the display removed to confirm the ~340-360 mAs estimate.
2. Set `SLEEP_DURATION_US` to 6 minutes (or longer for more margin).

**Both paths:**
3. Pick A / B / C for Android reception.
4. Implement chosen Android approach.
