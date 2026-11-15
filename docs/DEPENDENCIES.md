# DreamUI dependency policy

This is a design-stage dependency audit, not a claim that the reboot has a working game build.

## Minimum runtime target for the recoloring core

| Framework | Status | Why |
| --- | --- | --- |
| RED4ext | Required | Provides the native plugin host needed by retained framework integrations |
| Codeware | Required | Supplies the resource callbacks and scriptable-service support used by the recoloring, minimap, and world-map code paths |
| redscript | Required | Compiles game scripting changes and UI method patches |
| ArchiveXL | Conditional | May be needed for custom UI resources, localization, or extending assets; evaluate when rebuilding the actual menus and visual assets |
| RedData | Remove | Existing use is in JSON preset serialization and deserialization; built-in palettes will be compiled or packaged with the mod; runtime JSON preset IO is not planned |
| RedFileSystem | Remove | Existing use is in JSON preset discovery, reading, writing, and deletion; fixed theme does not need it |
| Mod Settings | Remove | No requirement for an external settings mod; a small native settings control for bundled accents is planned later |

Framework requirements include transitive dependencies; for example, the retained native framework components require RED4ext. Release-specific minimum versions must be pinned and tested for the supported game build. We must not inherit a version requirement merely because it appeared in an earlier mod's description.

## Source-level audit (pre-import)

- Runtime UI color handling uses ink-style resource events and property updates.
- Minimap recoloring uses a resource callback and style property references.
- World-map handling uses material/environment resource callbacks.
- File storage and JSON handling are concentrated in preset storage and editing.
- The old menu editor, preview controllers, color sliders, and file-backed preset-list components are unnecessary; DreamUI will have only a small set of built-in accent IDs.
- Mod Settings metadata belongs to optional user preference controls, not to the color application mechanism itself.
- An existing ArchiveXL sidecar was used for localization associated with a mod menu. That alone does not establish ArchiveXL as required for the new recoloring core.

## Removal work before declaring dependency-free

1. Implement a compiled DreamUI base palette and built-in accent variants without runtime JSON parsing or file-system reads.
2. Replace the external preset editor with a default built-in accent; add native accent selection and persistence later, independently of JSON/file-system dependencies.
3. Remove imports, types, and references to JSON/preset storage and editor events.
4. Remove Mod Settings annotations and optional configuration classes where no longer used.
5. Keep the resource callbacks needed for HUD, minimap, and world map.
6. Audit every custom UI resource and the soundtrack packaging independently before deciding whether ArchiveXL is mandatory.
7. Compile and test in-game before marking the dependency list final.

## Planned release install list

**Provisional:** RED4ext + Codeware + redscript.

**May be added:** ArchiveXL if actual authored assets need it.

**Not planned:** RedData, RedFileSystem, Mod Settings.

Custom music must use distributable original or appropriately licensed content. A standard archive replacement may not need ArchiveXL, but this depends on the final implementation and must be tested.

## Status

Planning only. Nothing here proves the final package launches or recolors the game. No third-party source code is included in this document.
