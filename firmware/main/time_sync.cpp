#include "time_sync.h"

#include <ctime>
#include <cstdlib>

#include "esp_log.h"
#include "esp_sntp.h"

namespace tokmagotchi {

static const char *TAG = "time";

void time_sync_start() {
    if (esp_sntp_enabled()) return;
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started");
}

void time_sync_set_timezone(const char *tz) {
    if (!tz || !*tz) tz = "UTC0";
    setenv("TZ", tz, 1);
    tzset();
}

bool time_sync_is_ready() {
    time_t now = std::time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    return (lt.tm_year + 1900) >= 2024;
}

} // namespace tokmagotchi
