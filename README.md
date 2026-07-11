# air_monitor
ESP32-based air monitor (CO2, temperature, humidity) with BLE notifications.
Ships in two firmware variants that share the sensor, BLE beacon protocol,
and Android widget:

- **display** — v1 hardware: e-paper dashboard, per-cycle full readout, charging inferred from battery voltage.
- **led** — v2 hardware: RGB status LED, momentary button, explicit USB charging detection. No display.

## Hardware

Shared across both variants:

- Seeed Studio XIAO ESP32-C3
- Sensirion SCD41 (CO2 / temperature / humidity, I2C)
- LiPo cell (400 mAh reference) with a 220 k / 220 k battery-voltage divider into `D0`

Display variant additionally uses:

- WeAct 200×200 ePaper module (SPI, SSD1681)

LED variant additionally uses:

- 5050 RGB LED (common-cathode assumed), three GPIO PWM channels through per-channel current-limiting resistors — 220 Ω red, 47 Ω green, 47 Ω blue
- Momentary push-button between a GPIO and GND, wired as a deep-sleep wake source
- VBUS-to-GPIO voltage divider for explicit "USB plugged in" detection (see wiring below)

## Prerequisites (one-time)

Install arduino-cli (official 64-bit binary, not the snap):

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

Install the ESP32 board package and required libraries:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Sensirion I2C SCD4x"
# display variant only:
arduino-cli lib install GxEPD2
arduino-cli lib install "Adafruit GFX Library"
```

Grant serial port access:

```bash
sudo usermod -aG dialout $USER
# then log out and back in, or use: sg dialout -c "<command>"
```

## Build & Flash

```bash
cd firmware
cmake -B build
```

Build one variant, or both:

```bash
cmake --build build --target build_display
cmake --build build --target build_led
cmake --build build             # both variants
```

Flash to the device:

```bash
cmake --build build --target flash_display
cmake --build build --target flash_led
```

To override the upload port or target a different board:

```bash
cmake -B build -DPORT=/dev/ttyACM1
cmake -B build -DFQBN=esp32:esp32:XIAO_ESP32S3
```

The default FQBN is `esp32:esp32:XIAO_ESP32C3`.

### Source layout

```
firmware/
├── common/          Arduino library shared by both variants
│   └── src/         battery, ble, reset_reason, sensor
├── display/         e-paper sketch (display.ino, app.cpp, display.cpp)
└── led/             LED sketch (led.ino, app.cpp, plus led/button/charging modules)
```

## Serial Monitor

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Press Enter once after connecting — the USB-Serial-JTAG peripheral requires the host to send a byte before output appears. This is default arduino-esp32 3.x behavior.

## Android Widget

The `app` directory contains an Android home-screen widget. It passively scans for BLE advertisements from the device and displays the latest sensor readings — no connection or pairing required.

The build uses Gradle with the Android Gradle Plugin. The wrapper is checked in, so no system Gradle install is needed — just JDK 17 and the Android SDK (build-tools 35, platform android-35). Point `local.properties` at the SDK on first checkout:

```bash
cd app
echo "sdk.dir=$HOME/Android/Sdk" > local.properties
```

Build the debug APK:

```bash
./gradlew :app:assembleDebug
# → app/build/outputs/apk/debug/app-debug.apk
```

Build the release AAB (for Google Play upload):

```bash
./gradlew :app:bundleRelease
# → app/build/outputs/bundle/release/app-release.aab
```

Release signing is driven by `keystore.properties` at the repo root (git-ignored). When it's present and points at an upload keystore, release builds are signed with it; when it's absent, they fall back to the debug keystore — fine for local test builds but rejected by Play.

Install a debug build on a connected Android device:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

After installing, add the **Air Monitor** widget from the Android launcher widget picker.

## BLE

The device uses a **connectionless beacon** — no GATT, no pairing. It wakes from deep sleep every 5 minutes, takes a measurement, updates the display (display variant) or flashes the status LED (LED variant), then advertises for 5 seconds before going back to sleep.

The advertisement contains manufacturer-specific data with company ID `0x4D41` (`AM`, little-endian bytes `0x41 0x4D`), followed by a versioned binary payload:

| Byte offset (after company ID) | Field | Type | Notes |
|---|---|---|---|
| 0 | Protocol version | uint8 | Currently `1`; check before parsing further fields |
| 1 | Status | uint8 | Last `esp_reset_reason_t`. `8` = normal wake from deep sleep; any other value means the chip cold-booted (see table below) |
| 2–3 | CO2 | uint16 LE | ppm |
| 4–5 | Temperature | int16 LE | 0.01 °C (e.g. 2350 = 23.50 °C) |
| 6 | Relative humidity | uint8 | % RH (integer) |
| 7 | Battery | uint8 | `0–100` = battery %; `0xFF` = charging. Detected via Vbat > 4.10 V threshold (display variant) or an explicit VBUS-divider ADC (LED variant); the % is unreliable during a charge cycle so we send a sentinel instead |

**Parsing rule:** read byte 0 first; only parse further fields if the version is known. The protocol is still under development — the version byte is bumped on any layout change rather than maintaining backward compatibility.

#### Status (reset reason) values

The raw `esp_reset_reason_t` enum is exposed as-is. The interesting values for this firmware:

| Value | Name | Meaning |
|---|---|---|
| 0 | `ESP_RST_UNKNOWN` | Reason could not be determined |
| 1 | `ESP_RST_POWERON` | Cold boot — USB plugged in, battery connected, or RESET button |
| 3 | `ESP_RST_SW` | Software-triggered `esp_restart()` (not used by this firmware) |
| 4 | `ESP_RST_PANIC` | Firmware crash (exception / abort) |
| 5 | `ESP_RST_INT_WDT` | Interrupt watchdog fired |
| 6 | `ESP_RST_TASK_WDT` | FreeRTOS task watchdog fired |
| 7 | `ESP_RST_WDT` | RTC watchdog (other) |
| 8 | `ESP_RST_DEEPSLEEP` | **Normal** — woke from `esp_deep_sleep()` |
| 9 | `ESP_RST_BROWNOUT` | Vbat sagged below the brownout threshold |
| 11 | `ESP_RST_USB` | USB peripheral triggered reset |
| 12 | `ESP_RST_JTAG` | JTAG triggered reset |

Values not in this table can still appear (`ESP_RST_EFUSE = 13`, `ESP_RST_PWR_GLITCH = 14`, `ESP_RST_CPU_LOCKUP = 15`, `ESP_RST_SDIO = 10`, `ESP_RST_EXT = 2`) — they're unlikely on this hardware/firmware but should be passed through unchanged.

The device advertises as **Air Monitor** (MAC `ac:27:6e:7e:c7:f4`). Scanners can filter by local name or company ID.

## Wiring

### SCD41 (I2C) — both variants

| SCD41 pin | XIAO pin | GPIO |
|---|---|---|
| VDD | 3V3 | — |
| GND | GND | — |
| SDA | D4 | GPIO6 |
| SCL | D5 | GPIO7 |

A decoupling capacitor between VDD and GND is required for stable CO2 readings. The NDIR laser draws a brief high current spike; without sufficient capacitance the CO2 measurement fails while RH/temp continue to work. The tested working combination is a 10 µF ceramic and a 100 µF electrolytic in parallel.

### Battery (J1 + voltage divider) — both variants

A JST connector (J1) brings in the LiPo battery. Two 220 kΩ resistors (R1, R2) form a 1:2 voltage divider so the full battery voltage range fits within the ESP32-C3 ADC input range:

```
Battery+ ──[R2 220kΩ]──┬── D0 (GPIO2, ADC)
                        │
                    [R1 220kΩ]
                        │
                       GND
