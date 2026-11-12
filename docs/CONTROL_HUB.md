# DreamUI native control hub — design contract

The main menu screenshot supplied by the project owner marks an open rectangular area near the Phantom Liberty panel. This is the preferred location for a compact **DreamUI** tile.

## Intended appearance (text-only layout proposal)

    DREAMUI                            v0.1.0
    Active Dream: DreamUI Default
    [ Manage Dreams ]

When opened, a native menu could show:

    DreamUI / Manage Dreams
    Active         DreamUI Default
    Available      Frostline, DreamUI Default, ...
    Apply          Restore Vanilla
    Options        Color overrides / Backgrounds (when supported)

## Non-negotiable UI constraints

- Use Cyberpunk's existing **Ink** menu/widget primitives and focus/navigation system. No ImGui, browser overlay, externally drawn UI or unrelated custom widget engine.
- Do **not** modify or overwrite the Phantom Liberty panel. Use adjacent available menu space and inspect vanilla widget hierarchy before anchoring anything.
- Retain keyboard, mouse, gamepad, aspect ratio and UI-scaling compatibility.
- Do not capture input or save themes until the corresponding native API is proven safe.
- If the expected vanilla menu hierarchy is absent or altered by another mod, **hide** the DreamUI tile and log an explanation rather than patching unknown memory.
- Show the loaded DreamUI version and active .dream theme; users should be able to list, select, preview, disable and reset themes after engine integration.
- No premium addons, DRM or paid unlocks.

## Already implemented in alpha.3

The portable `DreamCatalog` and `ControlHubModel` build an ordered theme list, reject duplicate theme IDs, choose an active theme with a safe fallback, and expose a name/version suitable for the future tile. The ASI logs the model's current values.

**No main-menu visual tile is installed by alpha.3.** The necessary game Ink widget discovery, input wiring, safe mounting/unmounting and theme switching are separate later milestones. This spec is a design goal, not a claim of working in-game UI.

## Phasing

1. Validate read-only game binary metadata and capture runtime logs.
2. Discover safe native Ink controller hooks and resource access.
3. Apply/restore one existing UI style color.
4. Mount the compact menu tile using existing Ink widgets, implement native keyboard/gamepad focus.
5. Add interactive Dream selection, refresh and restore after end-to-end testing.
