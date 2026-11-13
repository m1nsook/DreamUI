# Automatic native resource-load trace (source-only experiment)

DreamUI is a standalone Cyberpunk 2077 ASI. It does not require CET,
RED4ext, Codeware, ArchiveXL, HUD Painter, or RedHotTools at runtime.

## Purpose

A Tab-triggered ResourceLoader cache snapshot happens **after** some game
resource work and cannot establish what was present during load. HUD
Painter 1.4.1 caches `inkStyleResource.styles` during its
`Resource/Loaded` callback, then restores cached styles before requesting
a refresh. Its installed frameworks provide that callback; DreamUI does not
depend on them.

This experiment samples DreamUI's independent, gated native
`ResourceSerializer::SchedulePostLoadJobs` hook and automatically writes
atomic counters to `dreamui/logs/DreamUI-load.log`. It does **not**
implement HUD Painter's resource ownership, in-game Inspector, refresh,
or interactive palette switching.

## Build and enable (test installation only)

This feature is **not in the public v0.1.0-alpha.8 ASI**.

1. Build the Windows ASI with `-DDREAMUI_EXPERIMENTAL_NATIVE_STAGE=ON`, or
   obtain the matching Windows CI test artifact; back up the current ASI.
2. Install `DreamUI.asi` at `bin/x64/plugins/DreamUI.asi`. Keep a valid
   `.dream` package in `dreamui/themes/`.
3. Before launching the game, create the empty marker
   `dreamui/dev/native-ink-preview.enable`.
4. **Do not create `dreamui/dev/native-ink-write.enable`.** Without
   the second marker the serializer hook observes only and does not call
   the native color writer.
5. Launch Cyberpunk normally. The new `dreamui/logs/DreamUI-load.log`
   is created on launch and updates automatically about once per second
   as counters change. No Tab press or `commands.txt` edit is required.

`DreamUI.log` still records startup/installation errors. The existing Tab
key and `DreamUI-probe.log` still work as an optional **current-state**
snapshot, not a load-event recorder. This change does not introduce an
in-game dev window.

## How to interpret the trace

- `native stage=inactive`: no experimental hook installed. A regular
  build, missing opt-in marker, or unsupported executable may all leave
  the stage inactive. Read `DreamUI.log` for startup failures.
- `serializer_callbacks`: hooked serializer entries observed after
  installation. If zero, no callback has been observed; no conclusions
  about styles can be drawn.
- `candidates`: strong-handle, RTTI-verified `inkStyleResource` objects.
- `target_resources`: matching exact
  `base\\gameplay\\gui\\common\\main_colors.inkstyle` path.
- `last_target_styles` and `peak_target_styles`: direct style counts
  read during read-only preflight. Peak is the greatest count seen
  during this session, not the full resource history.
- `owned_original_colors` and `source_styles`: number of validated
  HDRColor values and source styles retained as pointer-free data on the
  first successful preflight; not an owning resource or full style clone.
- `last_matched_properties`: validated matching HDRColor properties
  from the most recent target preflight.
- `preflight_ready`, `preflight_no_styles`, `preflight_missing`, and
  `preflight_rejected`: outcomes from the same strict native read-only
  validation as the source-only patcher.

This reports observations **only after the hook is installed**; it is not
an exhaustive resource-loading timeline. A zero count may reflect
ordering, resource lifetime, or validation failure. The worker thread
only reads atomic counters and never dereferences serializer objects.
The trace is truncated at startup and capped at 1024 changed snapshots
to avoid unbounded logs in long game sessions.

## Safety and current limitations

Although the color writer stays disabled in observe-only mode, **MinHook
still modifies executable code** to capture native callbacks. The game's
callback ABI, hook ordering, and resource lifecycle are not yet verified
in a running test build. A crash remains possible. Use a separate clean
test installation and never enable native writes on a normal playthrough.

A positive `preflight_ready` is not proof of a visible color change.
Nothing here acquires a persistent game resource handle, rehydrates a
consumed styles array, uses `Game.GetInkInspector()`, or refreshes UI.

Reference implementation research: HUD Painter 1.4.1
`r6/scripts/HUDPainter/HudPainterStorage.reds` (`OnStyleLoaded`,
`OnMainColorsReady`) and RedHotTools 1.3.0
`modules/ink/main.lua` (`Game.GetInkInspector()`,
`CollectInkLayers()`). These archives are **research inputs, not
DreamUI runtime dependencies**.
