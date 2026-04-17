#pragma once

#include <string>

namespace tokmagotchi {

// Initialise I2S microphone driver. Safe no-op if already init.
void audio_capture_init();

// Begin/end a recording window. The caller drives these based on the user
// holding the scroll wheel. Output is a WAV file on the SD card.
// Returns the filename on success (empty on failure).
void audio_capture_start();
std::string audio_capture_stop();

} // namespace tokmagotchi
