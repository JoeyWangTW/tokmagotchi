import React, { useEffect, useState } from "react";
import { api } from "./api";
import type { ActivityEntry, DeviceState } from "@shared/types";

function Meter({ icon, label, value }: { icon: string; label: string; value: number }) {
  const pct = Math.round(value * 100);
  return (
    <div className="meter">
      <div>{icon} {label}</div>
      <div className="bar"><div className="fill" style={{ width: `${pct}%` }} /></div>
      <div className="fg">{pct}%</div>
    </div>
  );
}

export function Dashboard() {
  const [state, setState] = useState<DeviceState | undefined>();
  const [activity, setActivity] = useState<ActivityEntry[]>([]);

  useEffect(() => {
    api.fetchDeviceState().then(setState);
    api.listActivity().then(setActivity);
    const offState = api.onDeviceState(setState);
    const offActivity = api.onActivity((entry) =>
      setActivity((prev) => [entry, ...prev].slice(0, 200)),
    );
    return () => {
      offState();
      offActivity();
    };
  }, []);

  return (
    <>
      <section className="card">
        <h2>Pet</h2>
        <div style={{ display: "flex", alignItems: "center", gap: 16, marginBottom: 16 }}>
          <div style={{ fontSize: 48 }}>{state?.emoji ?? "🐾"}</div>
          <div>
            <div style={{ fontSize: 18 }}>{state?.name ?? "—"}</div>
            <div style={{ color: "#8f94a3", fontSize: 13 }}>{state?.state ?? "unknown"}</div>
          </div>
        </div>
        <Meter icon="🧠" label="Tokens" value={state?.meters.tokens ?? 0} />
        <Meter icon="🎤" label="Voice"  value={state?.meters.voice  ?? 0} />
        <Meter icon="📸" label="Vision" value={state?.meters.vision ?? 0} />
      </section>

      <section className="card">
        <h2>Recent Activity</h2>
        {activity.length === 0 ? (
          <div style={{ color: "#6a6f7d", fontSize: 13 }}>No activity yet.</div>
        ) : (
          <ul className="activity">
            {activity.map((e) => (
              <li key={e.id}>
                <time>{new Date(e.timestamp).toLocaleTimeString()}</time>
                {e.label}
              </li>
            ))}
          </ul>
        )}
      </section>
    </>
  );
}
