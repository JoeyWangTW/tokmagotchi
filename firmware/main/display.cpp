#include "display.h"
#include "pet_state.h"
#include "constants.h"

#include "esp_log.h"

// NOTE: LVGL integration is sketched here against the LVGL v8 API. The
// SenseCAP Watcher ships with a specific panel driver (SPI 412×412). Wiring
// up lv_port_disp and the touch/encoder input devices must reference the
// OSHW repo's board support when building on real hardware.
//
// For the purposes of this Phase 1 firmware skeleton, display_init creates
// the UI widgets and exposes the same public surface the rest of the
// firmware depends on. Replace the `#if 0` block with the actual LVGL init
// from the Watcher BSP when integrating.

#if __has_include("lvgl.h")
#include "lvgl.h"
#define TOKMAGOTCHI_HAS_LVGL 1
#else
#define TOKMAGOTCHI_HAS_LVGL 0
#endif

namespace tokmagotchi {

static const char *TAG = "display";

namespace {

PermissionDecisionCb g_decision_cb = nullptr;

bool g_perm_active = false;
PermissionDisplayRequest g_perm_req;
bool g_perm_highlight_allow = true;  // true=ALLOW, false=DENY

#if TOKMAGOTCHI_HAS_LVGL
lv_obj_t *g_scr_main = nullptr;
lv_obj_t *g_lbl_name = nullptr;
lv_obj_t *g_lbl_emoji = nullptr;
lv_obj_t *g_img_sprite = nullptr;
lv_obj_t *g_bar_tokens = nullptr;
lv_obj_t *g_bar_voice = nullptr;
lv_obj_t *g_bar_vision = nullptr;

lv_obj_t *g_scr_perm = nullptr;
lv_obj_t *g_lbl_perm_title = nullptr;
lv_obj_t *g_lbl_perm_body = nullptr;
lv_obj_t *g_btn_allow = nullptr;
lv_obj_t *g_btn_deny = nullptr;

void build_main_screen() {
    g_scr_main = lv_obj_create(nullptr);

    g_lbl_name = lv_label_create(g_scr_main);
    lv_obj_align(g_lbl_name, LV_ALIGN_TOP_LEFT, 10, 8);

    g_lbl_emoji = lv_label_create(g_scr_main);
    lv_obj_align(g_lbl_emoji, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_label_set_text(g_lbl_emoji, "\xF0\x9F\x98\x8A");

    g_img_sprite = lv_img_create(g_scr_main);
    lv_obj_align(g_img_sprite, LV_ALIGN_CENTER, 0, -20);

    g_bar_tokens = lv_bar_create(g_scr_main);
    lv_obj_set_size(g_bar_tokens, 280, 14);
    lv_obj_align(g_bar_tokens, LV_ALIGN_BOTTOM_MID, 10, -60);
    lv_bar_set_range(g_bar_tokens, 0, 100);

    g_bar_voice = lv_bar_create(g_scr_main);
    lv_obj_set_size(g_bar_voice, 280, 14);
    lv_obj_align(g_bar_voice, LV_ALIGN_BOTTOM_MID, 10, -38);
    lv_bar_set_range(g_bar_voice, 0, 100);

    g_bar_vision = lv_bar_create(g_scr_main);
    lv_obj_set_size(g_bar_vision, 280, 14);
    lv_obj_align(g_bar_vision, LV_ALIGN_BOTTOM_MID, 10, -16);
    lv_bar_set_range(g_bar_vision, 0, 100);
}

void build_perm_screen() {
    g_scr_perm = lv_obj_create(nullptr);
    g_lbl_perm_title = lv_label_create(g_scr_perm);
    lv_label_set_text(g_lbl_perm_title, "\xF0\x9F\x9A\xA8  PERMISSION NEEDED");
    lv_obj_align(g_lbl_perm_title, LV_ALIGN_TOP_MID, 0, 20);

    g_lbl_perm_body = lv_label_create(g_scr_perm);
    lv_label_set_long_mode(g_lbl_perm_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(g_lbl_perm_body, 360);
    lv_obj_align(g_lbl_perm_body, LV_ALIGN_CENTER, 0, 0);

    g_btn_deny = lv_btn_create(g_scr_perm);
    lv_obj_align(g_btn_deny, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_t *ld = lv_label_create(g_btn_deny);
    lv_label_set_text(ld, "DENY");

    g_btn_allow = lv_btn_create(g_scr_perm);
    lv_obj_align(g_btn_allow, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_t *la = lv_label_create(g_btn_allow);
    lv_label_set_text(la, "ALLOW");
}
#endif // TOKMAGOTCHI_HAS_LVGL

} // namespace

void display_init() {
#if TOKMAGOTCHI_HAS_LVGL
    // The real LVGL init (lv_init, lv_port_disp_init, lv_port_indev_init)
    // lives in the board support package. After that runs:
    build_main_screen();
    build_perm_screen();
    lv_scr_load(g_scr_main);
    ESP_LOGI(TAG, "display ready");
#else
    ESP_LOGW(TAG, "LVGL not available at build time — display is a stub");
#endif
}

void display_render() {
    auto snap = PetState::instance().snapshot();
#if TOKMAGOTCHI_HAS_LVGL
    if (!g_perm_active) {
        lv_label_set_text(g_lbl_name, snap.name.c_str());
        lv_label_set_text(g_lbl_emoji, PetState::instance().current_emoji());
        lv_bar_set_value(g_bar_tokens, (int)(snap.meters.tokens * 100), LV_ANIM_OFF);
        lv_bar_set_value(g_bar_voice,  (int)(snap.meters.voice  * 100), LV_ANIM_OFF);
        lv_bar_set_value(g_bar_vision, (int)(snap.meters.vision * 100), LV_ANIM_OFF);
    } else {
        // Visually indicate which button is highlighted.
        lv_obj_t *hl = g_perm_highlight_allow ? g_btn_allow : g_btn_deny;
        lv_obj_t *un = g_perm_highlight_allow ? g_btn_deny  : g_btn_allow;
        lv_obj_add_state(hl, LV_STATE_FOCUSED);
        lv_obj_clear_state(un, LV_STATE_FOCUSED);
    }
#else
    ESP_LOGD(TAG, "render mood=%s emoji=%s tokens=%.2f",
             PetState::mood_label(snap.mood),
             PetState::instance().current_emoji(),
             snap.meters.tokens);
#endif
}

void display_permission_show(const PermissionDisplayRequest &req) {
    g_perm_req = req;
    g_perm_active = true;
    g_perm_highlight_allow = true;
    PetState::instance().on_permission_pending();

#if TOKMAGOTCHI_HAS_LVGL
    std::string body = req.tool;
    if (!req.path.empty())    { body += ":\n" + req.path; }
    if (!req.command.empty()) { body += ":\n" + req.command; }
    lv_label_set_text(g_lbl_perm_body, body.c_str());
    lv_scr_load(g_scr_perm);
#endif
    ESP_LOGI(TAG, "permission prompt: %s %s", req.tool.c_str(),
             req.path.empty() ? req.command.c_str() : req.path.c_str());
}

void display_permission_clear() {
    g_perm_active = false;
#if TOKMAGOTCHI_HAS_LVGL
    lv_scr_load(g_scr_main);
#endif
}

bool display_permission_on_scroll(int delta) {
    if (!g_perm_active) return false;
    if (delta > 0) g_perm_highlight_allow = true;
    else if (delta < 0) g_perm_highlight_allow = false;
    return true;
}

bool display_permission_on_press() {
    if (!g_perm_active) return false;
    bool allow = g_perm_highlight_allow;
    std::string id = g_perm_req.id;
    display_permission_clear();
    PetState::instance().on_permission_resolved(allow);
    if (g_decision_cb) g_decision_cb(id, allow);
    return true;
}

void display_set_permission_decision_cb(PermissionDecisionCb cb) {
    g_decision_cb = cb;
}

} // namespace tokmagotchi
