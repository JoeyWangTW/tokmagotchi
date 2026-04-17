# tokmagotchi-hook

The Claude Code `PreToolUse` hook script. When a user asks Claude to use a tool (Write, Bash, Edit, etc.) this script intercepts, forwards the request to the Tokmagotchi desktop app's local server on port 27182, and returns an allow/deny decision as its exit code.

The Tokmagotchi desktop app's Settings panel installs this automatically. To install manually:

```bash
cp hook/tokmagotchi-hook.js ~/.local/bin/tokmagotchi-hook
chmod +x ~/.local/bin/tokmagotchi-hook
```

Then register it in `~/.claude/settings.json`:

```json
{
  "hooks": {
    "PreToolUse": [{
      "matcher": ".*",
      "hooks": [{ "type": "command", "command": "/home/<you>/.local/bin/tokmagotchi-hook" }]
    }]
  }
}
```

If the desktop app isn't running, the hook call will fail fast and Claude Code will treat it as a denial. That's the intended fail-safe: no pet, no tool.
