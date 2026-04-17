import React, { useEffect, useState } from "react";
import { api } from "./api";
import { Dashboard } from "./Dashboard";
import { PairingScreen } from "./PairingScreen";
import { MediaGallery } from "./MediaGallery";
import { SettingsPanel } from "./Settings";
import type { DeviceInfo } from "@shared/types";

type Tab = "dashboard" | "media" | "settings";

export function App() {
  const [device, setDevice] = useState<DeviceInfo | undefined>(undefined);
  const [tab, setTab] = useState<Tab>("dashboard");

  useEffect(() => {
    api.currentDevice().then(setDevice);
    const offFound = api.onDeviceFound(setDevice);
    const offLost = api.onDeviceLost(() => setDevice(undefined));
    return () => {
      offFound();
      offLost();
    };
  }, []);

  if (!device) {
    return <PairingScreen onPaired={setDevice} />;
  }

  return (
    <div className="app">
      <header className="topbar">
        <div className="brand">🐾 Tokmagotchi — {device.name}</div>
        <div className="status connected">● Connected ({device.address})</div>
      </header>
      <main className="main">
        <div className="tabs">
          <button className={tab === "dashboard" ? "active" : ""} onClick={() => setTab("dashboard")}>Dashboard</button>
          <button className={tab === "media" ? "active" : ""} onClick={() => setTab("media")}>Media</button>
          <button className={tab === "settings" ? "active" : ""} onClick={() => setTab("settings")}>Settings</button>
        </div>
        {tab === "dashboard" && <Dashboard />}
        {tab === "media" && <MediaGallery />}
        {tab === "settings" && <SettingsPanel device={device} />}
      </main>
    </div>
  );
}
