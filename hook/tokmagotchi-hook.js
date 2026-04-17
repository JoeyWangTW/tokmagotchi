#!/usr/bin/env node
// tokmagotchi-hook — Claude Code PreToolUse hook.
//
// Installed by the Tokmagotchi desktop app to ~/.local/bin, registered via
// ~/.claude/settings.json. Standalone copy lives here for manual install:
//
//   cp hook/tokmagotchi-hook.js ~/.local/bin/tokmagotchi-hook
//   chmod +x ~/.local/bin/tokmagotchi-hook
//
// Reads a PreToolUse JSON payload on stdin, forwards it to the local hook
// server at 127.0.0.1:27182/permission, and converts the device's allow /
// deny decision into a 0 / 1 exit code.
//
// NOTE: Keep this file in sync with the inline copy in
// desktop/src/main/installer.ts.

const http = require("http");

const HOOK_SERVER_PORT = 27182;

function readStdin() {
  return new Promise((resolve) => {
    let data = "";
    process.stdin.on("data", (c) => (data += c));
    process.stdin.on("end", () => resolve(data));
  });
}

function uuid() {
  return Math.random().toString(36).slice(2) + Date.now().toString(36);
}

(async () => {
  const raw = await readStdin();
  let payload = {};
  try { payload = JSON.parse(raw); } catch { /* allow empty */ }

  const body = JSON.stringify({
    id: payload.id || uuid(),
    tool: payload.tool_name || payload.tool || "unknown",
    path: payload.tool_input?.file_path || payload.path,
    command: payload.tool_input?.command || payload.command,
    timestamp: new Date().toISOString(),
  });

  const req = http.request({
    hostname: "127.0.0.1",
    port: HOOK_SERVER_PORT,
    path: "/permission",
    method: "POST",
    headers: {
      "content-type": "application/json",
      "content-length": Buffer.byteLength(body),
    },
  }, (res) => {
    let chunks = "";
    res.on("data", (c) => (chunks += c));
    res.on("end", () => {
      try {
        const resp = JSON.parse(chunks);
        process.exit(resp.decision === "allow" ? 0 : 1);
      } catch {
        process.exit(1);
      }
    });
  });
  req.on("error", () => process.exit(1));
  req.write(body);
  req.end();
})();
