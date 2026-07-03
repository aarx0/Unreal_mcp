# MCP Automation Bridge

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.0--5.8-orange)](https://www.unrealengine.com/)
[![GitHub](https://img.shields.io/badge/GitHub-aarx0/Unreal__mcp-blueviolet?logo=github)](https://github.com/aarx0/Unreal_mcp)

An Unreal Engine editor plugin that lets AI assistants (Claude Code, Cursor, VS Code, etc.)
control Unreal Engine through the Model Context Protocol (MCP). The plugin **is** the MCP
server — a native, pull-only Streamable HTTP endpoint inside the editor. No Node.js, no
external bridge process.

> This is the native-only [aarx0 fork](https://github.com/aarx0/Unreal_mcp). The upstream
> project ([ChiR24/Unreal_mcp](https://github.com/ChiR24/Unreal_mcp)) pairs the plugin with a
> TypeScript server; this fork deleted that layer entirely.

---

## Features

| Category | Capabilities |
|----------|-------------|
| **Asset Management** | Browse, import, duplicate, rename, delete, save assets; create materials |
| **Actor Control** | Spawn, delete, transform, physics, tags, components |
| **Editor Control** | PIE sessions, camera, viewport, screenshots, bookmarks |
| **Level Management** | Load/save levels, streaming, lighting |
| **Animation & Physics** | Animation BPs, state machines, montages, ragdolls, vehicles, constraints |
| **Visual Effects** | Niagara particles, GPU simulations, procedural effects |
| **Sequencer** | Cinematics, timeline control, camera animations |
| **Graph Editing** | Blueprint, Niagara, Material, Behavior Tree graphs (+ auto-layout `arrange_graph`) |
| **Audio** | Sound cues, audio components, MetaSounds |
| **UI** | Widget Blueprints, Common UI authoring, focus/nav introspection & drive |
| **System** | Console commands, UBT, Live Coding, tests, logs, project settings, Python execution |

**200+ automation actions** across 22 canonical MCP tools. `manage_tools` can enable/disable
tools at runtime, scoped to your MCP session — other connected clients are unaffected.

---

## Requirements

- **Unreal Engine**: 5.0 - 5.8 (5.8 preview validated; developed against 5.7)
- **Platforms**: Win64, Mac, Linux

---

## Installation

### Method 1: Copy to Project

1. Copy the `McpAutomationBridge` folder to your project's `Plugins/` directory:
   ```
   YourProject/Plugins/McpAutomationBridge/
   ```

2. Regenerate project files:
   - Right-click `.uproject` → "Generate Visual Studio project files"
   - Or run: `GenerateProjectFiles.bat`

3. Open your project in Unreal Editor

4. Enable required plugins in **Edit → Plugins**:

<details>
<summary><b>Core Plugins (Required)</b></summary>

   - ✅ MCP Automation Bridge
   - ✅ Editor Scripting Utilities
   - ✅ Niagara

</details>

<details>
<summary><b>Optional Plugins (Auto-enabled)</b></summary>

   - ✅ Level Sequence Editor (for `manage_sequence`)
   - ✅ Control Rig (for `animation_physics`)
   - ✅ GeometryScripting (for `manage_geometry`)
   - ✅ Behavior Tree Editor (for `manage_ai` Behavior Trees)
   - ✅ Niagara Editor (for Niagara authoring)
   - ✅ Gameplay Abilities (for `manage_gas`)
   - ✅ MetaSound (for `manage_audio` MetaSounds)
   - ✅ StateTree (for `manage_ai` State Trees)
   - ✅ Enhanced Input (for `manage_networking` input mappings)
   - ✅ Environment Query Editor (for AI/EQS)
   - ✅ Smart Objects (for AI smart objects)
   - ✅ Chaos Cloth (for cloth simulation)
   - ✅ Interchange (for asset import/export)
   - ✅ Data Validation (for data validation)
   - ✅ Procedural Mesh Component (for procedural geometry)
   - ✅ OnlineSubsystem (for sessions/networking)
   - ✅ OnlineSubsystemUtils (for sessions/networking)

</details>

   > 💡 Optional plugins are auto-enabled by the MCP Automation Bridge plugin. Only the core plugins require manual verification.

5. Restart the editor

### Method 2: Add in Editor

1. Open Unreal Editor → **Edit → Plugins**
2. Click **"Add"** button
3. Browse to and select the `McpAutomationBridge` folder
4. Enable the plugin and restart

---

## Quick Start

1. Enable the server in the project's `Config/DefaultGame.ini` (or via
   **Edit → Project Settings → Plugins → MCP Automation Bridge** — the settings class is
   `defaultconfig`, so values persist there, not in `Saved/Config/`):
   ```ini
   [/Script/McpAutomationBridge.McpAutomationBridgeSettings]
   bEnableNativeMCP=True
   NativeMCPPort=3000
   ```
2. Restart the editor. The status bar shows the endpoint when it's live.
3. Point your AI client at Streamable HTTP `http://127.0.0.1:3000/mcp`.

**Claude Code:**
```bash
claude mcp add unreal-engine --transport http http://127.0.0.1:3000/mcp
```

**Cursor** (`.cursor/mcp.json`):
```json
{
  "mcpServers": {
    "unreal-engine": {
      "url": "http://127.0.0.1:3000/mcp"
    }
  }
}
```

Example prompts:
- "List all assets in /Game/Characters"
- "Spawn a point light at (100, 200, 300)"
- "Create a new material called M_Glow"
- "Take a screenshot of the current viewport"

Tools only work while the editor is open with the plugin running. A tool-schema change reaches
the client only on a fresh MCP handshake — restart the client after rebuilding the plugin.

---

## Plugin Settings

Configure in **Edit → Project Settings → Plugins → MCP Automation Bridge** (stored in
`Config/DefaultGame.ini`):

- **Enable Native MCP**: turn the server on (default: off)
- **Native MCP Port**: HTTP port (default: 3000)
- **Listen Host**: bind address (default: 127.0.0.1)
- **Allow Non-Loopback**: enable LAN access (security consideration)
- **Require Capability Token**: enforce `X-MCP-Capability-Token` authentication
- **Load All Tools on Start**: all 22 canonical tools at startup vs. the core set (default: on)
- **Native MCP Instructions**: custom instructions served to AI clients

---

## Security

- **Loopback-only binding** by default (127.0.0.1)
- **Capability token authentication** on the native transport (enable in Project Settings)
- **Command validation** blocks dangerous console commands
- **Path sanitization** — blocks directory traversal in file operations
- **Python execution security** — 1 MB code limit, symlink resolution, temp file scope guard cleanup

---

## Troubleshooting

### Plugin Failed to Load

If you see *"Plugin 'McpAutomationBridge' failed to load"* on first open:
1. Close Unreal Editor
2. Reopen the project
3. The plugin should load correctly

This is a known UE behavior when plugins are rebuilt on first load.

### Connection Refused

1. Verify the plugin is enabled in **Edit → Plugins**
2. Verify `bEnableNativeMCP=True` is in `Config/DefaultGame.ini` (values written to
   `Saved/Config/.../Game.ini` are pruned by the editor and silently disable the bridge)
3. Check the configured port is not blocked by firewall

### Build Errors

The plugin uses `PCHUsageMode.NoPCHs` to prevent memory issues during compilation. If you encounter build errors:
1. Close Visual Studio
2. Delete `Intermediate/`, `Binaries/`, `Saved/` folders
3. Regenerate project files
4. Rebuild

---

## Documentation

- **Repo overview**: [root README](../../README.md)
- **Add or fix an action**: [docs/extending-the-bridge.md](../../docs/extending-the-bridge.md)
- **Action → handler lookup**: [docs/handler-mapping.md](../../docs/handler-mapping.md)
- **Safe asset saving**: [docs/safe-asset-saving.md](docs/safe-asset-saving.md)
- **Changelog**: [root CHANGELOG.md](../../CHANGELOG.md)

---

## License

MIT License - See [LICENSE](LICENSE) for details.
