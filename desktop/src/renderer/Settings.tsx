import React, { useState } from "react";
import { api } from "./api";
import type { DeviceInfo } from "@shared/types";

export function SettingsPanel({ device }: { device: DeviceInfo }) {
  const [name, setName] = useState(device.name.replace(/^tokmagotchi-?/i, "") || "Pixel");
  const [msg, setMsg] = useState<string>();

  const save = async () => {
    await api.setDeviceName(name);
    setMsg(`Name updated to ${name}`);
  };

  const install = async () => {
    const r = await api.installHook();
    setMsg(
      r.addedToSettings
        ? `Hook installed at ${r.hookPath} and registered in ${r.settingsPath}`
        : `Hook already installed at ${r.hookPath}`,
    );
  };

  const uninstall = async () => {
    await api.uninstallHook();
    setMsg("Hook removed.");
  };

  return (
    <>
      <section className="card">
        <h2>Device</h2>
        <div style={{ fontSize: 13, color: "#8f94a3" }}>
          {device.name} · {device.address}:{device.port}
        </div>

        <label style={{ display: "block", fontSize: 12, color: "#8f94a3", margin: "16px 0 4px" }}>
          Pet name
        </label>
        <input
          value={name}
          onChange={(e) => setName(e.target.value)}
          style={{
            background: "#0e1014",
            border: "1px solid #22242c",
            color: "#e8e8ee",
            padding: "8px 10px",
            borderRadius: 6,
            fontSize: 14,
            width: 240,
          }}
        />
        <div style={{ marginTop: 12 }}>
          <button className="btn primary" onClick={save}>Save</button>
        </div>
      </section>

      <section className="card">
        <h2>Claude Code hook</h2>
        <p style={{ color: "#8f94a3", fontSize: 13 }}>
          Installs <code>tokmagotchi-hook</code> into ~/.local/bin and registers it as a
          PreToolUse hook in ~/.claude/settings.json.
        </p>
        <div style={{ display: "flex", gap: 8 }}>
          <button className="btn primary" onClick={install}>Install hook</button>
          <button className="btn" onClick={uninstall}>Uninstall</button>
        </div>
      </section>

      {msg && (
        <section className="card">
          <div style={{ color: "#7ee787" }}>{msg}</div>
        </section>
      )}
    </>
  );
}
