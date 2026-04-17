# Tokmagotchi — Project Specification

> A physical developer companion that turns your coding activity into a tamagotchi-style pet. The pet lives on a SenseCAP Watcher device and reacts to your token usage, Claude Code permission requests, voice check-ins, and photos of your work.

## Overview

Tokmagotchi has two components that work together but can operate independently:

- **Desktop App (Electron)** — collects coding activity data, bridges the device to the internet, provides a pairing UI and activity dashboard.
- **Watcher Device (ESP32-S3 custom firmware)** — the physical pet, capable of full standalone operation without any connection.

The pet communicates exclusively through emoji overlaid on a pixel-art sprite.

## Phase 1 Scope

In scope:

- Claude Code token usage as primary feed (local log parsing).
- Claude Code permission hook — pet surfaces permission requests, user approves/denies on device.
- Voice capture — mic records on gesture, saves to SD.
- Photo capture — camera captures on gesture, saves to SD.
- Pixel-art sprite with emoji overlay.
- Electron desktop app with activity dashboard and pairing flow.
- Device works fully offline and standalone.

Out of scope (future phases):

- GitHub integration, person/presence detection, audio/vision processing, pet evolution, user-uploaded sprites, mobile companion app.

## Firmware constants

| Name                          | Value   |
|-------------------------------|---------|
| `DEVICE_HTTP_PORT`            | 8080    |
| `PERMISSION_TIMEOUT_MS`       | 30 000  |
| `MDNS_SERVICE`                | `_tokmagotchi._tcp` |
| `DECAY_TOKENS` (per hour)     | 0.10    |
| `DECAY_VOICE` (per hour)      | 0.05    |
| `DECAY_VISION` (per hour)     | 0.03    |
| `TOKEN_FEED_LARGE_THRESHOLD`  | 1000    |
| `SLEEP_HOUR` / `WAKE_HOUR`    | 22 / 7  |

## Pet state machine

Priority order (highest first):

```
SLEEPING  ALERT  REACTING  EXCITED  HAPPY  CONTENT  IDLE  HUNGRY  VERY_HUNGRY
```

Transitions:

- Large token feed (>1000 tokens) → `EXCITED` (🤩), then back to `CONTENT`.
- Small token feed → `HAPPY` (😄), then back.
- Permission request → `ALERT` (🚨), freeze until user response on device.
- Permission resolved → `REACTING` (✅ / 🚫), then back.
- Voice gesture → `REACTING` (🎧). Photo gesture → `REACTING` (👀).
- Time passes → meters decay, mood downgrades.
- 22:00 local → `SLEEPING` (💤). 07:00 → wake with ☀️.

## Device HTTP API

See source of `firmware/main/http_server.cpp` for the canonical shapes.

```
POST /feed         { "type": "token"|"voice"|"vision", "size": "small"|"large", "tokens": 123 }
POST /permission   { "id": "...", "tool": "Write", "path": "...", "command": "..." }
                   → blocks until user input, then { "id", "decision": "allow"|"deny", "decidedBy" }
POST /name         { "name": "Pixel" }
GET  /state        { "state", "emoji", "name", "meters": { tokens, voice, vision }, "lastFeed": {...} }
GET  /media/list   { "files": [{ "name", "size", "type" }] }
GET  /media/<name> raw file bytes
```

## Pairing flow

1. First-boot device advertises BLE as `tokmagotchi-<mac>` using the ESP-IDF provisioning manager.
2. Desktop app scans, lets the user pick a pet name, sends WiFi SSID + password + pet name over BLE.
3. Device connects to WiFi, persists pet name, advertises `_tokmagotchi._tcp` on port 8080 with hostname `tokmagotchi-<slug>`.
4. Desktop discovers via mDNS, pairing complete.

## Persistence

Persisted to SPIFFS in `/spiffs/pet_state.bin` (binary `PersistedState` struct, magic `TOK1`, version 1). Contains pet name, hunger values, last-feed timestamps, and first-boot flag. Recomputed on every save; hunger decay is applied on every `tick()` based on elapsed wall time.

## Display geometry

The Watcher ships a 1.45" **round** touchscreen. LVGL addresses it as a
412×412 surface, but only the inscribed circle (radius ≈ 200 px) is
physically visible — the bezel masks the four corners. All widgets in
`firmware/main/display.cpp` are placed with `LV_ALIGN_CENTER` + polar-safe
offsets so nothing falls outside the circle. At vertical offset `y` from the
centre the usable chord width is `2·√(R² − y²)`; widget widths were chosen
against that bound.

Main screen layout (centre-relative):

```
y = -140   pet name
y =  -90   big emoji
y =   10   sprite (~180 px square)
y =  120   tokens meter bar  (180 px wide)
y =  144   voice meter bar
y =  168   vision meter bar
```

Permission screen replaces the main screen entirely while an approval is
pending. Title at `y=-140`, body wrapped at 240 px, `DENY` / `ALLOW`
buttons side by side at `y=+110, x=±60`.

## Offline behaviour

- Pet state machine runs regardless of network.
- Hunger decays on internal clock (NTP-synced when online, otherwise uses last synced time).
- Voice / photo gestures record to SD and queue for sync.
- Permission requests cannot arrive without network; nothing to do.

## Open questions carried from scoping

1. Claude Code hook stdin payload schema.
2. Claude Code `*.jsonl` log `usage` field shape across versions.
3. LVGL emoji font choice that fits flash budget.
4. ESP-IDF v5.1 `wifi_provisioning` compatibility with Seeed's BLE stack assumptions.
5. SD filename convention — current code uses `audio_<timestamp>.wav` / `photo_<timestamp>.jpg`.
