# Tokmagotchi

A physical developer companion that turns your coding activity into a tamagotchi-style pet. The pet lives on a SenseCAP Watcher device and reacts to token usage, Claude Code permission requests, voice check-ins, and photos.

## Repository layout

```
tokmagotchi/
├── firmware/          ESP-IDF v5.1 firmware for the SenseCAP Watcher (ESP32-S3)
├── desktop/           (future) Electron app for log parsing + device bridge
├── hook/              (future) tokmagotchi-hook binary for Claude Code PreToolUse
├── assets/            (future) pixel-art sprites
└── docs/
    └── SPEC.md        full project specification
```

## Firmware — quick start

Phase 1 scope is the device firmware; see `firmware/` for source.

```bash
# Install ESP-IDF v5.1 per https://docs.espressif.com/projects/esp-idf/en/v5.1/get-started/
cd firmware
. $IDF_PATH/export.sh
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

On first boot the device advertises BLE as `tokmagotchi-<mac>`. Pair from the desktop app to send WiFi credentials + pet name. After that the device connects over WiFi and advertises `_tokmagotchi._tcp` on port 8080.

### Device HTTP API

| Method | Path          | Purpose                             |
|--------|---------------|-------------------------------------|
| POST   | /feed         | Token / voice / vision feed events  |
| POST   | /permission   | Surface a Claude Code permission    |
| POST   | /name         | Rename the pet                      |
| GET    | /state        | Pet mood + emoji + meter snapshot   |
| GET    | /media/list   | List audio/photo files on SD        |
| GET    | /media/<file> | Download a specific media file      |

### Board-specific TODOs

Real hardware integration requires filling in values that are not documented in the public datasheet; all such sites are marked `TODO` or `placeholder` in the source:

- `firmware/main/input.cpp` — encoder + button + touch GPIOs
- `firmware/main/audio_capture.cpp` — I2S mic pins (BCK/WS/DIN)
- `firmware/main/camera_capture.cpp` — Himax bridge protocol (JPEG RPC)
- `firmware/main/display.cpp` — LVGL panel driver / board support package

See [docs/SPEC.md](docs/SPEC.md) for the full design document.
