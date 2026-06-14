# air_monitor
ESP32-based air monitor (CO2, temperature, humidity) with BLE notifications.

## Hardware

- Seeed Studio XIAO ESP32-C3
- Sensirion SCD41 (CO2 / temperature / humidity, I2C)
- WeAct 200×200 ePaper module (SPI)

## Prerequisites (one-time)

Install arduino-cli (official 64-bit binary, not the snap):

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

Install the ESP32 board package and required libraries:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson
arduino-cli lib install "Sensirion I2C SCD4x"
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
cmake --build build
cmake --build build --target flash
```

To override the upload port or target a different board:

```bash
cmake -B build -DPORT=/dev/ttyACM1
cmake -B build -DFQBN=esp32:esp32:XIAO_ESP32S3
```

The default FQBN is `esp32:esp32:XIAO_ESP32C3`. The XIAO ESP32-C3 has no user-controllable LED; the firmware contains no LED code.

## Serial Monitor

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Press Enter once after connecting — the USB-Serial-JTAG peripheral requires the host to send a byte before output appears. This is default arduino-esp32 3.x behavior.

## Android Widget

The `app` directory contains an Android home-screen widget built with CMake and Android SDK command-line tools. It connects to the device over BLE and displays live sensor readings.

Build the debug APK:

```bash
cd app
cmake -S . -B build
cmake --build build
```

Install it on a connected Android device:

```bash
adb install -r build/AirMonitorWidget-debug.apk
```

After installing, add the **Air Monitor** widget from the Android launcher widget picker. If an older instance is already on the home screen, remove and re-add it so the launcher reloads the widget metadata.

The app runs a foreground service (`connectedDevice` type) that scans for the device and maintains a GATT connection. The scan restarts every 3 minutes if not connected, which prevents it getting stuck after external tools (e.g. nRF Connect) temporarily take the connection.

## BLE

The device advertises as **Air Monitor** and exposes a single GATT service:

| | UUID |
|---|---|
| Service | `f59c6ce6-b894-4e87-9c5b-b347b72c7e93` |
| Characteristic | `3d455d99-f31a-4826-bf25-7c5f23cedc49` |

The characteristic uses NOTIFY. The device pushes a JSON payload every 30 seconds:

```json
{"co2": 1234, "temp": "23.5", "rh": "45.1", "bat": 85}
```

Connection parameters are negotiated to a 500ms–1s interval to reduce radio duty cycle. The first notification is sent ~5 seconds after the client subscribes (to allow time for the CCCD write at the negotiated interval).

## Wiring

### SCD41 (I2C)

| SCD41 pin | XIAO pin | GPIO |
|---|---|---|
| VDD | 3V3 | — |
| GND | GND | — |
| SDA | D4 | GPIO6 |
| SCL | D5 | GPIO7 |

A decoupling capacitor between VDD and GND is required for stable CO2 readings. The NDIR laser draws a brief high current spike; without sufficient capacitance the CO2 measurement fails while RH/temp continue to work. The tested working combination is a 10 µF ceramic and a 100 µF electrolytic in parallel.

### ePaper Display (SPI)

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

### Battery (J1 + voltage divider)

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

## Power

### Components

| Component | Mode | Current |
|---|---|---|
| ESP32-C3 | BLE connected, light sleep | ~0.6–1.2 mA |
| ESP32-C3 | BLE advertising, light sleep | ~2–2.5 mA |
| SCD41 | Periodic (5s, continuous) | 15–18 mA @ 3.3 V (datasheet) |
| SCD41 | Low-power periodic (30s, autonomous) | 3.2–3.5 mA @ 3.3 V (datasheet) |
| SCD41 | Single-shot (5 min interval average) | 0.45–0.5 mA @ 3.3 V (datasheet) |

### Battery estimates (400 mAh cell)

| Scenario | Total current | Estimated life |
|---|---|---|
| Connected + SCD41 low-power periodic (30s) | ~4–5 mA | ~3–4 days |
| Connected, no sensor | ~0.6–1.2 mA | ~14–28 days |
| Advertising only, no sensor | ~2–2.5 mA | ~7–8 days |

### SCD41 measurement modes

**Low-power periodic** (`start_low_power_periodic_measurement`): the sensor manages its own 30s duty cycle autonomously — active for ~5s, idle for ~25s, repeating. The ESP32 just reads the result over I2C before each BLE notification. Simple to integrate; 3.2–3.5 mA average per datasheet.

**Single-shot** (`measure_single_shot`): sensor stays idle until the ESP32 triggers a measurement, waits ~5s for the result, then returns to idle. The ESP32 controls the interval. At a 5-minute interval the datasheet-measured average is 0.45–0.5 mA. Trade-off: BLE clients receive cached readings between measurements rather than fresh ones.

Automatic Self-Calibration (ASC) works with single-shot via the software `power_down` / `wake_up` commands, which preserve calibration state in the sensor's SRAM. Hard VDD power-cycling breaks ASC because SRAM is lost. The datasheet optimises ASC for a 5-minute measurement interval and requires the sensor to see outdoor-level CO2 (~400 ppm) at least once per week; the first reading after any power cycle should be discarded.

### Extending battery life

The current firmware maintains a persistent BLE connection, which prevents the ESP32 radio from sleeping deeply. Two architectural approaches can significantly improve battery life:

**Deep sleep between measurements**

ESP32 deep sleeps at ~20 µA between cycles. On each wake:
1. Trigger SCD41 single-shot measurement (~5s)
2. Advertise, accept one connection, send notification, disconnect (~3–5s)
3. Return to deep sleep for ~5 minutes

Active ~10s out of every 300s: ESP32 at ~20 mA × (10/300) ≈ 0.7 mA average + SCD41 single-shot 0.5 mA average = **~1.2 mA total → ~14 days on 400 mAh**.

Trade-off: no persistent connection. The Android client must scan and connect on demand rather than maintaining a subscription. A short advertising window on each wake (e.g. 10s) gives the app time to connect.

**BLE advertisement beacon (no connection)**

Encode sensor readings directly in the BLE advertisement packet (manufacturer-specific data). The client passively scans — no connection, no pairing, no GATT overhead.

- Advertising burst: ~50ms every 5 minutes, then deep sleep
- Average current: ~0.1 mA → **months on 400 mAh**

Trade-off: payload limited to ~20 bytes (sufficient for CO2 + temp + RH as integers); iOS has restrictions on reading manufacturer data from unconnected peripherals; Android app must use a BLE scanner rather than a GATT client.

**Comparison**

| Approach | Avg current | 400 mAh |
|---|---|---|
| Current (always-connected + SCD41 low-power periodic) | ~4–5 mA | ~3–4 days |
| Deep sleep + connect-on-wake (5 min interval) | ~1.2 mA | ~14 days |
| BLE advertisement beacon (5 min interval) | ~0.1 mA | months |
