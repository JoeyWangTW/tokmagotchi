// Keep in sync with firmware/main/constants.h.

export const HOOK_SERVER_PORT = 27182;
export const DEVICE_HTTP_PORT = 8080;
export const PERMISSION_TIMEOUT_MS = 30_000;
export const MDNS_SERVICE_TYPE = "tokmagotchi";   // bonjour strips leading underscore
export const MDNS_SERVICE_PROTO = "tcp";

// Hunger decay per hour (0.0–1.0).
export const DECAY_TOKENS = 0.10;
export const DECAY_VOICE  = 0.05;
export const DECAY_VISION = 0.03;

// Feed thresholds.
export const TOKEN_FEED_LARGE_THRESHOLD = 1000;

// Sleep schedule (24h local time).
export const SLEEP_HOUR = 22;
export const WAKE_HOUR  = 7;