```

Read with ADC 11 dB attenuation (0–3.9 V input range). The quiescent current through the divider is 4.2 V / 440 kΩ ≈ 10 µA — negligible.

Voltage is converted to a battery percentage using a 4-point lookup table with linear interpolation between segments:

| % | Battery V | V at D0 |
|---|---|---|
| 100 | 4.20 | 2.10 |
| 80 | 3.98 | 1.99 |
| 20 | 3.52 | 1.76 |
| 0 | 3.00 | 1.50 |

The middle segment (20–80%) approximates the flat part of the LiPo discharge curve; the steeper drops at each end get their own segments.

### ePaper Display (SPI) — display variant only

The WeAct 200×200 module connects over SPI. Despite the silkscreen labels (SCL/SDA), these are SPI signals — not I2C.

| WeAct pin | XIAO pin | GPIO |
|---|---|---|
| VCC | 3V3 | — |
| GND | GND | — |
| SDA | D10 | GPIO10 (MOSI) |
| SCL | D8 | GPIO8 (SCK) |
| CS | D3 | GPIO5 |
| D/C | D2 | GPIO4 |
| RES | D1 | GPIO3 |
| BUSY | D6 | GPIO21 |

Pins D7 and D9 remain free after this wiring.

The display is driven by [GxEPD2](https://github.com/ZinggJM/GxEPD2) using the `GxEPD2_154_D67` driver class.

### RGB LED (PWM) — LED variant only

A common-cathode 5050 RGB LED, one GPIO per channel, driven by ESP32-C3 LEDC PWM. Per-channel current-limiting resistors size the drive current so R / G / B contribute roughly equally at equal duty:

| Channel | Series R | Current at full duty | Notes |
|---|---|---|---|
| Red   | 220 Ω | ~5.9 mA | V_f ≈ 2.0 V, plenty of headroom |
| Green | 47 Ω  | ~6.4 mA | V_f ≈ 3.0 V, current sensitive to part-to-part V_f drift |
| Blue  | 47 Ω  | ~6.4 mA | Same as green |

```
D2 ──[220 Ω]── R anode ──┐
D3 ──[47 Ω]── G anode ──┼── common cathode ── GND
D6 ──[47 Ω]── B anode ──┘
```

Firmware never drives at 100 % duty — LED is used for brief low-brightness status flashes. Average current is deep-sleep-noise level, so no need to worry about brownout on turn-on transients.

### Button — LED variant only

Momentary tactile switch from a GPIO to GND, with the internal pull-up enabled on that pin. Wired as an RTC-capable pin so a press wakes the chip from deep sleep.

```
D7 ──┬── button ── GND
      │
      └── (internal pull-up)
