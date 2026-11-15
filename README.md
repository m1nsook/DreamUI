# DreamUI

A cohesive visual overhaul for Cyberpunk 2077, with one shared visual identity and selectable built-in accent palettes.

**Status:** Project reboot — planning and source-layout stage. No installable build is available yet.

## Planned features

- Unified HUD recolor with one DreamUI base palette and built-in accent choices (Amber Yellow default, Neon Pastel Pink, Arctic Cyan and Soft Lavender)
- Minimap recolor
- World map recolor
- New menu backgrounds
- Blur and vignette treatments
- DreamUI typography, favoring existing in-game Blender font resources over Rajdhani where suitable; first release prioritizes Latin-script legibility
- Custom main-menu music

The goal is one integrated mod with built-in accent presets, **not** a general theme framework or importable preset editor.

## Technical direction

- Build upon a proven existing recoloring implementation, with appropriate permissions.
- Reorganize and meaningfully extend the implementation to fit this project.
- Use established game modding frameworks where appropriate.
- Package the work as one clear installation, which may contain multiple game-supported file formats.
- Test the new modifications, integrations, and final package.

## Planned runtime requirements

The current minimal target is **RED4ext, Codeware, and redscript**. **ArchiveXL** remains conditional on how the final UI assets are implemented. The file-based preset editor will be removed; built-in accents are planned as static data with a future native selection UI, so **RedData, RedFileSystem, and Mod Settings** are not planned requirements. See [dependency audit](docs/DEPENDENCIES.md). This is not a tested install list yet.

See the [color architecture and accent concept](docs/COLORS.md).

## Discontinued approach

The previous custom ASI plugin, engine hooks, theme package format, native probing experiments, and associated tooling are no longer part of the project.

## Status

The repository will be populated incrementally as the reboot implementation is established.
