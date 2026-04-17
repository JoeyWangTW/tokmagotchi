#pragma once

namespace tokmagotchi {

// Start mDNS and advertise _tokmagotchi._tcp on DEVICE_HTTP_PORT.
// Hostname: tokmagotchi-<pet name>. Call after WiFi is connected.
void mdns_service_start();
void mdns_service_update_name();

} // namespace tokmagotchi