```

### Charging-detect divider — LED variant only

Rather than inferring charging from Vbat > 4.10 V (the display variant's approach), the LED board has a dedicated divider off USB VBUS into a second ADC pin. VBUS is 5 V when USB is plugged in, 0 V otherwise; a 220 k / 220 k divider brings that to ~2.5 V, easily distinguished from 0 V by an ADC read.

```
VBUS ──[220 kΩ]──┬── D1 (GPIO3, ADC)
                  │
              [220 kΩ]
                  │
                 GND
```

Same resistor values as the battery divider — one BOM entry, shared design.

## Power

### Architecture

Both variants use a **deep sleep beacon** cycle:

1. Wake from deep sleep (timer, or button press on LED variant)
2. Take SCD41 single-shot measurement (~5 s)
3. Update display (display variant) or flash status LED (LED variant)
4. Advertise BLE beacon for 5 s
5. Deep sleep for ~5 minutes

Total active time: ~11 s per 300 s cycle.

### Component current

| Component | Mode | Current |
|---|---|---|
| ESP32-C3 | Deep sleep | ~5 µA |
| ESP32-C3 | Active, 80 MHz, no radio | ~20 mA |
| ESP32-C3 | BLE advertising burst | ~30 mA peak |
| SCD41 | Single-shot (5 min interval average) | ~0.5 mA |
| SCD41 | Power-down | <0.001 mA |
| ePaper (display variant) | Update (active) | ~20 mA |
| ePaper (display variant) | Hibernate | <0.01 mA |
| RGB LED (LED variant) | ~10 % duty, brief flash | ~2 µA averaged over the 5 min cycle |

### Battery estimate (400 mAh cell)

Active ~11 s per 300 s cycle, deep sleep otherwise. Averaged over a full cycle:

- ESP32-C3 active: ~25 mA × (11/300) ≈ 0.9 mA
- SCD41 single-shot: ~0.5 mA average (per datasheet at 5 min interval)
- Deep sleep (everything): ~5 µA × (289/300) ≈ negligible
- Display refresh (display variant only): dominated by ESP32-C3 active time, not the ePaper itself
- LED signalling (LED variant only): negligible (~2 µA averaged)

**Total ≈ 1.4 mA average → ~12 days on 400 mAh** for either variant (excluding deep sleep current anomalies under investigation). The LED variant should be marginally better since ePaper refresh time shrinks to zero, but the ESP32-C3 active window still dominates the budget.

### SCD41 single-shot and ASC

The firmware uses `measure_single_shot` (not periodic mode). The sensor stays idle until triggered, takes ~5 s for a reading, then enters power-down. The ESP32 controls the interval.

Automatic Self-Calibration (ASC) works with single-shot via the `power_down` / `wake_up` command pair, which preserves calibration state in SRAM. Hard VDD power-cycling breaks ASC because SRAM is lost. The datasheet optimises ASC for a 5-minute interval and requires outdoor-level CO2 (~400 ppm) exposure at least once per week.
