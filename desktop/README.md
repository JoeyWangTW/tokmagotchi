# Tokmagotchi Desktop

Electron + React desktop companion for the Tokmagotchi pet. Parses Claude Code logs, bridges the local hook server to the device, and provides the pairing + dashboard UI.

## Quick start

```bash
cd desktop
npm install
npm run dev      # electron-vite dev server with HMR
```

Production build:

```bash
npm run build
npm run start
```

## What it does

- **Log watcher** (`src/main/log-watcher.ts`) — tails `~/.claude/projects/**/*.jsonl` and emits a feed event for every entry with a `usage` field. Threshold for `large` feed: 1000 tokens.
- **Hook server** (`src/main/hook-server.ts`) — Express on `127.0.0.1:27182`. Receives `POST /permission` from `tokmagotchi-hook`, blocks until the device resolves it (or 30 s timeout), returns the decision.
- **Device bridge** (`src/main/device-bridge.ts`) — mDNS browse for `_tokmagotchi._tcp`, HTTP client for the firmware's `/feed` `/permission` `/state` `/media/*` endpoints. Polls state every 3 s.
- **BLE provisioning** (`src/main/ble-provision.ts`) — first-boot WiFi setup via the ESP-IDF `esp-prov` tool (Python subprocess). Optional `@abandonware/noble` fallback for raw scan.
- **Installer** (`src/main/installer.ts`) — writes `~/.local/bin/tokmagotchi-hook` and registers it in `~/.claude/settings.json` as a `PreToolUse` hook.

## Dependencies

Required at runtime:

- `bonjour-service` — mDNS discovery.
- `chokidar` — log file watching.
- `express` — local hook server.

Optional (for BLE scanning):

- `@abandonware/noble` — requires platform BLE stack (BlueZ on Linux, CoreBluetooth on macOS, Windows BLE APIs).

External tools:

- [`esp-prov`](https://github.com/espressif/esp-idf-provisioning) — invoked as a subprocess for BLE provisioning. Install via ESP-IDF or `pip install esp-idf-provisioning`.

## Directory layout

```
desktop/
├── src/
│   ├── main/         Electron main process
│   ├── preload/      contextBridge API surface
│   └── renderer/     React UI
├── shared/           types + constants (shared with main + renderer)
├── electron.vite.config.ts
└── tsconfig*.json
```
