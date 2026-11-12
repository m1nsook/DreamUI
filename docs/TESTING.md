# DreamUI v0.1.0-alpha.3 — read-only engine probe and theme catalog

**Experimental test build — not a HUD recoloring mod.** It does not modify game memory, change colors or display DreamUI's main-menu tile.

## Installation

1. Make a backup of your modded game setup.
2. Keep the compatible ASI loader you already use. **Do not replace `version.dll`**.
3. Extract the release ZIP into the Cyberpunk 2077 directory, replacing the earlier DreamUI.asi and Default.dream.
4. Start Cyberpunk 2077, enter the main menu, and quit.
5. Open `dreamui/logs/DreamUI.log`.

## Expected alpha.3 log

    DreamUI v0.1.0-alpha.3 | read-only engine reconnaissance; no patches enabled
    Engine probe: read-only executable metadata, no in-memory scanning
    Executable file version: ... (file resource)
    Engine PE: x64=1 timestamp=... image_size=... rdata_scanned=... bytes
    Engine literals (diagnostic only): inkStyleResource=... inkStyleProperty=... inkWidget=...
    Engine status: no live Ink resource handles acquired; game memory untouched
    Validated theme dreamui.default version 0.1.0 | 20 color tokens
    Theme preflight completed: 1 valid theme(s)
    Control hub data model: title=DreamUI version=v0.1.0-alpha.3 installed=1 active=DreamUI Default
    Control hub native menu tile not installed yet

Version and literal-presence fields may vary by game build. If version information is unavailable, the version line may be absent. No color or menu changes should be visible.

## Safety / limitations

- The test build **still requires an existing compatible ASI loader**; an independent bundled loader is a later goal.
- The on-disk PE data is not evidence of a usable Ink resource handle.
- If an incompatible game or ASI loader crashes on startup, remove `bin/x64/plugins/DreamUI.asi` to disable the probe.
- You can report the log without attaching the copyrighted Cyberpunk executable.
- No third-party paid UI assets are included.

The new DreamUI main-menu tile idea is documented in [CONTROL_HUB.md](CONTROL_HUB.md) and will use only Cyberpunk's native Ink widgets once verified.
