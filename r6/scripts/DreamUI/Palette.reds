module DreamUI

// HDR values from the Frostline starting palette; no runtime JSON or file access.
public enum DreamUIAccent {
  Amber = 0,
  Pink = 1,
  Cyan = 2,
  Lavender = 3,
}

public enum DreamUIAccentShade {
  Bright = 0,
  Main = 1,
  Dim = 2,
}

public class DreamUIColorValue {
  public let value: HDRColor;

  public static final func Make(red: Float, green: Float, blue: Float) -> ref<DreamUIColorValue> {
    let result: ref<DreamUIColorValue> = new DreamUIColorValue();
    result.value = HDRColor(red, green, blue, 1.0);
    return result;
  }
}

public abstract class DreamUIPalette {
  // The default is fixed during alpha.1. A native in-game selector is planned later.
  public static final func ActiveAccent() -> DreamUIAccent {
    return DreamUIAccent.Amber;
  }

  // These alternatives are built in, but cannot yet be selected in the game.
  public static final func AccentColor(shade: DreamUIAccentShade) -> ref<DreamUIColorValue> {
    let accent: DreamUIAccent = DreamUIPalette.ActiveAccent();

    if Equals(accent, DreamUIAccent.Pink) {
      if Equals(shade, DreamUIAccentShade.Bright) { return DreamUIColorValue.Make(1.05, 0.73, 1.02); };
      if Equals(shade, DreamUIAccentShade.Main) { return DreamUIColorValue.Make(0.96, 0.47, 0.77); };
      return DreamUIColorValue.Make(0.45, 0.21, 0.40);
    };

    if Equals(accent, DreamUIAccent.Cyan) {
      if Equals(shade, DreamUIAccentShade.Bright) { return DreamUIColorValue.Make(0.75, 1.12, 1.22); };
      if Equals(shade, DreamUIAccentShade.Main) { return DreamUIColorValue.Make(0.38, 0.90, 1.03); };
      return DreamUIColorValue.Make(0.14, 0.42, 0.53);
    };

    if Equals(accent, DreamUIAccent.Lavender) {
      if Equals(shade, DreamUIAccentShade.Bright) { return DreamUIColorValue.Make(0.99, 0.87, 1.17); };
      if Equals(shade, DreamUIAccentShade.Main) { return DreamUIColorValue.Make(0.70, 0.57, 1.00); };
      return DreamUIColorValue.Make(0.34, 0.27, 0.60);
    };

    if Equals(shade, DreamUIAccentShade.Bright) { return DreamUIColorValue.Make(1.12, 0.84, 0.42); };
    if Equals(shade, DreamUIAccentShade.Main) { return DreamUIColorValue.Make(1.00, 0.73, 0.34); };
    return DreamUIColorValue.Make(0.73, 0.60, 0.43);
  }

  // Accent overrides are deliberately limited to decorative/highlight roles.
  // Enemy colors, warnings, health danger and damage type colors remain semantic.
  private static final func AccentOverride(path: CName) -> ref<DreamUIColorValue> {
    if Equals(DreamUIPalette.ActiveAccent(), DreamUIAccent.Amber) { return null; };

    if Equals(path, n"MainColors.ActiveRed") ||
       Equals(path, n"MainColors.ActiveYellow") {
      return DreamUIPalette.AccentColor(DreamUIAccentShade.Bright);
    };
    if Equals(path, n"MainColors.Gold") ||
       Equals(path, n"MainColors.Orange") ||
       Equals(path, n"MainColors.SubtitleSpeaker") {
      return DreamUIPalette.AccentColor(DreamUIAccentShade.Main);
    };
    if Equals(path, n"MainColors.PanelGold") ||
       Equals(path, n"MainColors.MildOrange") {
      return DreamUIPalette.AccentColor(DreamUIAccentShade.Dim);
    };
    return null;
  }

