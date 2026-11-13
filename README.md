# DreamUI

Free, open-source theme framework for Cyberpunk 2077's **existing** UI.

**Status: v0.1.0-alpha.8 — on-demand Tab-triggered native Ink diagnostics. Not a HUD recoloring mod yet.**

DreamUI aims to improve Cyberpunk's native HUD, world map, minimap markers, item rarity, looting and menus without introducing a separate widget framework.

## Goals

- One self-contained installation without mandatory externally installed script frameworks.
- Native Ink UI and existing game visual resources only; no custom overlay engine.
- Portable, independently authored `.dream` theme packages.
- Semantic colors for UI hierarchy, hostile/friendly markers, and item rarity.
- Free themes, backgrounds and developer tooling.

## Implemented

- Python package builder and dependency-free C++ ZIP reader.
- ASI startup verified in-game for alpha.1 and theme parsing verified for alpha.2.
- Manifest validation and 20 semantic color tokens.
- Candidate Ink binding table and **mock-only** reversible RGB apply/restore logic.
- **Native typed HDRColor lifecycle core:** game-resource event identity/generation checks, engine-thread gate, exact four-float snapshot, one-property transaction, refresh, restoration and rollback retry; mock-tested.
- **Conditional native serializer-stage bridge:** source-only experimental hook with strong serializer-owned handle checks, three-property HDRColor preflight, atomic transactions, journaled rollback and exact-instance restoration. A dedicated opt-in build and marker enable **read-only** inspection; a second independent marker is required for native writes. Ordinary builds keep the bridge disabled.
- Read-only Windows executable metadata probe with bounded PE parser and tests.
- Independent linker-map resolver: safely reads the game-provided address table, checks timestamps/section bounds and reads the ResourceLoader singleton after startup and safely inspects the cached Ink resource metadata without invoking game functions.
- On-demand **Tab** diagnostic (no timed polls): read-only cached Ink style inspection, including bounded linked-resource traversal. No game writes.
- **Engine-owned clone groundwork:** source-only `InkOwnedCloneSession` manages game-created strong handles through a strict native backend contract, including partial-failure cleanup and source/clone verification; the standalone address resolver can locate the native `ISerializable::Clone` symbol. No clone call, new resource ownership or refresh is enabled in-game yet.
- **Type-checked full-style ownership experiment:** the experimental serializer callback can also retain a complete, pointer-free copy of a style tree when every property is a supported `HDRColor` Variant. Unknown/unsafe Variant types invalidate the whole clone; no native allocator, live refresh, or game-memory write is enabled by this snapshot.
- **Pointer-free first-load color capture:** the guarded native hook can retain verified original HDRColor values and style identities in DreamUI-owned memory, without retaining unsafe engine pointers. This is not a copy of the full Ink styles array or an instant restore.
- **Automatic experimental native load trace:** in an opt-in source build, capture guarded serializer-stage callback and style preflight counters to `DreamUI-load.log` as the game loads, without Tab or developer commands. No native writes in observation mode; installing a native hook still carries risk.
- A headless developer command registry/inbox adapted from the owner's DreamUI prototype; supports manual screen-phase labels without an ImGui/DX12 overlay.
- **Validated theme selection:** `select <theme-id>` chooses from installed `.dream` packages, saves an active-theme preference, and supplies the theme's colors to the guarded native bridge at the next authenticated resource load; not instant switching.
- Native DreamCatalog model for listing installed themes, version display and choosing a fallback active theme; **no menu widget yet**.
- GitHub Actions tests and Windows x64 prereleases.

## Not implemented

A game-verified native serializer callback, reliable immediate Ink refresh and in-game theme application, minimap/world-map recoloring, main-menu DreamUI tile and a self-contained ASI loader. The source-only experimental hook and read/write adapters are not yet verified in a running game. Default RED4ext.SDK relocation APIs still require RED4ext; DreamUI does not call them.

An [optional HUD Painter exporter](docs/HUDPAINTER_COMPAT.md) can apply `.dream` palettes through HUD Painter in installations that already have it, without making HUD Painter a dependency of DreamUI's ASI.

See [native serializer-stage bridge](docs/NATIVE_SERIALIZER_STAGE.md), [automatic load trace](docs/AUTOMATIC_LOAD_TRACE.md), [developer commands](docs/DEVELOPER_COMMANDS.md), [native Ink lifecycle implementation](docs/NATIVE_RESOURCE_LIFECYCLE.md), [HUD Painter source research](docs/HUD_PAINTER_REFERENCE.md), [alpha.8 testing](docs/TESTING.md), [engine probe](docs/ENGINE_PROBE.md), [independent resolver](docs/ENGINE_ADAPTER.md), [planned control hub](docs/CONTROL_HUB.md), and [native Ink integration](docs/INK_INTEGRATION.md).

## Build

On Windows with MSVC and CMake 3.23+:

```powershell
python -m unittest discover -s tests -p 'test_*.py' -v
python tools/dreamtool.py pack themes/Default dist/Default.dream
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

A compatible ASI loader is currently required; do not overwrite existing `version.dll`. No paid or proprietary mods are included. DreamUI-authored code is MIT licensed.
