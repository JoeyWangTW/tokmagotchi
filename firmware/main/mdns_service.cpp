#include "mdns_service.h"
#include "pet_state.h"
#include "constants.h"

#include <algorithm>
#include <cctype>

#include "esp_log.h"
#include "mdns.h"

namespace tokmagotchi {

static const char *TAG = "mdns";

namespace {
bool g_started = false;

std::string slugify(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            out.push_back('-');
        }
    }
    if (out.empty()) out = "pet";
    return out;
}
}

void mdns_service_start() {
    if (g_started) return;
    ESP_ERROR_CHECK(mdns_init());
    mdns_service_update_name();
    mdns_service_add(nullptr, MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO,
                     DEVICE_HTTP_PORT, nullptr, 0);
    g_started = true;
    ESP_LOGI(TAG, "advertised %s%s on :%d", MDNS_SERVICE_TYPE,
             MDNS_SERVICE_PROTO, DEVICE_HTTP_PORT);
}

void mdns_service_update_name() {
    std::string slug = "tokmagotchi-" + slugify(PetState::instance().name());
    mdns_hostname_set(slug.c_str());
    mdns_instance_name_set(PetState::instance().name().c_str());
}

} // namespace tokmagotchi
