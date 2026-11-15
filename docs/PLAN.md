# DreamUI Development Plan

## Product

DreamUI is a single, consistent Cyberpunk 2077 visual identity with several built-in selectable accent palettes.

### Visual workstreams

1. Shared frost-inspired base palette, reserved gameplay semantics, and built-in accent presets
2. Minimap and in-world navigation colors
3. World map presentation and icon differentiation
4. Menu backgrounds and treatments
5. Blur and vignette effects
6. DreamUI typography, including Latin-script character coverage
7. Main-menu soundtrack

## Development stages

### 1. Baseline

- Inventory the code and assets permitted for reuse.
- Document upstream behavior privately where required.
- Identify dependencies needed for the planned feature set.
- Bring in only necessary components after review.

### 2. Recoloring

- Implement the stable base palette and default Amber accent for HUD styling.
- Add built-in candidate accents (Neon Pastel Pink, Arctic Cyan, Soft Lavender) against explicit role mappings.
- Keep user-facing preset import/export and file-based palette editing out of scope; add native accent selection later.
- Extend minimap recoloring.
- Extend world map recoloring.
- Check contrast, readability, and gameplay state distinctions when accent selection changes.

### 3. Menu and sound

- Implement redesigned menu backgrounds.
- Add and tune blur and vignette effects.
- Integrate an original or properly licensed menu music track.

### 4. Typography

- Audit existing game font resources and font-family references.
- Prefer the game's existing Blender typography where available, avoiding unnecessary bundled commercial font files.
- Design correct font weight mapping, readability, and interface spacing.
- Cover English and Latin text including punctuation, numbers, and extended Latin characters where the installed resources allow it.
- Preserve readable fallbacks for other languages rather than breaking unsupported glyphs.
- If new font binaries are ever needed, confirm explicit redistribution/embedding rights before packaging.
- Test menus, HUD, subtitles, and long strings for clipping and missing glyphs.

### 5. Integrated release

- Test modified behavior and compatibility.
- Package a unified installation.
- Publish accurate installation and conflict guidance.
- Fulfill the agreed release acknowledgements in the designated release platform.

## Deliberately out of scope

- A custom ASI loader or native game-hook experiment
- Portable theme-pack format or end-user theme builder
- General-purpose visual framework
