import { EventEmitter } from "events";
import { Bonjour, Service } from "bonjour-service";
import {
  DEVICE_HTTP_PORT,
  MDNS_SERVICE_PROTO,
  MDNS_SERVICE_TYPE,
} from "@shared/constants";
import type {
  DeviceInfo,
  DeviceState,
  FeedEvent,
  MediaFile,
  PermissionRequest,
  PermissionResponse,
} from "@shared/types";

// Discovers the Watcher over mDNS and exposes a thin HTTP client for the
// endpoints the firmware serves on :8080. Re-announces itself when the
// device drops off the network, and emits `state` whenever a fresh state
// snapshot has been fetched.

export class DeviceBridge extends EventEmitter {
  private bonjour = new Bonjour();
  private browser?: ReturnType<Bonjour["find"]>;
  private device?: DeviceInfo;
  private pollTimer?: NodeJS.Timeout;

  start(): void {
    if (this.browser) return;
    this.browser = this.bonjour.find({
      type: MDNS_SERVICE_TYPE,
      protocol: MDNS_SERVICE_PROTO,
    });
    this.browser.on("up", (svc: Service) => {
      const addr = svc.addresses?.find((a) => !a.includes(":")) ?? svc.host;
      if (!addr) return;
      const info: DeviceInfo = {
        host: svc.host ?? "",
        address: addr,
        port: svc.port ?? DEVICE_HTTP_PORT,
        name: svc.name ?? "Tokmagotchi",
      };
      this.device = info;
      this.emit("found", info);
      this.startPolling();
    });
    this.browser.on("down", () => {
      if (this.device) this.emit("lost", this.device);
      this.device = undefined;
      this.stopPolling();
    });
    console.log(`[device-bridge] mDNS browse _${MDNS_SERVICE_TYPE}._${MDNS_SERVICE_PROTO}`);
  }

  stop(): void {
    this.browser?.stop();
    this.browser = undefined;
    this.stopPolling();
    this.bonjour.destroy();
  }

  current(): DeviceInfo | undefined {
    return this.device;
  }

  async pushFeed(event: FeedEvent): Promise<void> {
    if (!this.device) return;
    await this.post("/feed", {
      type: event.type,
      size: event.size,
      tokens: event.tokens,
    });
  }

  async pushPermission(req: PermissionRequest): Promise<PermissionResponse> {
    if (!this.device) {
      return { id: req.id, decision: "deny", decidedBy: "timeout" };
    }
    const body = await this.post("/permission", {
      id: req.id,
      tool: req.tool,
      path: req.path,
      command: req.command,
    });
    return body as PermissionResponse;
  }

  async fetchState(): Promise<DeviceState | undefined> {
    if (!this.device) return undefined;
    return (await this.get("/state")) as DeviceState;
  }

  async setName(name: string): Promise<void> {
    if (!this.device) return;
    await this.post("/name", { name });
  }

  async listMedia(): Promise<MediaFile[]> {
    if (!this.device) return [];
    const body = (await this.get("/media/list")) as { files: MediaFile[] };
    return body.files ?? [];
  }

  async downloadMedia(file: string): Promise<ArrayBuffer> {
    if (!this.device) throw new Error("no device");
    const url = `${this.baseUrl()}/media/${encodeURIComponent(file)}`;
    const r = await fetch(url);
    if (!r.ok) throw new Error(`download ${file} failed: ${r.status}`);
    return await r.arrayBuffer();
  }

  private baseUrl(): string {
    if (!this.device) throw new Error("no device");
    return `http://${this.device.address}:${this.device.port}`;
  }

  private async get(path: string): Promise<unknown> {
    const r = await fetch(`${this.baseUrl()}${path}`);
    if (!r.ok) throw new Error(`${path} failed: ${r.status}`);
    return await r.json();
  }

  private async post(path: string, body: unknown): Promise<unknown> {
    const r = await fetch(`${this.baseUrl()}${path}`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
    });
    if (!r.ok) throw new Error(`${path} failed: ${r.status}`);
    return await r.json();
  }

  private startPolling() {
    if (this.pollTimer) return;
    this.pollTimer = setInterval(async () => {
      try {
        const state = await this.fetchState();
        if (state) this.emit("state", state);
      } catch (err) {
        console.warn("[device-bridge] poll failed", err);
      }
    }, 3000);
  }

  private stopPolling() {
    if (this.pollTimer) clearInterval(this.pollTimer);
    this.pollTimer = undefined;
  }
}
