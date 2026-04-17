export type FeedType = "token" | "voice" | "vision";
export type FeedSize = "small" | "large";

export interface FeedEvent {
  type: FeedType;
  size?: FeedSize;
  tokens?: number;
  model?: string;
  timestamp: string;
}

export interface PermissionRequest {
  id: string;
  tool: string;
  path?: string;
  command?: string;
  timestamp: string;
}

export interface PermissionResponse {
  id: string;
  decision: "allow" | "deny";
  decidedBy: "device" | "timeout" | "desktop";
}

export interface DeviceMeters {
  tokens: number;
  voice: number;
  vision: number;
}

export interface DeviceState {
  name: string;
  state: string;
  emoji: string;
  meters: DeviceMeters;
  lastFeed?: {
    tokens?: number;
    voice?: number;
    vision?: number;
  };
}

export interface DeviceInfo {
  host: string;         // mDNS hostname (e.g. tokmagotchi-pixel.local)
  address: string;      // resolved IP
  port: number;
  name: string;         // advertised instance name
}

export interface MediaFile {
  name: string;
  size: number;
  type: "audio" | "photo" | "other";
}

export interface ActivityEntry {
  id: string;
  timestamp: string;
  kind: "feed" | "permission" | "gesture" | "system";
  label: string;
}
