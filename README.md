# air_monitor
ESP32-based air monitor

## Hardware

Seeed Studio XIAO ESP32-S3 Sense

## Prerequisites (one-time)

Install arduino-cli (official 64-bit binary, not the snap):

```bash
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

Install the ESP32 board package:

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
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

## BLE

The device advertises as **Air Monitor** and exposes a single GATT service:

| | UUID |
|---|---|
| Service | `f59c6ce6-b894-4e87-9c5b-b347b72c7e93` |
| Characteristic | `3d455d99-f31a-4826-bf25-7c5f23cedc49` |

The characteristic is readable (up to 512 bytes). Currently returns `hello world`; intended to carry a JSON sensor payload (e.g. `{"co2":1234,"temp":23.5}`).
