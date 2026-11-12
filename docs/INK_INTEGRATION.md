# Native Ink integration — alpha.2

## Current scope

Alpha.1 log confirmed ASI startup and ZIP reading. Alpha.2 introduces native theme manifest and semantic color parsing, candidate mappings for native Ink style properties, and a mock-tested apply/restore session.

The game-facing Ink color writer remains **unimplemented**. These mappings do not imply that patches have been made in Cyberpunk's runtime.

## Independent address resolution

RED4ext.SDK can describe native engine structures, but its default UniversalReloc resolver calls RED4ext.dll and RED4ext_ResolveAddress. Without RED4ext, it can terminate the game process. DreamUI must not call that path until its own address resolver has been independently implemented and verified against the game executable.

Reference: https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Relocation-inl.hpp

## Initial candidate bindings

Resource: base\gameplay\gui\common\main_colors.inkstyle

| Theme semantic token | Vanilla style property |
| --- | --- |
| text.primary | MainColors.PanelWhite |
| text.secondary | MainColors.Grey |
| text.disabled | MainColors.DarkGrey |
| selection.active | MainColors.ActiveYellow |

These are existing Ink style property names, identified from a vanilla-style color list, not validated native runtime hooks. The project contains independently authored code, no HUD Painter source or paid assets.

## Next native adapter milestone

- Detect game executable version and fail closed on unknown builds.
- Resolve engine functions independently of RED4ext at runtime, using verified versioned signatures only.
- Read the actual vanilla Ink resource and capture its original value.
- Validate native thread scheduling and resource refresh requirements before any modification.
- Apply exactly one reversible override with tests for failed patching and reset/reload.
- Test in-game with and without other UI mods.
