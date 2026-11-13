# DreamUI v0.1.0-alpha.9 — standalone native Ink loading diagnostics

**Experimental prerelease. No in-game recoloring or developer window yet.**
DreamUI is a standalone `DreamUI.asi` and `.dream` theme loader; it
requires an ASI loader but **does not require** CET, RED4ext, ArchiveXL,
Codeware, HUD Painter, or RedHotTools.

## Improvements since alpha.8

- The release ASI now **includes** the guarded native Ink load-stage
  observer, which was previously available only in Windows development
  artifacts. It remains off unless you explicitly opt in.
- A validated `.dream` palette is loaded at startup. The observer can
  record when REDengine's native serializer sees
  `base\\gameplay\\gui\\common\\main_colors.inkstyle`, before
  its direct styles disappear from the later ResourceLoader cache.
- `dreamui/logs/DreamUI-load.log` is created automatically per launch.
  When the observer is enabled, it records *changed* hook counters and
  preflight results about once per second, up to 1024 updates.
- The existing foreground **Tab** snapshot remains available, and
  `DreamUI-probe.log` still records each distinct press.
- The release explicitly compiles **without native color writing or
  native Clone calls**. The `native-ink-write.enable` and
  `native-ink-clone.enable` marker files cannot enable those features
  in this alpha.9 binary. The corresponding experiments remain
  developer-source-only.

## Install

1. Back up your previous `DreamUI.asi` and any existing logs.
2. Extract the ZIP into the **Cyberpunk 2077 game root**. The expected
   result is `bin/x64/plugins/DreamUI.asi` and
   `dreamui/themes/Default.dream`.
3. Keep the ASI loader provided by your Cyberpunk / Steam REDmod
   installation; do not replace or remove it.
4. Normally just launch the game. No marker file is required to load
   and validate themes or to use the Tab probe.

## Optional: observe Ink resource loading

To inspect actual serializer-stage resource loading on a **separate
clean test installation**:

1. Close the game.
2. Create an empty file:
   `Cyberpunk 2077/dreamui/dev/native-ink-preview.enable`
3. Launch the game. Do not press Tab to start the loading trace:
   `dreamui/logs/DreamUI-load.log` is written automatically.
4. To capture a current-state cache comparison, press **Tab** once
   at the main menu or in gameplay. That adds an entry to
   `dreamui/logs/DreamUI-probe.log`.
5. When reporting a problem, share `DreamUI.log`,
   `DreamUI-load.log` and `DreamUI-probe.log`.

**Important:** the opt-in observer installs a native function hook,
which modifies executable instructions even when it does not write
colors. It is restricted to a known Cyberpunk executable fingerprint
and can still crash the game. Delete the preview marker while the game
is closed to disable it for the next launch. The marker is only read
at startup.

Do **not** add `native-ink-write.enable` or
`native-ink-clone.enable` during alpha.9 testing. This release
explicitly locks both capabilities out, regardless of markers. A
developer rebuilding with different CMake options would have a
different, higher-risk test binary.

## How to read the logs

- `DreamUI.log`: startup, theme validation and native-stage
  activation or rejection; this log can append across launches.
- `DreamUI-load.log`: new per-session automatic load telemetry.
  `native stage=inactive` means no active native hook (for example,
  preview marker absent or unsupported executable). A positive
  `preflight_ready` means native properties were validated at load,
  **not** that a color was applied.
- `DreamUI-probe.log`: new per-session **on-demand Tab** cache
  snapshots. Later `styles=0` does not contradict a preceding load
  trace finding populated styles.
- `DreamUI-dev.log`: optional file-command responses. The
  `commands.txt` inbox is **not** an in-game developer console.

The owner's earlier clean-game experimental trace showed ten direct
main-colors styles and three validated HDRColor targets at the
serializer stage. **Alpha.9 itself still needs to be verified
in-game** as a distributed release artifact.

## Still unfinished

Interactive native Ink recoloring, an engine-owned persistent style
resource, proper game-driven refresh/restoration, a native DX12
developer window, and a game menu tile. No release note or successful
CI build should be treated as proof of visible HUD modification.

Reference: [Automatic load trace](AUTOMATIC_LOAD_TRACE.md),
[Native serializer stage](NATIVE_SERIALIZER_STAGE.md), and
[Developer handoff](DEVELOPMENT_HANDOFF.md).
