#include "input.h"
#include "display.h"
#include "pet_state.h"
#include "audio_capture.h"
#include "camera_capture.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

namespace tokmagotchi {

static const char *TAG = "input";

namespace {

// Placeholder GPIOs — replace with Watcher schematic values.
constexpr gpio_num_t PIN_ENC_A    = GPIO_NUM_4;
constexpr gpio_num_t PIN_ENC_B    = GPIO_NUM_5;
constexpr gpio_num_t PIN_ENC_BTN  = GPIO_NUM_6;
constexpr gpio_num_t PIN_TOUCH_INT = GPIO_NUM_7;

constexpr TickType_t HOLD_THRESHOLD = pdMS_TO_TICKS(600);

void configure_pins() {
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_ENC_A) | (1ULL << PIN_ENC_B) |
                        (1ULL << PIN_ENC_BTN) | (1ULL << PIN_TOUCH_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&in);
}

int read_encoder_delta() {
    // Minimal polled quadrature decoder. Real impl should use PCNT or GPIO
    // ISR. Returns +1 / -1 / 0 per call at ~5ms cadence.
    static int last_a = 0;
    int a = gpio_get_level(PIN_ENC_A);
    int b = gpio_get_level(PIN_ENC_B);
    int delta = 0;
    if (a != last_a && a == 1) {
        delta = b ? -1 : +1;
    }
    last_a = a;
    return delta;
}

void input_task(void *) {
    configure_pins();
    TickType_t button_down_at = 0;
    bool recording = false;

    while (true) {
        int d = read_encoder_delta();
        if (d != 0) {
            if (!display_permission_on_scroll(d)) {
                // No permission UI active — scrolling just acknowledges
                // the pet. Could be used for menus in the future.
            }
        }

        int btn = gpio_get_level(PIN_ENC_BTN);
        if (btn == 0 /* pressed */) {
            if (button_down_at == 0) button_down_at = xTaskGetTickCount();
            TickType_t held = xTaskGetTickCount() - button_down_at;
            if (!recording && held > HOLD_THRESHOLD) {
                recording = true;
                PetState::instance().on_voice_gesture();
                audio_capture_start();
            }
        } else {
            if (button_down_at != 0) {
                TickType_t held = xTaskGetTickCount() - button_down_at;
                if (recording) {
                    audio_capture_stop();
                    PetState::instance().feed_voice();
                    recording = false;
                } else if (held < HOLD_THRESHOLD) {
                    // Short press: confirm permission if pending.
                    display_permission_on_press();
                }
                button_down_at = 0;
            }
        }

        if (gpio_get_level(PIN_TOUCH_INT) == 0) {
            PetState::instance().on_photo_gesture();
            camera_capture_snapshot();
            PetState::instance().feed_vision();
            vTaskDelay(pdMS_TO_TICKS(200));  // debounce
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

} // namespace

void input_start() {
    xTaskCreate(input_task, "input", 4096, nullptr, 6, nullptr);
    ESP_LOGI(TAG, "input task started");
}

} // namespace tokmagotchi
