import { EventEmitter } from "events";
import * as fs from "fs";
import * as path from "path";
import * as os from "os";
import chokidar, { FSWatcher } from "chokidar";
import type { FeedEvent } from "@shared/types";
import { TOKEN_FEED_LARGE_THRESHOLD } from "@shared/constants";

// Claude Code writes newline-delimited JSON logs under
//   ~/.claude/projects/<project-hash>/logs/*.jsonl
// We tail every .jsonl in that tree and emit a FeedEvent for each entry that
// has a `usage` field.

const CLAUDE_DIR = path.join(os.homedir(), ".claude", "projects");

interface ClaudeUsage {
  input_tokens?: number;
  output_tokens?: number;
  cache_creation_input_tokens?: number;
  cache_read_input_tokens?: number;
  model?: string;
}

interface ClaudeLogEntry {
  type?: string;
  timestamp?: string;
  usage?: ClaudeUsage;
  message?: { usage?: ClaudeUsage; model?: string };
}

export class LogWatcher extends EventEmitter {
  private watcher?: FSWatcher;
  private offsets = new Map<string, number>();

  start(): void {
    if (this.watcher) return;
    if (!fs.existsSync(CLAUDE_DIR)) {
      console.warn(`[log-watcher] ${CLAUDE_DIR} not found — is Claude Code installed?`);
      return;
    }
    this.watcher = chokidar.watch(path.join(CLAUDE_DIR, "**/*.jsonl"), {
      persistent: true,
      ignoreInitial: false,
      awaitWriteFinish: { stabilityThreshold: 200, pollInterval: 100 },
    });

    this.watcher.on("add", (file) => this.readNew(file));
    this.watcher.on("change", (file) => this.readNew(file));
    console.log(`[log-watcher] watching ${CLAUDE_DIR}`);
  }

  stop(): void {
    this.watcher?.close();
    this.watcher = undefined;
  }

  private async readNew(file: string) {
    try {
      const stat = await fs.promises.stat(file);
      const last = this.offsets.get(file) ?? 0;
      if (stat.size <= last) {
        // File was truncated or unchanged — reset if truncated.
        if (stat.size < last) this.offsets.set(file, 0);
        return;
      }
      const stream = fs.createReadStream(file, { start: last, end: stat.size });
      let tail = "";
      for await (const chunk of stream) {
        tail += chunk.toString("utf8");
      }
      this.offsets.set(file, stat.size);

      const lines = tail.split("\n").filter((l) => l.trim());
      for (const line of lines) {
        let entry: ClaudeLogEntry;
        try {
          entry = JSON.parse(line);
        } catch {
          continue;
        }
        const usage = entry.usage ?? entry.message?.usage;
        if (!usage) continue;
        const input = usage.input_tokens ?? 0;
        const output = usage.output_tokens ?? 0;
        const total = input + output;
        if (total <= 0) continue;
        const event: FeedEvent = {
          type: "token",
          size: total > TOKEN_FEED_LARGE_THRESHOLD ? "large" : "small",
          tokens: total,
          model: usage.model ?? entry.message?.model,
          timestamp: entry.timestamp ?? new Date().toISOString(),
        };
        this.emit("feed", event);
      }
    } catch (err) {
      console.error(`[log-watcher] read ${file} failed`, err);
    }
  }
}
