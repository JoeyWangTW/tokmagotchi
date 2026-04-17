#pragma once

#include <stdint.h>
#include <time.h>
#include <string>

namespace tokmagotchi {

enum class PetMood {
    SLEEPING,
    ALERT,
    REACTING,
    EXCITED,
    HAPPY,
    CONTENT,
    IDLE,
    HUNGRY,
    VERY_HUNGRY,
};

enum class ReactionKind {
    NONE,
    TOKEN_SMALL,     // 😄
    TOKEN_LARGE,     // 🤩
    PERMISSION_OK,   // ✅
    PERMISSION_NO,   // 🚫
    VOICE,           // 🎧
    PHOTO,           // 👀
    MORNING,         // ☀️
};

struct Meters {
    float tokens = 1.0f;
    float voice  = 1.0f;
    float vision = 1.0f;
};

struct PersistedState {
    // Layout is stable for SPIFFS persistence; bump VERSION on change.
    static constexpr uint32_t MAGIC = 0x544F4B31;  // "TOK1"
    static constexpr uint32_t VERSION = 1;

    uint32_t magic;
    uint32_t version;
    char     name[32];
    float    token_hunger;
    float    voice_hunger;
    float    vision_hunger;
    int64_t  last_updated;       // unix seconds
    int64_t  last_token_feed;
    int64_t  last_voice_feed;
    int64_t  last_vision_feed;
    uint8_t  first_boot;
    uint8_t  _pad[3];
};

// Thread-safe singleton. All accessors take an internal mutex.
class PetState {
public:
    static PetState &instance();

    // Load from SPIFFS. Returns true if state existed, false if first boot.
    bool load();
    void save();

    // Apply decay based on elapsed real time since last_updated.
    void tick(time_t now);

    // Feed events.
    void feed_token(bool large, int tokens);
    void feed_voice();
    void feed_vision();

    // Permission lifecycle.
    void on_permission_pending();
    void on_permission_resolved(bool allowed);

    // Gesture reactions.
    void on_voice_gesture();
    void on_photo_gesture();

    // Time-of-day transitions.
    void refresh_mood(time_t now);

    // Accessors (snapshot — thread-safe).
    struct Snapshot {
        PetMood mood;
        ReactionKind reaction;
        Meters meters;
        std::string name;
        int64_t last_token_feed;
        int64_t last_voice_feed;
        int64_t last_vision_feed;
    };
    Snapshot snapshot();

    // Name assignment (first boot / pairing).
    void set_name(const std::string &name);
    std::string name();

    // Returns the emoji codepoint sequence for current mood/reaction.
    const char *current_emoji();

    // Map mood+reaction to state machine priority. Exposed for the display
    // layer so it can re-render without racing with mutators.
    static const char *mood_label(PetMood m);

private:
    PetState();
    void recompute_mood_locked(time_t now);
    static bool is_sleep_hour_locked(time_t now);

    PersistedState p_;
    PetMood mood_ = PetMood::CONTENT;
    ReactionKind reaction_ = ReactionKind::NONE;
    int64_t reaction_until_ms_ = 0;
    bool permission_pending_ = false;
    void *mutex_;  // SemaphoreHandle_t, opaque here to keep header C-clean.
};

} // namespace tokmagotchi
