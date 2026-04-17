#include "pet_state.h"
#include "display.h"
#include "http_server.h"
#include "ble_provision.h"
#include "audio_capture.h"
#include "camera_capture.h"
#include "mdns_service.h"
#include "time_sync.h"
#include "input.h"
#include "sd_storage.h"
#include "constants.h"

#include <cstring>
#include <ctime>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace tokmagotchi {

static const char *TAG = "app_main";

namespace {

bool g_wifi_connected = false;

void on_wifi_event(void *, esp_event_base_t base, int32_t id, void *) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        g_wifi_connected = false;
        ESP_LOGW(TAG, "wifi disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        g_wifi_connected = true;
        ESP_LOGI(TAG, "wifi got IP");
        time_sync_start();
        mdns_service_start();
        http_server_start();
    }
}

void init_wifi_sta() {
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, nullptr);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP,    &on_wifi_event, nullptr);
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}

void mount_spiffs() {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = STATE_MOUNT_POINT,
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&conf);
}

void on_provisioned(const WifiCreds &c) {
    if (!c.pet_name.empty()) {
        PetState::instance().set_name(c.pet_name);
        mdns_service_update_name();
    }
    // The ESP provisioning manager already saved the WiFi creds to NVS and
    // will attempt STA connect after WIFI_PROV_END fires. Nothing else to do.
}

void state_tick_task(void *) {
    // Save every minute, re-evaluate mood every few seconds.
    int since_save = 0;
    while (true) {
        time_t now = std::time(nullptr);
        PetState::instance().tick(now);
        display_render();
        vTaskDelay(pdMS_TO_TICKS(2000));
        since_save += 2;
        if (since_save >= 60) {
            PetState::instance().save();
            since_save = 0;
        }
    }
}

} // namespace

} // namespace tokmagotchi

extern "C" void app_main(void) {
    using namespace tokmagotchi;
    ESP_LOGI(TAG, "tokmagotchi firmware booting");

    // NVS — needed by WiFi credentials + provisioning.
    esp_err_t r = nvs_flash_init();
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    mount_spiffs();
    sd_storage_mount();
    audio_capture_init();
    camera_capture_init();

    bool loaded = PetState::instance().load();
    if (!loaded) {
        ESP_LOGI(TAG, "first boot — starting BLE provisioning");
        init_wifi_sta();  // provisioning manager needs WiFi initialised
        ble_provision_start(on_provisioned);
    } else {
        init_wifi_sta();  // uses creds already in NVS
    }

    time_sync_set_timezone("UTC0");  // desktop can override via a future /tz endpoint

    display_init();
    input_start();

    // Apply decay for time elapsed while powered off, then kick off the
    // periodic tick task.
    PetState::instance().tick(std::time(nullptr));
    xTaskCreate(state_tick_task, "tick", 4096, nullptr, 4, nullptr);

    ESP_LOGI(TAG, "boot complete: pet '%s'",
             PetState::instance().name().c_str());
}
