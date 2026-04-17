import { app, BrowserWindow, ipcMain } from "electron";
import * as path from "path";
import { LogWatcher } from "./log-watcher";
import { HookServer } from "./hook-server";
import { DeviceBridge } from "./device-bridge";
import { BleProvisioner } from "./ble-provision";
import { installHook, uninstallHook } from "./installer";
import type { ActivityEntry, FeedEvent, PermissionRequest } from "@shared/types";

let win: BrowserWindow | null = null;
const logs = new LogWatcher();
const hook = new HookServer();
const bridge = new DeviceBridge();
const ble = new BleProvisioner();

const activity: ActivityEntry[] = [];
function pushActivity(e: Omit<ActivityEntry, "id">) {
  const entry: ActivityEntry = { ...e, id: `${Date.now()}-${Math.random()}` };
  activity.unshift(entry);
  if (activity.length > 200) activity.pop();
  win?.webContents.send("activity", entry);
}

function createWindow() {
  win = new BrowserWindow({
    width: 900,
    height: 640,
    webPreferences: {
      preload: path.join(__dirname, "../preload/index.js"),
      contextIsolation: true,
      sandbox: false,
    },
  });

  if (process.env.ELECTRON_RENDERER_URL) {
    win.loadURL(process.env.ELECTRON_RENDERER_URL);
  } else {
    win.loadFile(path.join(__dirname, "../renderer/index.html"));
  }
}

function wireServices() {
  logs.on("feed", async (event: FeedEvent) => {
    pushActivity({
      kind: "feed",
      timestamp: event.timestamp,
      label: `+${event.tokens ?? 0} tokens${event.model ? ` (${event.model})` : ""}`,
    });
    try {
      await bridge.pushFeed(event);
    } catch (err) {
      console.warn("[main] pushFeed failed", err);
    }
  });

  hook.on("permission", async (req: PermissionRequest) => {
    pushActivity({
      kind: "permission",
      timestamp: req.timestamp,
      label: `Permission pending: ${req.tool} ${req.path ?? req.command ?? ""}`,
    });
    try {
      const resp = await bridge.pushPermission(req);
      hook.resolve(resp.id, resp.decision, resp.decidedBy);
      pushActivity({
        kind: "permission",
        timestamp: new Date().toISOString(),
        label: `Permission ${resp.decision} (${resp.decidedBy}): ${req.tool}`,
      });
    } catch (err) {
      console.warn("[main] forward permission failed, auto-denying", err);
      hook.resolve(req.id, "deny", "timeout");
    }
  });

  bridge.on("found", (info) => {
    pushActivity({
      kind: "system",
      timestamp: new Date().toISOString(),
      label: `Device connected: ${info.name} @ ${info.address}`,
    });
    win?.webContents.send("device:found", info);
  });
  bridge.on("lost", (info) => {
    pushActivity({
      kind: "system",
      timestamp: new Date().toISOString(),
      label: `Device lost: ${info.name}`,
    });
    win?.webContents.send("device:lost", info);
  });
  bridge.on("state", (state) => {
    win?.webContents.send("device:state", state);
  });
}

function wireIpc() {
  ipcMain.handle("activity:list", () => activity);
  ipcMain.handle("device:current", () => bridge.current());
  ipcMain.handle("device:state", () => bridge.fetchState());
  ipcMain.handle("device:listMedia", () => bridge.listMedia());
  ipcMain.handle("device:setName", (_e, name: string) => bridge.setName(name));
  ipcMain.handle("ble:scan", () => ble.scan());
  ipcMain.handle("ble:provision", (_e, params) => ble.provision(params));
  ipcMain.handle("hook:install", () => installHook());
  ipcMain.handle("hook:uninstall", () => {
    uninstallHook();
    return true;
  });
}

app.whenReady().then(() => {
  wireServices();
  wireIpc();
  createWindow();
  logs.start();
  hook.start();
  bridge.start();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") app.quit();
});

app.on("before-quit", () => {
  logs.stop();
  hook.stop();
  bridge.stop();
});
