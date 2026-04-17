#include "audio_capture.h"
#include "sd_storage.h"
#include "constants.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"

namespace tokmagotchi {

static const char *TAG = "audio";

namespace {

constexpr int SAMPLE_RATE = 16000;
constexpr int CHUNK_SAMPLES = 512;

i2s_chan_handle_t g_rx = nullptr;
std::atomic<bool> g_recording{false};
TaskHandle_t g_task = nullptr;
std::string g_current_file;

struct WavHeader {
    // Canonical 16-bit PCM header for SAMPLE_RATE mono.
    char     riff[4]     = {'R','I','F','F'};
    uint32_t chunk_size  = 0;         // filled at stop
    char     wave[4]     = {'W','A','V','E'};
    char     fmt[4]      = {'f','m','t',' '};
    uint32_t fmt_size    = 16;
    uint16_t audio_fmt   = 1;          // PCM
    uint16_t num_channels = 1;
    uint32_t sample_rate = SAMPLE_RATE;
    uint32_t byte_rate   = SAMPLE_RATE * 2;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;
    char     data[4]     = {'d','a','t','a'};
    uint32_t data_size   = 0;          // filled at stop
};

void record_task(void *) {
    std::vector<int16_t> buf(CHUNK_SAMPLES);
    FILE *f = std::fopen(g_current_file.c_str(), "wb");
    if (!f) {
        ESP_LOGE(TAG, "cannot open %s", g_current_file.c_str());
        g_recording = false;
        g_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    WavHeader header{};
    std::fwrite(&header, 1, sizeof(header), f);

    uint32_t data_bytes = 0;
    while (g_recording.load()) {
        size_t bytes_read = 0;
        i2s_channel_read(g_rx, buf.data(), buf.size() * sizeof(int16_t),
                         &bytes_read, pdMS_TO_TICKS(100));
        if (bytes_read) {
            std::fwrite(buf.data(), 1, bytes_read, f);
            data_bytes += bytes_read;
        }
    }

    header.data_size = data_bytes;
    header.chunk_size = 36 + data_bytes;
    std::fseek(f, 0, SEEK_SET);
    std::fwrite(&header, 1, sizeof(header), f);
    std::fclose(f);
    ESP_LOGI(TAG, "wrote %s (%u bytes)", g_current_file.c_str(), data_bytes);

    g_task = nullptr;
    vTaskDelete(nullptr);
}

} // namespace

// Replace these with the Watcher's microphone GPIOs from the OSHW schematic.
// They're intentionally invalid so the init no-ops loudly until configured.
#ifndef TOKMAGOTCHI_MIC_BCK
#define TOKMAGOTCHI_MIC_BCK GPIO_NUM_NC
#endif
#ifndef TOKMAGOTCHI_MIC_WS
#define TOKMAGOTCHI_MIC_WS  GPIO_NUM_NC
#endif
#ifndef TOKMAGOTCHI_MIC_DIN
#define TOKMAGOTCHI_MIC_DIN GPIO_NUM_NC
#endif

void audio_capture_init() {
    if (g_rx) return;
    if (TOKMAGOTCHI_MIC_BCK == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "mic pins not configured — audio disabled");
        return;
    }
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, nullptr, &g_rx);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = TOKMAGOTCHI_MIC_BCK,
            .ws   = TOKMAGOTCHI_MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = TOKMAGOTCHI_MIC_DIN,
            .invert_flags = {},
        },
    };
    i2s_channel_init_std_mode(g_rx, &std_cfg);
    i2s_channel_enable(g_rx);
    ESP_LOGI(TAG, "mic initialised @ %d Hz mono", SAMPLE_RATE);
}

void audio_capture_start() {
    if (g_recording.load()) return;
    if (!g_rx) {
        ESP_LOGW(TAG, "mic not initialised, cannot record");
        return;
    }
    if (!sd_storage_available()) {
        ESP_LOGW(TAG, "SD unavailable, cannot record");
        return;
    }
    g_current_file = std::string(SD_MOUNT_POINT) + "/" +
                     sd_storage_make_filename("audio", "wav");
    g_recording = true;
    xTaskCreate(record_task, "audio_rec", 4096, nullptr, 5, &g_task);
}

std::string audio_capture_stop() {
    if (!g_recording.load()) return {};
    g_recording = false;
    // The task flushes and exits on its own; caller can poll file size later.
    return g_current_file;
}

} // namespace tokmagotchi
