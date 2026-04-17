import { contextBridge, ipcRenderer } from "electron";
import type {
  ActivityEntry,
  DeviceInfo,
  DeviceState,
  MediaFile,
} from "@shared/types";

const api = {
  // One-shots.
  listActivity: (): Promise<ActivityEntry[]> => ipcRenderer.invoke("activity:list"),
  currentDevice: (): Promise<DeviceInfo | undefined> => ipcRenderer.invoke("device:current"),
  fetchDeviceState: (): Promise<DeviceState | undefined> => ipcRenderer.invoke("device:state"),
  listMedia: (): Promise<MediaFile[]> => ipcRenderer.invoke("device:listMedia"),
  setDeviceName: (name: string): Promise<void> => ipcRenderer.invoke("device:setName", name),

  // BLE / pairing.
  bleScan: (): Promise<string[]> => ipcRenderer.invoke("ble:scan"),
  bleProvision: (params: {
    ssid: string;
    password: string;
    petName: string;
    proofOfPossession: string;
  }): Promise<void> => ipcRenderer.invoke("ble:provision", params),

  // Hook installer.
  installHook: (): Promise<{ hookPath: string; settingsPath: string; addedToSettings: boolean }> =>
    ipcRenderer.invoke("hook:install"),
  uninstallHook: (): Promise<boolean> => ipcRenderer.invoke("hook:uninstall"),

  // Subscriptions.
  onActivity: (cb: (entry: ActivityEntry) => void) => {
    const handler = (_: unknown, entry: ActivityEntry) => cb(entry);
    ipcRenderer.on("activity", handler);
    return () => ipcRenderer.removeListener("activity", handler);
  },
  onDeviceState: (cb: (state: DeviceState) => void) => {
    const handler = (_: unknown, state: DeviceState) => cb(state);
    ipcRenderer.on("device:state", handler);
    return () => ipcRenderer.removeListener("device:state", handler);
  },
  onDeviceFound: (cb: (info: DeviceInfo) => void) => {
    const handler = (_: unknown, info: DeviceInfo) => cb(info);
    ipcRenderer.on("device:found", handler);
    return () => ipcRenderer.removeListener("device:found", handler);
  },
  onDeviceLost: (cb: (info: DeviceInfo) => void) => {
    const handler = (_: unknown, info: DeviceInfo) => cb(info);
    ipcRenderer.on("device:lost", handler);
    return () => ipcRenderer.removeListener("device:lost", handler);
  },
};

contextBridge.exposeInMainWorld("tokmagotchi", api);

export type TokmagotchiApi = typeof api;
