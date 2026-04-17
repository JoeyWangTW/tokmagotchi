#pragma once

#include <string>
#include <vector>

namespace tokmagotchi {

struct MediaFile {
    std::string name;
    std::string type;  // "audio" | "photo"
    size_t size = 0;
};

// Mount the SD card at SD_MOUNT_POINT. Safe to call when no card is present;
// subsequent write operations will fail gracefully.
bool sd_storage_mount();

// List media files on the card. Returns an empty vector if unmounted.
std::vector<MediaFile> sd_storage_list();

// Generate a unique filename like "audio_20260417_143022.wav" using local time.
std::string sd_storage_make_filename(const char *prefix, const char *ext);

// Returns true if the SD card is mounted and writable right now.
bool sd_storage_available();

} // namespace tokmagotchi
