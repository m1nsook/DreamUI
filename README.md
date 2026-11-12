# DreamUI

**Free, open-source theme framework for Cyberpunk 2077's existing UI.**

DreamUI aims to improve the readability and appearance of the *native* game HUD, inventory, looting, minimap, world map, notifications and menus, without introducing a replacement widget framework.

> **Status: experimental native bootstrap (v0.1.0 prototype). Not an in-game recolor mod yet.** No game UI hooks are currently implemented, and no release is considered ready for players.

## Project goals

- A single self-contained installation; no separately downloaded scripting framework.
- Existing game UI widgets and marker resources, not custom UI overlays.
- User themes packaged as portable, verifiable `.dream` files.
- Distinct semantic colors for hostiles, friendly markers, loot rarity, selected controls, disabled labels and warnings.
- Optional backgrounds and world-map themes without premium restrictions.
- Free and open-source code, community-contributed themes, and NextGuide documentation.

## Current working components

- Dependency-free Python packer that validates and produces deterministic `.dream` theme archives.
- Dependency-free C++ ZIP reader with CRC, bounds and path checks, plus a Windows x64 ASI startup/logging prototype.
- A CI workflow compiling the `.asi` and running native/package-format tests.

The ASI startup and in-game behavior **must be tested on Cyberpunk 2.5**. The ZIP archive and Windows loader logic are not evidence that UI recoloring works.

## Build

Use Python 3 and CMake 3.23+ with a Windows MSVC toolchain for the ASI:

```powershell
python -m unittest discover -s tests -p 'test_*.py' -v
python tools/dreamtool.py pack themes/Default dist/Default.dream
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The artifact contains `DreamUI.asi` and `Default.dream`, **not** an all-in-one installer. During the prototype, an ASI loader is required (e.g. Ultimate ASI Loader); do not replace an existing game `version.dll`.

See [architecture and known limitations](docs/ARCHITECTURE.md). Docs site planned with NextGuide when the native integration is ready.

## Third-party code and licensing

DreamUI does not embed HUD Painter, paid background packs, Codeware, RED4ext or redscript code. References to existing mods are for compatibility/resource investigation only. See the MIT [LICENSE](LICENSE) for DreamUI-authored code.
