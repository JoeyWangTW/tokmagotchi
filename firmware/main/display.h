#pragma once

#include <string>

namespace tokmagotchi {

struct PermissionDisplayRequest {
    std::string id;
    std::string tool;
    std::string path;
    std::string command;
};

// Initialise LVGL, the panel driver and the pet UI.
// Call once from app_main after PSRAM + SPIFFS are up.
void display_init();

// Main UI tick — redraws sprite + emoji + meters from PetState snapshot.
// Call from the LVGL task or a 250ms timer.
void display_render();

// Swap into/out of permission UI mode. While a permission is pending the
// pet sprite is replaced with the prompt view and scroll wheel drives the
// highlight. Call display_permission_clear() after a decision is made.
void display_permission_show(const PermissionDisplayRequest &req);
void display_permission_clear();

// User input hooks — called by the input subsystem on scroll / press events.
// Returns true when the event was consumed by the permission UI.
bool display_permission_on_scroll(int delta);
bool display_permission_on_press();

// Decision callback set by http_server / app_main so the device can unblock
// a pending permission POST response.
using PermissionDecisionCb = void (*)(const std::string &id, bool allow);
void display_set_permission_decision_cb(PermissionDecisionCb cb);

} // namespace tokmagotchi
