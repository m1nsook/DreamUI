# DreamUI v0.1.0-alpha.2 — native theme preflight

This is an experimental native runtime test, **not a HUD recolor mod**. It checks a .dream manifest, parses RGB token values, and logs proposed vanilla Ink bindings.

## Installation and test

1. Back up your modded game setup.
2. Use your existing compatible ASI loader. **Do not replace version.dll.**
3. Extract the release ZIP into the Cyberpunk 2077 game folder; it contains bin/x64/plugins/DreamUI.asi and dreamui/themes/Default.dream.
4. Launch Cyberpunk 2077 2.5 and exit. The UI should look unchanged.
5. Inspect dreamui/logs/DreamUI.log for:
   - DreamUI alpha.2 native theme preflight
   - Validated theme dreamui.default version 0.1.0
   - Candidate Ink mapping text.primary -> MainColors.PanelWhite
   - Theme preflight completed: 1 valid theme(s)
   - Ink resource adapter not yet verified; no game patches applied

If the game crashes, remove DreamUI.asi and report the ASI loader and game version.

## Changes from alpha.1

- Native JSON schema and RGB token validation, not just a ZIP member count.
- Four explicit candidate Ink binding paths.
- Mock-tested reversible color apply/restore interface.

The native game color adapter is not implemented; the release intentionally performs no memory writes or UI changes. No paid assets or extra scripting frameworks are bundled.
