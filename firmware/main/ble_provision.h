#pragma once

#include <functional>
#include <string>

namespace tokmagotchi {

struct WifiCreds {
    std::string ssid;
    std::string password;
    std::string pet_name;  // optional — desktop sends during pairing
};

// Start BLE advertising as "tokmagotchi-<mac>" and wait for the desktop app
// to write WiFi credentials + pet name. Invokes the callback on success.
// Call this only on first boot (or when re-provisioning is requested).
using ProvisionedCb = std::function<void(const WifiCreds &)>;
void ble_provision_start(ProvisionedCb cb);
void ble_provision_stop();

} // namespace tokmagotchi
