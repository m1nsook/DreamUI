# Optional HUD Painter preset exporter

DreamUI includes a source-only compatibility exporter for installations
that **already have HUD Painter 1.4.1**. It can create a usable native Ink
palette preset from a DreamUI .dream theme. **This is optional**: HUD
Painter, Codeware, redscript and RED4ext do not become mandatory for
the standalone DreamUI ASI.

No scripts, binaries or default presets from HUD Painter are redistributed.

## Usage

Run Python 3.10+ from the DreamUI repository root, substituting the
actual location of your game install:

```powershell
python tools/hudpainter_export.py themes/Default "C:\Games\Cyberpunk 2077\r6\storages\HUDPainter\DEFAULT.json" "C:\Games\Cyberpunk 2077\r6\storages\HUDPainter\DreamUI Default.json"
```

A packed .dream archive can replace `themes/Default`.

The exporter refuses to overwrite the original DEFAULT preset or
any existing filename. Open HUD Painter's own in-game preset manager,
choose **DreamUI Default** to apply the palette, and select **DEFAULT**
to revert through HUD Painter.

## Scope

This exports a full native preset by preserving the supplied baseline's
property arrays. Only three properties are changed:

| DreamUI semantic token | Native inkStyleProperty |
| --- | --- |
| `text.primary` | `MainColors.Red` |
| `text.secondary` | `MainColors.PanelBlue` |
| `selection.active` | `MainColors.ActiveYellow` |

The output preserves all other native colors (including Johnny values),
alpha channels, and the baseline schema. Each RGB color is converted to
four-channel HDRColor fields with the original native property peak
brightness; this is a preview, not a calibrated HDR transfer function.

The exporter checks theme metadata, bounded and safe ZIP entries,
validated property arrays, duplicate property names, finite channel
values, and refuses to overwrite existing files.

This provides an **actual visible native Ink recolor through HUD Painter**,
without enabling unsafe, unverified direct memory writes in DreamUI.
It does NOT yet complete DreamUI's independently implemented native
resource callback, apply, refresh or restoration.
