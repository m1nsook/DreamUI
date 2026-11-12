# What HUD Painter actually does — source-level reference (2026-11-12)

This is an architectural analysis of three locally supplied, third-party releases:
- HUD Painter 1.4.1 (2026-10-26), redscript package + REDengine archive
- RedFileSystem 0.16.0 (2026-10-25)
- RedData 0.11.1 (2026-10-25)

The archives were inspected as data only. Their proprietary files, compiled libraries, scripts, textures and assets are NOT checked into DreamUI or redistributed. All DreamUI implementations must be independently authored.

## Actual source flow

### 1. Resource-loaded callback, not cache polling

HUD Painter's `r6/scripts/HUDPainter/HudPainterStorage.reds`, lines 15–25, registers `Resource/Loaded` callbacks for:

- `base\gameplay\gui\common\main_colors.inkstyle`
- `base\gameplay\gui\common\themes\main_colors_johnny.inkstyle`

Lines 233–265 receive a `ResourceEvent`, cast `GetResource()` to `inkStyleResource` and cache the existing `resource.styles` arrays BEFORE performing color updates (when a non-default preset is active).

Its second path, lines 267–304, calls `GameInstance.GetResourceDepot().LoadResource` for both files, registers resource-token completion callbacks, restores the cached default styles before changing selected colors, and calls `resource.RefreshResource()`. This is a lifecycle-aware, event-driven process, not an arbitrary fixed-time scan of ResourceLoader internals.

### 2. Exact property-edit operation

In `HudPainterStorage.reds`, lines 306–340, the tool checks that `resource.styles` is nonempty. It processes entries of the FIRST `inkStyle`, preserving unchanged `inkStyleProperty` entries. When a `propertyPath` matches a preset color, it creates an `HDRColor` value and uses `ToVariant` to put a correctly typed value into that property. It then replaces the target style's property array. The explicit `RefreshResource()` call occurs in its runtime preset-switching path.

**Crucial:** The source itself refuses to patch when the styles array is empty. Our alpha.8 result `styles=0, themes=10, overrides=2` does NOT verify a color value, regardless of whether the token-cache path matched. Do not infer from these counts alone that a particular `inkStyleTheme` reference contains PanelWhite.

### 3. Widgets, menus, minimap, maps are separate integrations

`PatchesToBindProperties.reds` registers a resource-load callback for the main style file. It rewrites `MiniMapStyle` property **references** such as `MiniMapStyle.RoadColor` to point at palette tokens. Other patches bind pre-existing widget color properties using `SetStyle` and `BindProperty`, inside controller initialization callbacks. These are native Ink operations, not CSS.

`MenuIntegration.reds` registers `Resource/Ready` for `base\gameplay\gui\fullscreen\menu.inkmenu` and `base\gameplay\gui\fullscreen\main_menu\pregame_menu.inkmenu`, modifies `inkMenuResource` menus/scenarios and hooks existing menu controllers to expose HUD Painter's own `.inkwidget` screen. DreamUI's planned tile near Phantom Liberty needs its **own** verified native Ink controller integration and layout checks; copying this archive/widget would be improper.

`PatchesForWorldMap.reds` watches `Resource/Ready` for world-map environment/material resources and modifies material/environment parameters. A world-map marker recolor is NOT necessarily covered by the global style palette, so don't promise it via a single Ink style property.

### 4. RedFileSystem, RedData, and other dependencies

`HudPainterStorage.reds` imports `RedFileSystem.*` and `RedData.Json.*` to store, list, parse, write and delete JSON presets (RedFileSystem's `FileSystem.GetStorage` and RedData's `ToJson`/`FromJson` APIs). Their bundled `red4ext/plugins/*/*.dll` files are runtime native plugins, not an embedded HUD Painter engine hook.

HUD Painter's current Nexus requirements include **RED4ext, Codeware, redscript, ArchiveXL, RedFileSystem and RedData**, with Mod Settings optional. It can use resource callbacks provided by this installed modding stack. DreamUI's single standalone ASI must NOT treat those redscript functions as calls it can make without the frameworks. Study behavior and native types, but reimplement only the necessary native lifecycle/event/loader bridge independently.

Reference: https://www.nexusmods.com/cyberpunk2077/mods/14935?tab=requirements

### 5. The previous proof target was poorly selected

The CDPR modding resource specifically labels **MainColors.PanelWhite** as **W.I.P.**, without a documented visual use. **MainColors.Red** controls main/launch/pause menus and much primary text; **MainColors.PanelBlue** controls secondary text/icons; **MainColors.Grey** controls various subdued labels; **MainColors.ActiveYellow** controls selected/highlighted UI elements.

Reference: https://github.com/CDPR-Modding-Documentation/Cyberpunk-Modding-Docs/blob/main/modding-guides/hud-painter-resource.md

The shipped HUD Painter DEFAULT preset includes 92 main-theme entries and 10 Johnny-theme entries. Many native HDRColor channels exceed 1.0; #RRGGBB is fine for user-friendly SDR themes, but an engine-accurate patch/restore implementation must preserve full HDR float values (including alpha) and the Variant type.

## DreamUI engineering decision

**Stop publishing another diagnostic prerelease simply to retry a different color key or adjust startup timings.** The Tab probe remains optional, but it has given us the needed evidence.

The next milestone is native resource lifecycle integration in the standalone ASI:
1. Examine the publicly documented native ResourceLoader/ResourceToken function ABI, resource lifecycle and callback scheduling; continue using our independent game-specific address resolver, not RED4ext.dll's default UniversalReloc.
2. Implement typed read-only resource access in a mock-tested native adapter. Confirm the resource object is actually `inkStyleResource`, contains a nonempty `styles` array, and expose a `MainColors.Red` property with verified Variant/HDRColor RTTI.
3. Preserve original typed Variant/color and lifetime. Implement one reversible change, resource refresh and reset with rollback tests. All failure paths must fail closed.
4. Only then create a new in-game release to verify visible change. Prefer reusing the existing on-demand Tab diagnostics in the same game session for troubleshooting without restarting.
5. Consider native main-menu integration and distinct Johnny/minimap/world-map behavior once the main palette works.

These are goals, **not implemented functionality**. Verified raw linker offsets and a mapped global pointer are not proof that a resource-loading function can be called safely from our worker thread.

## SDK and authoritative references

- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/ResourceLoader.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/ink/StyleResource.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/ink/StyleProperty.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/HDRColor.hpp
