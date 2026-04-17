#include "pet_state.h"
#include "constants.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace tokmagotchi {

static const char *TAG = "pet_state";

namespace {
constexpr float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}
}

PetState &PetState::instance() {
    static PetState s;
    return s;
}

PetState::PetState() {
    mutex_ = xSemaphoreCreateMutex();
    std::memset(&p_, 0, sizeof(p_));
    p_.magic = PersistedState::MAGIC;
    p_.version = PersistedState::VERSION;
    p_.token_hunger = 1.0f;
    p_.voice_hunger = 1.0f;
    p_.vision_hunger = 1.0f;
    p_.first_boot = 1;
    std::strncpy(p_.name, "Pixel", sizeof(p_.name) - 1);
}

bool PetState::load() {
    FILE *f = std::fopen(STATE_FILE_PATH, "rb");
    if (!f) {
        ESP_LOGI(TAG, "no persisted state, first boot");
        return false;
    }
    PersistedState tmp{};
    size_t n = std::fread(&tmp, 1, sizeof(tmp), f);
    std::fclose(f);
    if (n != sizeof(tmp) || tmp.magic != PersistedState::MAGIC) {
        ESP_LOGW(TAG, "persisted state invalid, resetting");
        return false;
    }
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    p_ = tmp;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    ESP_LOGI(TAG, "loaded pet '%s' t=%.2f v=%.2f p=%.2f",
             p_.name, p_.token_hunger, p_.voice_hunger, p_.vision_hunger);
    return true;
}

void PetState::save() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    PersistedState copy = p_;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));

    FILE *f = std::fopen(STATE_FILE_PATH, "wb");
    if (!f) {
        ESP_LOGE(TAG, "cannot open state file for write");
        return;
    }
    std::fwrite(&copy, 1, sizeof(copy), f);
    std::fclose(f);
}

