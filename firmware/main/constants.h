#pragma once

// Tokmagotchi shared firmware constants.
// Keep in sync with desktop/shared/constants.ts.

namespace tokmagotchi {

constexpr int DEVICE_HTTP_PORT = 8080;
constexpr int PERMISSION_TIMEOUT_MS = 30'000;
constexpr const char *MDNS_SERVICE_TYPE = "_tokmagotchi";
constexpr const char *MDNS_SERVICE_PROTO = "_tcp";

// Hunger decay per hour, 0.0–1.0 scale.
constexpr float DECAY_TOKENS = 0.10f;
constexpr float DECAY_VOICE  = 0.05f;
constexpr float DECAY_VISION = 0.03f;

// Feed thresholds.
constexpr int TOKEN_FEED_LARGE_THRESHOLD = 1000;

// Feed magnitudes — how much each feed refills the relevant meter.
constexpr float FEED_AMOUNT_TOKEN_SMALL = 0.15f;
constexpr float FEED_AMOUNT_TOKEN_LARGE = 0.40f;
constexpr float FEED_AMOUNT_VOICE       = 0.50f;
constexpr float FEED_AMOUNT_VISION      = 0.50f;

// Sleep schedule (24h local time).
constexpr int SLEEP_HOUR = 22;
constexpr int WAKE_HOUR  = 7;

// Reaction states last this long before state machine re-evaluates.
constexpr int REACTION_DURATION_MS = 4000;

// Display geometry — the Watcher's panel is a 412×412 square where only the
// inscribed circle is visible. All LVGL widget placement must stay inside
// this circle, so we work in polar-safe offsets from the centre rather than
// pinning to corners.
constexpr int DISPLAY_SIZE_PX   = 412;
constexpr int DISPLAY_RADIUS_PX = 206;
constexpr int DISPLAY_SAFE_RADIUS_PX = 196;  // leave 10px for bezel / AA
constexpr int DISPLAY_CENTER_X  = DISPLAY_SIZE_PX / 2;
constexpr int DISPLAY_CENTER_Y  = DISPLAY_SIZE_PX / 2;

// Persisted state file on SPIFFS.
constexpr const char *STATE_FILE_PATH = "/spiffs/pet_state.bin";
constexpr const char *STATE_MOUNT_POINT = "/spiffs";

// SD mount point.
constexpr const char *SD_MOUNT_POINT = "/sdcard";

} // namespace tokmagotchi
