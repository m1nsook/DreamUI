# DreamUI typography direction

## Intent

Favor the Blender family already present in Cyberpunk 2077 where appropriate, replacing Rajdhani usages selectively for a more cohesive DreamUI appearance.

The initial supported design target is English and Latin-script text, including punctuation, digits and commonly used accented characters. Languages with unsupported glyphs should retain a readable game fallback; never blindly replace a font family for every localization.

## Implementation approach

1. Audit `base\\gameplay\\gui\\fonts` and the existing font families/styles available to the game.
2. Identify Rajdhani references in relevant menu and HUD widgets and choose selective replacement rather than a global forced swap by default.
3. Map Regular, Medium, Semibold and Bold to available font styles, avoiding simulated weight where possible.
4. Package only authorized assets or modified game references; font binaries require separate distribution/embedding permission.
5. Compare English and accented Latin samples, numbers, special characters, localization strings, clipping and layout behavior in-game.
6. Confirm whether any scripting or framework dependency is genuinely needed; a vanilla `.archive` asset replacement alone may suffice.

## Licensing caveat

Blender is a commercial typeface distributed by Binnenland. Presence of a font in an installed game does not automatically authorize redistributing a standalone copy of its font software. We may reference game-owned assets already present on the player's installation, or obtain separate permission for any redistributed binaries. Do not check proprietary font files into GitHub.

## Status

Planning only. No font assets have been imported or modified yet.