void PetState::tick(time_t now) {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    if (p_.last_updated == 0) {
        p_.last_updated = now;
    }
    double dt_hours = (now - p_.last_updated) / 3600.0;
    if (dt_hours > 0) {
        p_.token_hunger  = clamp01(p_.token_hunger  - DECAY_TOKENS * dt_hours);
        p_.voice_hunger  = clamp01(p_.voice_hunger  - DECAY_VOICE  * dt_hours);
        p_.vision_hunger = clamp01(p_.vision_hunger - DECAY_VISION * dt_hours);
        p_.last_updated = now;
    }
    recompute_mood_locked(now);
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::feed_token(bool large, int tokens) {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    float amount = large ? FEED_AMOUNT_TOKEN_LARGE : FEED_AMOUNT_TOKEN_SMALL;
    p_.token_hunger = clamp01(p_.token_hunger + amount);
    p_.last_token_feed = time(nullptr);
    reaction_ = large ? ReactionKind::TOKEN_LARGE : ReactionKind::TOKEN_SMALL;
    reaction_until_ms_ = esp_timer_get_time() / 1000 + REACTION_DURATION_MS;
    ESP_LOGI(TAG, "token feed %s (%d tokens) → hunger=%.2f",
             large ? "LARGE" : "small", tokens, p_.token_hunger);
    recompute_mood_locked(time(nullptr));
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::feed_voice() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    p_.voice_hunger = clamp01(p_.voice_hunger + FEED_AMOUNT_VOICE);
    p_.last_voice_feed = time(nullptr);
    reaction_ = ReactionKind::VOICE;
    reaction_until_ms_ = esp_timer_get_time() / 1000 + REACTION_DURATION_MS;
    recompute_mood_locked(time(nullptr));
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::feed_vision() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    p_.vision_hunger = clamp01(p_.vision_hunger + FEED_AMOUNT_VISION);
    p_.last_vision_feed = time(nullptr);
    reaction_ = ReactionKind::PHOTO;
    reaction_until_ms_ = esp_timer_get_time() / 1000 + REACTION_DURATION_MS;
    recompute_mood_locked(time(nullptr));
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::on_permission_pending() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    permission_pending_ = true;
    mood_ = PetMood::ALERT;
    reaction_ = ReactionKind::NONE;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::on_permission_resolved(bool allowed) {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    permission_pending_ = false;
    reaction_ = allowed ? ReactionKind::PERMISSION_OK : ReactionKind::PERMISSION_NO;
    reaction_until_ms_ = esp_timer_get_time() / 1000 + REACTION_DURATION_MS;
    recompute_mood_locked(time(nullptr));
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::on_voice_gesture() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    reaction_ = ReactionKind::VOICE;
    reaction_until_ms_ = esp_timer_get_time() / 1000 + REACTION_DURATION_MS;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::on_photo_gesture() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    reaction_ = ReactionKind::PHOTO;
    reaction_until_ms_ = esp_timer_get_time() / 1000 + REACTION_DURATION_MS;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

void PetState::refresh_mood(time_t now) {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    recompute_mood_locked(now);
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
}

bool PetState::is_sleep_hour_locked(time_t now) {
    struct tm lt;
    localtime_r(&now, &lt);
    // Wraps across midnight: SLEEP_HOUR .. 23, 0 .. WAKE_HOUR-1.
    if (SLEEP_HOUR > WAKE_HOUR) {
        return lt.tm_hour >= SLEEP_HOUR || lt.tm_hour < WAKE_HOUR;
    }
    return lt.tm_hour >= SLEEP_HOUR && lt.tm_hour < WAKE_HOUR;
}

void PetState::recompute_mood_locked(time_t now) {
    int64_t now_ms = esp_timer_get_time() / 1000;
    bool reaction_active = reaction_ != ReactionKind::NONE && now_ms < reaction_until_ms_;
    if (!reaction_active) {
        reaction_ = ReactionKind::NONE;
    }

    // Priority: SLEEPING > ALERT > REACTING > EXCITED > HAPPY > CONTENT > IDLE > HUNGRY > VERY_HUNGRY.
    if (permission_pending_) {
        mood_ = PetMood::ALERT;
        return;
    }
    if (is_sleep_hour_locked(now)) {
        mood_ = PetMood::SLEEPING;
        return;
    }
    if (reaction_active) {
        if (reaction_ == ReactionKind::TOKEN_LARGE) {
            mood_ = PetMood::EXCITED;
        } else if (reaction_ == ReactionKind::TOKEN_SMALL) {
            mood_ = PetMood::HAPPY;
        } else {
            mood_ = PetMood::REACTING;
        }
        return;
    }

    float min_meter = std::min({p_.token_hunger, p_.voice_hunger, p_.vision_hunger});
    int low_count = (p_.token_hunger  < 0.3f ? 1 : 0)
                  + (p_.voice_hunger  < 0.3f ? 1 : 0)
                  + (p_.vision_hunger < 0.3f ? 1 : 0);

    if (min_meter < 0.1f || low_count >= 2) {
        mood_ = PetMood::VERY_HUNGRY;
    } else if (min_meter < 0.3f) {
        mood_ = PetMood::HUNGRY;
    } else if (min_meter > 0.5f) {
        mood_ = PetMood::CONTENT;
    } else {
        mood_ = PetMood::IDLE;
    }
}

PetState::Snapshot PetState::snapshot() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    Snapshot s;
    s.mood = mood_;
    s.reaction = reaction_;
    s.meters.tokens = p_.token_hunger;
    s.meters.voice = p_.voice_hunger;
    s.meters.vision = p_.vision_hunger;
    s.name = p_.name;
    s.last_token_feed = p_.last_token_feed;
    s.last_voice_feed = p_.last_voice_feed;
    s.last_vision_feed = p_.last_vision_feed;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return s;
}

void PetState::set_name(const std::string &name) {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    std::memset(p_.name, 0, sizeof(p_.name));
    std::strncpy(p_.name, name.c_str(), sizeof(p_.name) - 1);
    p_.first_boot = 0;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    save();
}

std::string PetState::name() {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    std::string n = p_.name;
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return n;
}

const char *PetState::current_emoji() {
    // UTF-8 byte sequences. These need an emoji-capable font on the LVGL side.
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(mutex_), portMAX_DELAY);
    const char *e = "\xF0\x9F\x98\x8A";  // 😊 default
    if (reaction_ != ReactionKind::NONE) {
        switch (reaction_) {
            case ReactionKind::TOKEN_SMALL:   e = "\xF0\x9F\x98\x84"; break; // 😄
            case ReactionKind::TOKEN_LARGE:   e = "\xF0\x9F\xA4\xA9"; break; // 🤩
            case ReactionKind::PERMISSION_OK: e = "\xE2\x9C\x85";     break; // ✅
            case ReactionKind::PERMISSION_NO: e = "\xF0\x9F\x9A\xAB"; break; // 🚫
            case ReactionKind::VOICE:         e = "\xF0\x9F\x8E\xA7"; break; // 🎧
            case ReactionKind::PHOTO:         e = "\xF0\x9F\x91\x80"; break; // 👀
            case ReactionKind::MORNING:       e = "\xE2\x98\x80\xEF\xB8\x8F"; break; // ☀️
            default: break;
        }
    } else {
        switch (mood_) {
            case PetMood::SLEEPING:    e = "\xF0\x9F\x92\xA4"; break; // 💤
            case PetMood::ALERT:       e = "\xF0\x9F\x9A\xA8"; break; // 🚨
            case PetMood::EXCITED:     e = "\xF0\x9F\xA4\xA9"; break; // 🤩
            case PetMood::HAPPY:       e = "\xF0\x9F\x98\x84"; break; // 😄
            case PetMood::CONTENT:     e = "\xF0\x9F\x98\x8A"; break; // 😊
            case PetMood::IDLE:        e = "\xF0\x9F\x98\x90"; break; // 😐
            case PetMood::HUNGRY:      e = "\xF0\x9F\x98\xA9"; break; // 😩
            case PetMood::VERY_HUNGRY: e = "\xF0\x9F\x98\xAD"; break; // 😭
            default: break;
        }
    }
    xSemaphoreGive(static_cast<SemaphoreHandle_t>(mutex_));
    return e;
}

const char *PetState::mood_label(PetMood m) {
    switch (m) {
        case PetMood::SLEEPING:    return "SLEEPING";
        case PetMood::ALERT:       return "ALERT";
        case PetMood::REACTING:    return "REACTING";
        case PetMood::EXCITED:     return "EXCITED";
        case PetMood::HAPPY:       return "HAPPY";
        case PetMood::CONTENT:     return "CONTENT";
        case PetMood::IDLE:        return "IDLE";
        case PetMood::HUNGRY:      return "HUNGRY";
        case PetMood::VERY_HUNGRY: return "VERY_HUNGRY";
    }
    return "UNKNOWN";
}

} // namespace tokmagotchi
