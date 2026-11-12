# DreamUI

Free, open-source theme framework for Cyberpunk 2077's **existing** UI.

**Status: v0.1.0-alpha.7 — read-only native Ink style-array readiness inspection. Not a HUD recoloring mod yet.**

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
- Candidate Ink binding table and **mock-only** reversible apply/restore logic.
- Read-only Windows executable metadata probe with bounded PE parser and tests.
- Independent linker-map resolver: safely reads the game-provided address table, checks timestamps/section bounds and reads the ResourceLoader singleton after startup and safely inspects the cached Ink resource metadata without invoking game functions.
- Read-only cached vanilla Ink style lookup (version-gated) with mock-tested token-map and property-memory reader; no style writes.
- Native DreamCatalog model for listing installed themes, version display and choosing a fallback active theme; **no menu widget yet**.
- GitHub Actions tests and Windows x64 prereleases.

## Not implemented

Actual engine resource handles, Ink reading/writing, theme application, minimap/world-map recoloring, main-menu DreamUI tile and a self-contained ASI loader. Default RED4ext.SDK relocation APIs still require RED4ext; DreamUI does not call them.

See [alpha.7 testing](docs/TESTING.md), [engine probe](docs/ENGINE_PROBE.md), [independent resolver](docs/ENGINE_ADAPTER.md), [planned control hub](docs/CONTROL_HUB.md), and [native Ink integration](docs/INK_INTEGRATION.md).

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
