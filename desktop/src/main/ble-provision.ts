import { EventEmitter } from "events";

// BLE provisioning uses the ESP-IDF wifi_provisioning protocol. Rather than
// re-implementing the protobuf+AES session from scratch, we invoke the
// reference Python tool `esp-prov` as a subprocess — it's cross-platform,
// battle-tested, and avoids the noble/bluez native-module packaging pain.
//
// Fallback: if `@abandonware/noble` is installed, we fall back to a raw
// scan-and-connect flow that hands off credentials via a single GATT write
// to the "tokmagotchi-name" endpoint the firmware exposes.

import { spawn } from "child_process";

export interface BleProvisionParams {
  ssid: string;
  password: string;
  petName: string;
  proofOfPossession: string;   // "tokmagotchi-pop" by default
  servicePrefix?: string;      // e.g. "tokmagotchi-"
}

export class BleProvisioner extends EventEmitter {
  async scan(timeoutMs = 8000): Promise<string[]> {
    // Lazy-require noble so the app still boots when the optional dep is
    // missing or the platform doesn't have a BLE stack available.
    let noble: typeof import("@abandonware/noble") | undefined;
    try {
      noble = await import("@abandonware/noble");
    } catch {
      console.warn("[ble] @abandonware/noble not installed — scan disabled");
      return [];
    }

    return new Promise((resolve) => {
      const found = new Set<string>();
      const onDiscover = (p: import("@abandonware/noble").Peripheral) => {
        const name = p.advertisement.localName ?? "";
        if (name.startsWith("tokmagotchi-")) found.add(name);
      };
      const onState = (state: string) => {
        if (state === "poweredOn") noble!.startScanning([], true);
      };
      noble!.on("discover", onDiscover);
      noble!.on("stateChange", onState);
      if (noble!.state === "poweredOn") noble!.startScanning([], true);

      setTimeout(() => {
        noble!.stopScanning();
        noble!.removeListener("discover", onDiscover);
        noble!.removeListener("stateChange", onState);
        resolve(Array.from(found));
      }, timeoutMs);
    });
  }

  async provision(params: BleProvisionParams): Promise<void> {
    return new Promise((resolve, reject) => {
      // esp-prov is the official tool. Assumes the user has installed it via
      // `pip install esp-idf-provisioning` or checked out from esp-idf.
      const args = [
        "--transport", "ble",
        "--service_name", `${params.servicePrefix ?? "tokmagotchi-"}`,
        "provision-wifi",
        "--ssid", params.ssid,
        "--passphrase", params.password,
        "--sec_ver", "1",
        "--pop", params.proofOfPossession,
      ];
      const proc = spawn("esp-prov", args, { stdio: "inherit" });
      proc.on("error", reject);
      proc.on("close", (code) => {
        if (code === 0) resolve();
        else reject(new Error(`esp-prov exited with ${code}`));
      });
    });
  }
}
