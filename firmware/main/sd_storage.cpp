#include "sd_storage.h"
#include "constants.h"

#include <dirent.h>
#include <sys/stat.h>
#include <ctime>
#include <cstdio>
#include <cstring>

#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

namespace tokmagotchi {

static const char *TAG = "sd";

namespace {
sdmmc_card_t *g_card = nullptr;
bool g_mounted = false;

const char *ext_type(const std::string &name) {
    if (name.size() >= 4) {
        if (name.compare(name.size() - 4, 4, ".wav") == 0) return "audio";
        if (name.compare(name.size() - 4, 4, ".jpg") == 0) return "photo";
        if (name.compare(name.size() - 4, 4, ".png") == 0) return "photo";
    }
    return "other";
}
}

bool sd_storage_mount() {
    if (g_mounted) return true;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false,
    };
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;  // Watcher SD uses 1-bit mode; consult OSHW schematic.

    esp_err_t r = esp_vfs_fat_sdmmc_mount(SD_MOUNT_POINT, &host, &slot,
                                          &mount_cfg, &g_card);
    if (r != ESP_OK) {
        ESP_LOGW(TAG, "SD mount failed: %s", esp_err_to_name(r));
        return false;
    }
    g_mounted = true;
    ESP_LOGI(TAG, "SD mounted (%llu MB)", ((uint64_t)g_card->csd.capacity) *
             g_card->csd.sector_size / (1024 * 1024));
    return true;
}

bool sd_storage_available() { return g_mounted; }

std::vector<MediaFile> sd_storage_list() {
    std::vector<MediaFile> out;
    if (!g_mounted) return out;
    DIR *d = opendir(SD_MOUNT_POINT);
    if (!d) return out;
    struct dirent *ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_type != DT_REG) continue;
        MediaFile f;
        f.name = ent->d_name;
        f.type = ext_type(f.name);
        std::string full = std::string(SD_MOUNT_POINT) + "/" + f.name;
        struct stat st;
        if (stat(full.c_str(), &st) == 0) f.size = st.st_size;
        out.push_back(std::move(f));
    }
    closedir(d);
    return out;
}

std::string sd_storage_make_filename(const char *prefix, const char *ext) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s_%04d%02d%02d_%02d%02d%02d.%s",
                  prefix,
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday,
                  lt.tm_hour, lt.tm_min, lt.tm_sec,
                  ext);
    return buf;
}

} // namespace tokmagotchi
