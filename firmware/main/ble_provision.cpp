#include "ble_provision.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

#include <cstring>

// Uses the ESP-IDF wifi_provisioning manager with BLE transport. The desktop
// app's BLE client speaks the same "proto-ver v1.1" protocol via
// esp-idf-provisioning libs, or we can use Seeed's custom GATT profile; this
// file goes with the official flow which is stable across IDF versions.

namespace tokmagotchi {

static const char *TAG = "ble_prov";

namespace {
ProvisionedCb g_cb;
bool g_started = false;

void on_event(void *, esp_event_base_t base, int32_t id, void *data) {
    if (base != WIFI_PROV_EVENT) return;
    switch (id) {
        case WIFI_PROV_START:
            ESP_LOGI(TAG, "BLE provisioning started");
            break;
        case WIFI_PROV_CRED_RECV: {
            auto *cred = static_cast<wifi_sta_config_t *>(data);
            ESP_LOGI(TAG, "received creds ssid=%s", (char *)cred->ssid);
            if (g_cb) {
                WifiCreds c;
                c.ssid = reinterpret_cast<char *>(cred->ssid);
                c.password = reinterpret_cast<char *>(cred->password);
                g_cb(c);
            }
            break;
        }
        case WIFI_PROV_CRED_FAIL:
            ESP_LOGW(TAG, "provisioning failed");
            break;
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "provisioning success");
            break;
        case WIFI_PROV_END:
            wifi_prov_mgr_deinit();
            g_started = false;
            break;
        default:
            break;
    }
}

// Custom GATT endpoint — desktop writes JSON like:
//   { "name": "Pixel" }
// during the provisioning window so the pet name arrives with the WiFi creds.
esp_err_t custom_name_handler(uint32_t, const uint8_t *inbuf, ssize_t inlen,
                              uint8_t **outbuf, ssize_t *outlen, void *) {
    if (inbuf && inlen > 0 && g_cb) {
        // Minimal parser: find "name":"..." and extract between quotes. The
        // desktop controls this payload, so we keep parsing permissive.
        std::string body(reinterpret_cast<const char *>(inbuf), inlen);
        auto k = body.find("\"name\"");
        if (k != std::string::npos) {
            auto q1 = body.find('"', k + 6);
            auto q2 = body.find('"', q1 + 1);
            auto q3 = body.find('"', q2 + 1);
            if (q2 != std::string::npos && q3 != std::string::npos) {
                WifiCreds partial;
                partial.pet_name = body.substr(q2 + 1, q3 - q2 - 1);
                g_cb(partial);
            }
        }
    }
    const char *resp = "{\"ok\":true}";
    *outlen = std::strlen(resp);
    *outbuf = static_cast<uint8_t *>(std::malloc(*outlen));
    std::memcpy(*outbuf, resp, *outlen);
    return ESP_OK;
}

} // namespace

void ble_provision_start(ProvisionedCb cb) {
    if (g_started) return;
    g_cb = std::move(cb);

    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &on_event, nullptr);

    wifi_prov_mgr_config_t cfg = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(cfg));

    // Service name advertised over BLE: "tokmagotchi-<last 3 mac bytes>".
    uint8_t mac[6] = {};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char service[32];
    std::snprintf(service, sizeof(service), "tokmagotchi-%02X%02X%02X",
                  mac[3], mac[4], mac[5]);

    wifi_prov_mgr_endpoint_create("tokmagotchi-name");
    ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(
        WIFI_PROV_SECURITY_1, "tokmagotchi-pop", service, nullptr));
    wifi_prov_mgr_endpoint_register("tokmagotchi-name", custom_name_handler, nullptr);

    g_started = true;
    ESP_LOGI(TAG, "advertising as '%s'", service);
}

void ble_provision_stop() {
    if (!g_started) return;
    wifi_prov_mgr_stop_provisioning();
    wifi_prov_mgr_deinit();
    g_started = false;
}

} // namespace tokmagotchi