  public static final func Resolve(path: CName, johnny: Bool) -> ref<DreamUIColorValue> {
    let accent: ref<DreamUIColorValue> = DreamUIPalette.AccentOverride(path);
    if IsDefined(accent) { return accent; };

    if johnny {
    if Equals(path, n"MainColors.Red") { return DreamUIColorValue.Make(0.92, 0.94, 0.95); };
    if Equals(path, n"MainColors.ActiveRed") { return DreamUIColorValue.Make(1.11, 0.79, 0.38); };
    if Equals(path, n"MainColors.CombatRed") { return DreamUIColorValue.Make(1.2, 0.43, 0.4); };
    if Equals(path, n"MainColors.PanelRed") { return DreamUIColorValue.Make(0.81, 0.85, 0.86); };
    if Equals(path, n"MainColors.Blue") { return DreamUIColorValue.Make(0.63, 0.77, 0.83); };
    if Equals(path, n"MainColors.ActiveBlue") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.Yellow") { return DreamUIColorValue.Make(0.86, 0.71, 0.46); };
    if Equals(path, n"MainColors.ActiveYellow") { return DreamUIColorValue.Make(1.12, 0.84, 0.42); };
    if Equals(path, n"MainColors.SubtitleRed") { return DreamUIColorValue.Make(0.83, 0.86, 0.87); };
    if Equals(path, n"MainColors.SubtitleBlue") { return DreamUIColorValue.Make(0.69, 0.84, 0.89); };
      return null;
    };

    if Equals(path, n"MainColors.Red") { return DreamUIColorValue.Make(0.92, 0.94, 0.95); };
    if Equals(path, n"MainColors.PanelRed") { return DreamUIColorValue.Make(0.79, 0.83, 0.85); };
    if Equals(path, n"MainColors.ActiveRed") { return DreamUIColorValue.Make(1.11, 0.79, 0.38); };
    if Equals(path, n"MainColors.DamageType_Critical") { return DreamUIColorValue.Make(1.2, 0.43, 0.4); };
    if Equals(path, n"MainColors.MildRed") { return DreamUIColorValue.Make(0.61, 0.67, 0.71); };
    if Equals(path, n"MainColors.DarkRed") { return DreamUIColorValue.Make(0.15, 0.18, 0.2); };
    if Equals(path, n"MainColors.PanelDarkRed") { return DreamUIColorValue.Make(0.11, 0.14, 0.16); };
    if Equals(path, n"MainColors.FaintRed") { return DreamUIColorValue.Make(0.15, 0.18, 0.2); };
    if Equals(path, n"MainColors.CombatRed") { return DreamUIColorValue.Make(1.2, 0.43, 0.4); };
    if Equals(path, n"MainColors.CombatRedNoHDR") { return DreamUIColorValue.Make(1.05, 0.38, 0.38); };
    if Equals(path, n"MainColors.MediumRed") { return DreamUIColorValue.Make(0.11, 0.14, 0.16); };
    if Equals(path, n"MainColors.DarkerRed") { return DreamUIColorValue.Make(0.25, 0.29, 0.32); };
    if Equals(path, n"MainColors.Blue") { return DreamUIColorValue.Make(0.64, 0.81, 0.94); };
    if Equals(path, n"MainColors.PanelBlue") { return DreamUIColorValue.Make(0.59, 0.75, 0.84); };
    if Equals(path, n"MainColors.DamageTypeEMP") { return DreamUIColorValue.Make(0.69, 0.84, 0.89); };
    if Equals(path, n"MainColors.ActiveBlue") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.DamageTypeEMP_Critical") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.MildBlue") { return DreamUIColorValue.Make(0.55, 0.6, 0.63); };
    if Equals(path, n"MainColors.DarkBlue") { return DreamUIColorValue.Make(0.17, 0.21, 0.25); };
    if Equals(path, n"MainColors.PanelDarkBlue") { return DreamUIColorValue.Make(0.11, 0.14, 0.16); };
    if Equals(path, n"MainColors.FaintBlue") { return DreamUIColorValue.Make(0.15, 0.18, 0.2); };
    if Equals(path, n"MainColors.MediumBlue") { return DreamUIColorValue.Make(0.6, 0.76, 0.86); };
    if Equals(path, n"MainColors.DarkerBlue") { return DreamUIColorValue.Make(0.045, 0.052, 0.064); };
    if Equals(path, n"MainColors.Yellow") { return DreamUIColorValue.Make(0.86, 0.71, 0.46); };
    if Equals(path, n"MainColors.Gold") { return DreamUIColorValue.Make(1.0, 0.73, 0.34); };
    if Equals(path, n"MainColors.PanelGold") { return DreamUIColorValue.Make(0.86, 0.69, 0.42); };
    if Equals(path, n"MainColors.Overshield") { return DreamUIColorValue.Make(0.69, 0.84, 0.89); };
    if Equals(path, n"MainColors.ActiveYellow") { return DreamUIColorValue.Make(1.12, 0.84, 0.42); };
    if Equals(path, n"MainColors.MildYellow") { return DreamUIColorValue.Make(0.73, 0.6, 0.43); };
    if Equals(path, n"MainColors.FaintYellow") { return DreamUIColorValue.Make(0.22, 0.18, 0.125); };
    if Equals(path, n"MainColors.DarkGold") { return DreamUIColorValue.Make(0.4, 0.34, 0.25); };
    if Equals(path, n"MainColors.PanelDarkGold") { return DreamUIColorValue.Make(0.4, 0.34, 0.25); };
    if Equals(path, n"MainColors.PanelDarkYellow") { return DreamUIColorValue.Make(0.14, 0.125, 0.1); };
    if Equals(path, n"MainColors.Green") { return DreamUIColorValue.Make(0.39, 0.88, 0.63); };
    if Equals(path, n"MainColors.PanelGreen") { return DreamUIColorValue.Make(0.36, 0.6, 0.48); };
    if Equals(path, n"MainColors.DamageTypeChemical") { return DreamUIColorValue.Make(0.48, 0.82, 0.64); };
    if Equals(path, n"MainColors.MildGreen") { return DreamUIColorValue.Make(0.36, 0.6, 0.48); };
    if Equals(path, n"MainColors.ActiveGreen") { return DreamUIColorValue.Make(0.43, 0.99, 0.72); };
    if Equals(path, n"MainColors.DarkGreen") { return DreamUIColorValue.Make(0.13, 0.28, 0.21); };
    if Equals(path, n"MainColors.PanelDarkGreen") { return DreamUIColorValue.Make(0.13, 0.28, 0.21); };
    if Equals(path, n"MainColors.Purple") { return DreamUIColorValue.Make(0.68, 0.66, 0.81); };
    if Equals(path, n"MainColors.LightPurple") { return DreamUIColorValue.Make(0.79, 0.77, 0.91); };
    if Equals(path, n"MainColors.StreetCred") { return DreamUIColorValue.Make(0.66, 0.85, 0.75); };
    if Equals(path, n"MainColors.Black") { return DreamUIColorValue.Make(0.018, 0.02, 0.022); };
    if Equals(path, n"MainColors.PanelBlack") { return DreamUIColorValue.Make(0.018, 0.02, 0.022); };
    if Equals(path, n"MainColors.Grey") { return DreamUIColorValue.Make(0.72, 0.77, 0.8); };
    if Equals(path, n"MainColors.DarkGrey") { return DreamUIColorValue.Make(0.39, 0.43, 0.46); };
    if Equals(path, n"MainColors.PanelDarkGrey") { return DreamUIColorValue.Make(0.55, 0.6, 0.63); };
    if Equals(path, n"MainColors.DarkerGrey") { return DreamUIColorValue.Make(0.25, 0.29, 0.32); };
    if Equals(path, n"MainColors.panelDarkerGrey") { return DreamUIColorValue.Make(0.25, 0.29, 0.32); };
    if Equals(path, n"MainColors.PanelDarkestGrey") { return DreamUIColorValue.Make(0.045, 0.052, 0.064); };
    if Equals(path, n"MainColors.White") { return DreamUIColorValue.Make(0.92, 0.94, 0.95); };
    if Equals(path, n"MainColors.PanelWhite") { return DreamUIColorValue.Make(0.92, 0.94, 0.95); };
    if Equals(path, n"MainColors.ActiveWhite") { return DreamUIColorValue.Make(1.08, 1.11, 1.13); };
    if Equals(path, n"MainColors.Orange") { return DreamUIColorValue.Make(1.0, 0.73, 0.34); };
    if Equals(path, n"MainColors.MildOrange") { return DreamUIColorValue.Make(0.85, 0.69, 0.46); };
    if Equals(path, n"MainColors.Warning") { return DreamUIColorValue.Make(1.05, 0.38, 0.38); };
    if Equals(path, n"MainColors.Neutral") { return DreamUIColorValue.Make(0.74, 0.79, 0.82); };
    if Equals(path, n"MainColors.Hacking") { return DreamUIColorValue.Make(0.69, 0.84, 0.89); };
    if Equals(path, n"MainColors.Cyberspace") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.GettingHacked") { return DreamUIColorValue.Make(1.2, 0.43, 0.4); };
    if Equals(path, n"MainColors.FastTravel") { return DreamUIColorValue.Make(0.49, 0.67, 0.72); };
    if Equals(path, n"MainColors.StrongFastTravel") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.QuickhackAccent") { return DreamUIColorValue.Make(0.49, 0.67, 0.72); };
    if Equals(path, n"MainColors.PanelGrey") { return DreamUIColorValue.Make(0.66, 0.7, 0.73); };
    if Equals(path, n"MainColors.DamageTypePhysical") { return DreamUIColorValue.Make(0.81, 0.85, 0.86); };
    if Equals(path, n"MainColors.DamageTypeThermal") { return DreamUIColorValue.Make(1.0, 0.73, 0.34); };
    if Equals(path, n"MainColors.DamageTypePhysical_Critical") { return DreamUIColorValue.Make(1.08, 1.11, 1.13); };
    if Equals(path, n"MainColors.DamageTypeChemical_Critical") { return DreamUIColorValue.Make(0.6, 0.99, 0.76); };
    if Equals(path, n"MainColors.DamageTypeThermal_Critical") { return DreamUIColorValue.Make(1.12, 0.84, 0.42); };
    if Equals(path, n"MainColors.TutorialColorIntensity") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.TutorialIntensity") { return DreamUIColorValue.Make(0.86, 1.02, 1.05); };
    if Equals(path, n"MainColors.TutorialColor") { return DreamUIColorValue.Make(0.69, 0.84, 0.89); };
    if Equals(path, n"MainColors.NPC_Chatter") { return DreamUIColorValue.Make(0.81, 0.85, 0.86); };
    if Equals(path, n"MainColors.EnemyBase") { return DreamUIColorValue.Make(1.02, 0.43, 0.385); };
    if Equals(path, n"MainColors.EnemyBaseNoHDR") { return DreamUIColorValue.Make(0.98, 0.42, 0.375); };
    if Equals(path, n"MainColors.EnemyMinimapBase") { return DreamUIColorValue.Make(0.957, 0.392, 0.376); };
    if Equals(path, n"MainColors.Fullscreen_PrimaryForegroundDarker") { return DreamUIColorValue.Make(0.28, 0.32, 0.34); };
    if Equals(path, n"MainColors.Fullscreen_PrimaryBackgroundDark") { return DreamUIColorValue.Make(0.075, 0.086, 0.099); };
    if Equals(path, n"MainColors.Fullscreen_PrimaryBackgroundDarkest") { return DreamUIColorValue.Make(0.023, 0.027, 0.033); };
    if Equals(path, n"MainColors.Fullscreen_SecondaryBackground1") { return DreamUIColorValue.Make(0.16, 0.18, 0.2); };
    if Equals(path, n"MainColors.Fullscreen_SecondaryBackground2") { return DreamUIColorValue.Make(0.12, 0.14, 0.16); };
    if Equals(path, n"MainColors.Fullscreen_SecondaryBackground3") { return DreamUIColorValue.Make(0.15, 0.18, 0.2); };
    if Equals(path, n"MainColors.Fullscreen_SecondaryBackground4") { return DreamUIColorValue.Make(0.25, 0.29, 0.32); };
    if Equals(path, n"MainColors.Fullscreen_VioletBackground") { return DreamUIColorValue.Make(0.12, 0.11, 0.2); };
    if Equals(path, n"MainColors.Fullscreen_RedDarkBackground") { return DreamUIColorValue.Make(0.12, 0.14, 0.16); };
    if Equals(path, n"MainColors.SubtitleSpeaker") { return DreamUIColorValue.Make(0.92, 0.73, 0.44); };
    if Equals(path, n"MainColors.NameplateBackground") { return DreamUIColorValue.Make(0.28, 0.095, 0.105); };
    if Equals(path, n"MainColors.SupBlue") { return DreamUIColorValue.Make(0.49, 0.67, 0.72); };
    if Equals(path, n"MainColors.SupRed") { return DreamUIColorValue.Make(0.83, 0.41, 0.4); };
    if Equals(path, n"MainColors.RelicColor") { return DreamUIColorValue.Make(0.72, 0.74, 0.96); };
    if Equals(path, n"MainColors.RelicDark") { return DreamUIColorValue.Make(0.12, 0.11, 0.2); };
    return null;
  }
}
