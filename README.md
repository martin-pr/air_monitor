# air_monitor
ESP32-based air monitor (CO2, temperature, humidity) with BLE notifications.

## Hardware

- Seeed Studio XIAO ESP32-S3 Sense
- Sensirion SCD41 (CO2 / temperature / humidity, I2C)

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

To override the upload port:

```bash
cmake -B build -DPORT=/dev/ttyACM1
```

## Serial Monitor

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Press Enter once after connecting — the USB-Serial-JTAG peripheral (HWCDC) requires the host to send a byte before output appears. This is default arduino-esp32 3.x behavior.

## Android Widget

The `app` directory contains a minimal Android home-screen widget built directly with CMake and Android SDK command-line tools. It currently displays a single text field: `Hello world!`.

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

After installing, add the **Air Monitor** widget from the Android launcher widget picker. If an older instance is already on the home screen, remove and add it again so the launcher reloads the widget metadata.

## BLE

The device advertises as **Air Monitor** and exposes a single GATT service:

| | UUID |
|---|---|
| Service | `f59c6ce6-b894-4e87-9c5b-b347b72c7e93` |
| Characteristic | `3d455d99-f31a-4826-bf25-7c5f23cedc49` |

The characteristic uses NOTIFY. The device pushes a JSON payload every 30 seconds:

```json
{"co2": 1234, "temp": 23.5, "rh": 45.1}
```

Connection parameters are negotiated to a 500ms–1s interval to reduce radio duty cycle. The first notification is sent ~5 seconds after the client subscribes (to allow time for CCCD write at the negotiated interval).

## Power

### Components

| Component | Mode | Current |
|---|---|---|
| ESP32-S3 | BLE connected, light sleep | ~0.6–1.2 mA |
| ESP32-S3 | BLE advertising, light sleep | ~2–2.5 mA |
| SCD41 | Low-power periodic (30s, autonomous) | ~3.2 mA |
| SCD41 | Single-shot on demand (ESP32-triggered) | ~0.5 mA idle + ~15 mA for ~5s |
| LED | Per blink (50ms on, once per 30s notification) | negligible |

### Battery estimates (400 mAh cell)

| Scenario | Total current | Estimated life |
|---|---|---|
| Connected + SCD41 low-power periodic (30s) | ~4–4.5 mA | ~4 days |
| Connected + SCD41 single-shot every 5 min | ~1.5–2 mA | ~8–11 days |
| Connected, no sensor | ~0.6–1.2 mA | ~14–28 days |
| Advertising only, no sensor | ~2–2.5 mA | ~7–8 days |

### SCD41 measurement modes

**Low-power periodic** (`start_low_power_periodic_measurement`): the sensor manages its own 30s duty cycle autonomously — active for ~5s, idle for ~25s, repeating. The ESP32 just reads the result over I2C before each BLE notification. Simple to integrate; ~3.2 mA average.

**Single-shot** (`measure_single_shot`): sensor stays idle until the ESP32 triggers a measurement, waits ~5s for the result, then returns to idle. The ESP32 controls the interval. More complex but significantly lower average draw at long intervals (e.g. every 5 minutes). Trade-off: BLE clients receive cached readings between measurements rather than fresh ones.

### Extending battery life

The current firmware maintains a persistent BLE connection, which prevents the ESP32 radio from sleeping deeply. Two architectural approaches can significantly improve battery life:

**Deep sleep between measurements**

ESP32 deep sleeps at ~20 µA between cycles. On each wake:
1. Trigger SCD41 single-shot measurement (~5s)
2. Advertise, accept one connection, send notification, disconnect (~3–5s)
3. Return to deep sleep for ~5 minutes

Active ~10s out of every 300s → ~1 mA average total → **~17 days on 400 mAh**.

Trade-off: no persistent connection. The Android client must scan and connect on demand rather than maintaining a subscription. A short advertising window on each wake (e.g. 10s) gives the app time to connect.

**BLE advertisement beacon (no connection)**

Encode sensor readings directly in the BLE advertisement packet (manufacturer-specific data). The client passively scans — no connection, no pairing, no GATT overhead.

- Advertising burst: ~50ms every 5 minutes, then deep sleep
- Average current: ~0.1 mA → **months on 400 mAh**

Trade-off: payload limited to ~20 bytes (sufficient for CO2 + temp + RH as integers); iOS has restrictions on reading manufacturer data from unconnected peripherals; Android app must use a BLE scanner rather than a GATT client.

**Comparison**

| Approach | Avg current | 400 mAh |
|---|---|---|
| Current (always-connected + SCD41 periodic) | ~4.5 mA | ~4 days |
| Deep sleep + connect-on-wake (5 min interval) | ~1 mA | ~17 days |
| BLE advertisement beacon (5 min interval) | ~0.1 mA | months |
