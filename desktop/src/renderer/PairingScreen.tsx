import React, { useState } from "react";
import { api } from "./api";
import type { DeviceInfo } from "@shared/types";

export function PairingScreen({ onPaired }: { onPaired: (d: DeviceInfo) => void }) {
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const [petName, setPetName] = useState("Pixel");
  const [pop, setPop] = useState("tokmagotchi-pop");
  const [status, setStatus] = useState<string>("Waiting for a device on the local network…");
  const [scanning, setScanning] = useState(false);
  const [provisioning, setProvisioning] = useState(false);

  // Pair flow when the device is already on the network (re-pair / manual discovery).
  // The main process auto-subscribes to mDNS; this page just shows progress.
  React.useEffect(() => {
    const off = api.onDeviceFound(onPaired);
    return () => off();
  }, [onPaired]);

  const scan = async () => {
    setScanning(true);
    setStatus("Scanning BLE for tokmagotchi-* devices…");
    const found = await api.bleScan();
    setScanning(false);
    setStatus(found.length ? `BLE found: ${found.join(", ")}` : "No BLE devices found.");
  };

  const provision = async () => {
    setProvisioning(true);
    setStatus("Provisioning over BLE (esp-prov)…");
    try {
      await api.bleProvision({ ssid, password, petName, proofOfPossession: pop });
      await api.setDeviceName(petName);
      setStatus("Device provisioned. Waiting for mDNS announcement…");
    } catch (err) {
      setStatus(`Provisioning failed: ${String(err)}`);
    } finally {
      setProvisioning(false);
    }
  };

  return (
    <div className="pair">
      <div className="panel">
        <h1>Pair your Tokmagotchi</h1>
        <p style={{ color: "#8f94a3" }}>{status}</p>

        <label>Pet name</label>
        <input value={petName} onChange={(e) => setPetName(e.target.value)} />

        <label>WiFi SSID</label>
        <input value={ssid} onChange={(e) => setSsid(e.target.value)} />

        <label>WiFi password</label>
        <input type="password" value={password} onChange={(e) => setPassword(e.target.value)} />

        <label>Proof of possession</label>
        <input value={pop} onChange={(e) => setPop(e.target.value)} />

        <div style={{ display: "flex", gap: 8, marginTop: 16 }}>
          <button className="btn" onClick={scan} disabled={scanning}>
            {scanning ? "Scanning…" : "Scan BLE"}
          </button>
          <button
            className="btn primary"
            onClick={provision}
            disabled={provisioning || !ssid || !petName}
          >
            {provisioning ? "Provisioning…" : "Pair over BLE"}
          </button>
        </div>
      </div>
    </div>
  );
}
