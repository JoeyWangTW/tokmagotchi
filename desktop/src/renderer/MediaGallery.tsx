import React, { useEffect, useState } from "react";
import { api } from "./api";
import type { MediaFile } from "@shared/types";

export function MediaGallery() {
  const [files, setFiles] = useState<MediaFile[]>([]);
  const [err, setErr] = useState<string>();

  const refresh = async () => {
    try {
      setFiles(await api.listMedia());
      setErr(undefined);
    } catch (e) {
      setErr(String(e));
    }
  };

  useEffect(() => {
    refresh();
  }, []);

  return (
    <section className="card">
      <h2>Media on device SD</h2>
      <div style={{ marginBottom: 12 }}>
        <button className="btn" onClick={refresh}>Refresh</button>
      </div>
      {err && <div style={{ color: "#e96c6c" }}>{err}</div>}
      {files.length === 0 ? (
        <div style={{ color: "#6a6f7d", fontSize: 13 }}>No files yet.</div>
      ) : (
        <ul className="activity">
          {files.map((f) => (
            <li key={f.name}>
              {f.type === "audio" ? "🎤 " : f.type === "photo" ? "📸 " : "📄 "}
              {f.name} · {(f.size / 1024).toFixed(1)} KB
            </li>
          ))}
        </ul>
      )}
    </section>
  );
}
