import * as fs from "fs";
import * as path from "path";
import * as os from "os";
import { HOOK_SERVER_PORT } from "@shared/constants";

// Installs the tokmagotchi-hook script into ~/.local/bin and registers it
// in ~/.claude/settings.json as a PreToolUse hook.
//
// The hook script is tiny (stdin → HTTP POST → stdout / exit code). We ship
// it as a Node script so the user doesn't need an extra toolchain.

const HOOK_SCRIPT = `#!/usr/bin/env node
// tokmagotchi-hook — installed by Tokmagotchi desktop.
// Reads a Claude Code PreToolUse payload on stdin, forwards it to the
// local hook server, and returns allow/deny via exit code.

const http = require("http");

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
    port: ${HOOK_SERVER_PORT},
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
`;

export interface InstallResult {
  hookPath: string;
  settingsPath: string;
  addedToSettings: boolean;
}

export function installHook(): InstallResult {
  const binDir = path.join(os.homedir(), ".local", "bin");
  fs.mkdirSync(binDir, { recursive: true });
  const hookPath = path.join(binDir, "tokmagotchi-hook");
  fs.writeFileSync(hookPath, HOOK_SCRIPT, { mode: 0o755 });

  const settingsPath = path.join(os.homedir(), ".claude", "settings.json");
  fs.mkdirSync(path.dirname(settingsPath), { recursive: true });

  let settings: Record<string, unknown> = {};
  if (fs.existsSync(settingsPath)) {
    try {
      settings = JSON.parse(fs.readFileSync(settingsPath, "utf8"));
    } catch {
      // Corrupt settings — don't clobber, bail out.
      return { hookPath, settingsPath, addedToSettings: false };
    }
  }
  const hooks = (settings.hooks ??= {}) as Record<string, unknown>;
  const pre = (hooks.PreToolUse ??= []) as Array<Record<string, unknown>>;

  const already = pre.some((h) => {
    const sub = h.hooks as Array<Record<string, unknown>> | undefined;
    return sub?.some((x) => typeof x.command === "string" && x.command === hookPath);
  });
  if (!already) {
    pre.push({
      matcher: ".*",
      hooks: [{ type: "command", command: hookPath }],
    });
    fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
  }

  return { hookPath, settingsPath, addedToSettings: !already };
}

export function uninstallHook(): void {
  const hookPath = path.join(os.homedir(), ".local", "bin", "tokmagotchi-hook");
  if (fs.existsSync(hookPath)) fs.unlinkSync(hookPath);

  const settingsPath = path.join(os.homedir(), ".claude", "settings.json");
  if (!fs.existsSync(settingsPath)) return;
  try {
    const settings = JSON.parse(fs.readFileSync(settingsPath, "utf8")) as {
      hooks?: { PreToolUse?: Array<{ hooks?: Array<{ command?: string }> }> };
    };
    const pre = settings.hooks?.PreToolUse;
    if (!pre) return;
    settings.hooks!.PreToolUse = pre.filter(
      (h) => !h.hooks?.some((x) => x.command === hookPath),
    );
    fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
  } catch {
    /* ignore */
  }
}
