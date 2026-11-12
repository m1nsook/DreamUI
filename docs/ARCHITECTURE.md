# DreamUI Architecture (prototype)

DreamUI modifies existing Cyberpunk 2077 Ink styles, colors, markers and menu background resources. It will **not** render a parallel user-interface framework. The game-facing renderer/patches are **not implemented** in the first proof of concept.

## Bootstrapping

`DreamUI.asi` is a Windows x64 DLL with an `.asi` extension. An ASI loader is still required to load it. The first native proof only checks the host process, discovers `.dream` files, validates their ZIP container and writes a diagnostic log. It never modifies game memory or saves.

The eventual one-ZIP installer must arrange for an appropriately licensed ASI loader when missing and **must not overwrite** any existing `version.dll`. No installer or binary distribution is released yet.

## `.dream` v1 packaging

- ZIP archive with the `.dream` extension; **stored entries only**, no encryption, no ZIP64.
- UTF-8 `manifest.json` in the package root; theme token data in `theme/colors.json`.
- `schemaVersion: 1` and `type: theme` required.
- Files have deterministic order/timestamps; maximum 128 files and 16 MiB.
- No `..`, absolute paths, backslashes, drive prefixes, or duplicate filenames.
- Runtime verifies ZIP member CRC. The developer-side Python packager additionally validates the JSON schema.

Theme values are **semantic intentions**, not claims that game color overrides exist yet. For example, `map.enemy` will eventually map to the game's existing hostile marker color. Unsupported mappings must never silently recolor other elements.

## Near-term milestones

1. Validate ASI load and logs on Cyberpunk 2077 2.5 without game patches.
2. Identify existing Ink style / minimap marker resource bindings using independently authored code and public SDK definitions.
3. Apply and restore one non-critical HUD style color safely.
4. Expand bindings for loot, minimap, world map and notifications; test in-game before declaring them supported.
5. Create self-contained installer and NextGuide developer documentation after native compatibility is demonstrated.

## Source/license boundary

References such as HUD Painter and paid menu-background mods are for behavior/resource research. DreamUI must not redistribute their code, art, or paid assets without explicit permission. Public MIT-licensed SDKs can be used under their license terms.

## Alpha.3 read-only native probe and Dream Manager model

Alpha.3 reads the Windows game executable file from disk to log PE/VersionInfo metadata. It never scans live game memory or patches Ink. It also builds an ordered DreamCatalog model from validated themes for a future native main-menu tile. See [engine diagnostics](ENGINE_PROBE.md) and [control hub specification](CONTROL_HUB.md).
