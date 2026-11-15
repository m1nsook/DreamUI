module DreamUI

// Independent color roles for game UI surfaces. These are not global Ink
// MainColors entries; tweaking them affects only the intended component.
// Start with the same values as the first in-game-verified minimap appearance.
public abstract class DreamUIElementColors {
  public static final func MinimapRoad() -> HDRColor {
    return HDRColor(0.17, 0.21, 0.25, 1.0);
  }

  public static final func MinimapFloor() -> HDRColor {
    return HDRColor(0.92, 0.94, 0.95, 1.0);
  }

  public static final func MinimapExteriorWall() -> HDRColor {
    return HDRColor(0.74, 0.79, 0.82, 1.0);
  }

  public static final func MinimapInteriorWall() -> HDRColor {
    return HDRColor(0.74, 0.79, 0.82, 1.0);
  }
}
