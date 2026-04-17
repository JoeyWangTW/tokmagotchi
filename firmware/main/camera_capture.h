#pragma once

#include <string>

namespace tokmagotchi {

// The Watcher's OV5647 is wired to the Himax co-processor, not directly to
// the ESP32. For Phase 1 we treat the camera as opaque and request a frame
// by sending an "RPC" over the internal UART/SPI link defined by the Himax
// firmware. The implementation below is a shim — actual bytes-on-wire must
// match the vendor protocol documented in the OSHW repo.
void camera_capture_init();

// Triggers a single photo capture and writes it to SD. Non-blocking; returns
// the filename the image will be written to, or empty if unavailable.
std::string camera_capture_snapshot();

} // namespace tokmagotchi
