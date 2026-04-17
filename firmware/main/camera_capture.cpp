#include "camera_capture.h"
#include "sd_storage.h"
#include "constants.h"

#include <cstdio>

#include "esp_log.h"

namespace tokmagotchi {

static const char *TAG = "camera";

namespace {
bool g_inited = false;
}

void camera_capture_init() {
    if (g_inited) return;
    // TODO: configure UART/SPI link to Himax chip and perform handshake.
    // The OSHW repo defines the command frame layout; this stub lets the
    // rest of the firmware build and exercise its code paths.
    g_inited = true;
    ESP_LOGI(TAG, "camera bridge initialised (stub)");
}

std::string camera_capture_snapshot() {
    if (!sd_storage_available()) {
        ESP_LOGW(TAG, "SD unavailable, cannot snap");
        return {};
    }
    std::string name = sd_storage_make_filename("photo", "jpg");
    std::string path = std::string(SD_MOUNT_POINT) + "/" + name;

    // TODO: request JPEG frame from Himax, stream bytes to `path`. For now
    // we write a zero-byte placeholder so media sync/listing still works
    // end-to-end during integration.
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return {};
    std::fclose(f);

    ESP_LOGI(TAG, "snapshot → %s", path.c_str());
    return name;
}

} // namespace tokmagotchi
