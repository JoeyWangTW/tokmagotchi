#pragma once

#include <string>

namespace tokmagotchi {

// Start the device HTTP server on DEVICE_HTTP_PORT.
// Registers: POST /feed, POST /permission, GET /state, GET /media/list, GET /media/<file>.
void http_server_start();
void http_server_stop();

// Called by display input when the user resolves a pending permission on-device.
void http_server_resolve_permission(const std::string &id, bool allow);

} // namespace tokmagotchi
