# DreamUI

Cyberpunk 2077 UI overhaul — first playable-script candidate.

## Current implementation (alpha.1 development)

- Frostline-based built-in HUD colors (92 main Ink properties, 10 Johnny properties)
- Ice/graphite minimap style references
- First world-map building, metro and grading recolor pass
- Amber accent default; three alternate accent color sets exist in code but cannot be selected yet

Install the `r6` folder into the Cyberpunk 2077 game directory. Requires **RED4ext, Codeware and redscript**. This first candidate has **no custom archive assets**, fonts, background changes or audio yet.

**Not game-verified:** It has not been compiled against a local game installation or tested in Cyberpunk. Treat it as an experimental build. If redscript compilation fails, remove `r6/scripts/DreamUI` and send the compiler log. Do not install alongside another mod that modifies these same Ink resources without checking conflicts.

Future work: native accent selector, more extensive HUD and map patch coverage, menu visual changes, fonts, and audio.

The discontinued native ASI project is not part of this source tree.
