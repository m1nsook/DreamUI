# DreamUI

A cohesive, single-theme visual overhaul for Cyberpunk 2077.

**Status:** Project reboot — planning and source-layout stage. No installable build is available yet.

## Planned features

- Unified HUD recolor
- Minimap recolor
- World map recolor
- New menu backgrounds
- Blur and vignette treatments
- Custom main-menu music

The goal is one integrated mod, not a general theme framework or user-facing preset creation system.

## Technical direction

- Build upon a proven existing recoloring implementation, with appropriate permissions.
- Reorganize and meaningfully extend the implementation to fit this project.
- Use established game modding frameworks where appropriate.
- Package the work as one clear installation, which may contain multiple game-supported file formats.
- Test the new modifications, integrations, and final package.

## Planned runtime requirements

The current minimal target is **RED4ext, Codeware, and redscript**. **ArchiveXL** remains conditional on how the final UI assets are implemented. Preset storage and editor settings will be removed, so **RedData, RedFileSystem, and Mod Settings** are not planned requirements. See [dependency audit](docs/DEPENDENCIES.md). This is not a tested install list yet.

## Discontinued approach

The previous custom ASI plugin, engine hooks, theme package format, native probing experiments, and associated tooling are no longer part of the project.

## Status

The repository will be populated incrementally as the reboot implementation is established.
