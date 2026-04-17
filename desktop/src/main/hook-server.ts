import { EventEmitter } from "events";
import express, { Request, Response } from "express";
import { Server } from "http";
import { HOOK_SERVER_PORT, PERMISSION_TIMEOUT_MS } from "@shared/constants";
import type { PermissionRequest, PermissionResponse } from "@shared/types";

// Express server that accepts POST /permission from tokmagotchi-hook and
// blocks until the device (or timeout) resolves the request. Also exposes
// POST /feed-manual for testing and GET /status for hook-binary health checks.

type Resolver = (resp: PermissionResponse) => void;

export class HookServer extends EventEmitter {
  private app = express();
  private server?: Server;
  private pending = new Map<string, Resolver>();

  constructor(private timeoutMs = PERMISSION_TIMEOUT_MS) {
    super();
    this.app.use(express.json({ limit: "256kb" }));
    this.app.get("/status", (_req, res) => res.json({ ok: true }));
    this.app.post("/permission", this.handlePermission.bind(this));
  }

  start(): void {
    if (this.server) return;
    this.server = this.app.listen(HOOK_SERVER_PORT, "127.0.0.1", () => {
      console.log(`[hook-server] listening on 127.0.0.1:${HOOK_SERVER_PORT}`);
    });
  }

  stop(): void {
    this.server?.close();
    this.server = undefined;
    for (const [, resolver] of this.pending) {
      resolver({ id: "", decision: "deny", decidedBy: "timeout" });
    }
    this.pending.clear();
  }

  // Called by device-bridge when the device reports a decision.
  resolve(id: string, decision: "allow" | "deny", decidedBy: PermissionResponse["decidedBy"]) {
    const r = this.pending.get(id);
    if (!r) return;
    r({ id, decision, decidedBy });
    this.pending.delete(id);
  }

  private handlePermission(req: Request, res: Response) {
    const body = req.body as Partial<PermissionRequest>;
    if (!body?.id || !body.tool) {
      res.status(400).json({ error: "id+tool required" });
      return;
    }
    const perm: PermissionRequest = {
      id: body.id,
      tool: body.tool,
      path: body.path,
      command: body.command,
      timestamp: body.timestamp ?? new Date().toISOString(),
    };

    // Race resolver vs timeout.
    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      this.pending.delete(perm.id);
      res.json({ id: perm.id, decision: "deny", decidedBy: "timeout" } satisfies PermissionResponse);
    }, this.timeoutMs);

    this.pending.set(perm.id, (resp) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      res.json(resp);
    });

    this.emit("permission", perm);
  }
}
