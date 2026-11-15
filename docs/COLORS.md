# DreamUI built-in color system

**Status:** Design reference only. The colors in [the draft palette](../design/palette.json) are concepts approved for further development, not tested game-ready HDR values.

## One identity, selectable accents

DreamUI has one consistent base appearance:

- Cool white
- Silver-gray
- Muted icy gray-blue
- Dark graphite
- Charcoal
- Near-black

The base remains stable when a user chooses a different accent.

## Built-in accent presets

| ID | Display name | Purpose |
| --- | --- | --- |
| `amber` | Amber Yellow | **Default** warm highlight |
| `pink` | Neon Pastel Pink | Soft, bright alternative |
| `cyan` | Arctic Cyan | Cool, luminous alternative |
| `lavender` | Soft Lavender | Muted violet alternative |

Each preset defines three design shades: light, primary, and dark. Hex colors are design references; game implementation may require HDR conversion and tuning.

## Color roles and guardrails

Accent colors are intended for interactive highlights, selected states, non-critical map accents, and decoration.

Semantic colors for hostile/friendly identification, critical health, warnings, damage types, and quest meaning must be mapped separately. Never implement accent selection by globally replacing every property whose name resembles an accent color: a game property that appears red or yellow may be conveying gameplay information.

## Architecture

- **Design source:** `design/palette.json` stores design-time swatches and IDs; it is not loaded by the game.
- **Runtime:** Convert validated colors into code-owned constants or cooked resource data. Do not require RedData or RedFileSystem just to read the palette.
- **Initial release:** Start with the default Amber accent until runtime swapping is built and verified, even though multiple designs are available in the source.
- **Future native settings:** expose the built-in accent IDs through a DreamUI-specific settings control. The persistence mechanism should reuse available game-side settings or an existing dependency, but has not been selected or validated.
- **Scope:** This is not a downloadable preset system, a color editor, or a theme framework.

## Integration order

1. Map Frostline-inspired source properties to DreamUI base, accent, and semantic roles without blindly copying the palette.
2. Validate the default Amber design for HUD, minimap, and world map first.
3. Implement other built-in accent palettes against the same role mapping.
4. Add accent selection and persistence when the native settings UI is developed.
5. Test contrast, different menu states, both ink styles, localization strings, and gameplay signals in game.

None of these stages are represented as working game features by the presence of this JSON file.
