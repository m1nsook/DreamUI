module DreamUI

// World map colors use the same authored palette as the HUD.
// No settings UI, disk-based presets, or custom material resources required.
public class DreamUIWorldMapService extends ScriptableService {
  private cb func OnLoad() {
    let callbacks: ref<CallbackSystem> = GameInstance.GetCallbackSystem();

    callbacks
      .RegisterCallback(n"Resource/Ready", this, n"OnEnvironmentReady")
      .AddTarget(ResourceTarget.Path(r"base\\weather\\24h_basic\\3dmap.envparam"));

    callbacks
      .RegisterCallback(n"Resource/Ready", this, n"OnBuildingsReady")
      .AddTarget(ResourceTarget.Path(r"base\\materials\\3d_map_cubes.mt"));

    callbacks
      .RegisterCallback(n"Resource/Loaded", this, n"OnMetroLoaded")
      .AddTarget(ResourceTarget.Path(r"base\\materials\\3d_map_metro.mt"));
  }

  private cb func OnEnvironmentReady(event: ref<ResourceEvent>) -> Void {
    let resource: ref<worldEnvironmentAreaParameters> = event.GetResource() as worldEnvironmentAreaParameters;
    if !IsDefined(resource) { return; };

    for parameter in resource.renderAreaSettings.areaParameters {
      let grading: ref<ColorGradingAreaSettings> = parameter as ColorGradingAreaSettings;
      if IsDefined(grading) {
        grading.contrast = 1.10;
        grading.saturation = 0.95;
        break;
      };
    };
  }

  private cb func OnBuildingsReady(event: ref<ResourceEvent>) -> Void {
    let resource: ref<CMaterialTemplate> = event.GetResource() as CMaterialTemplate;
    if !IsDefined(resource) || ArraySize(resource.parameters) <= 2 { return; };

    let entries: array<ref<CMaterialParameter>> = resource.parameters[2];
    for parameter in entries {
      let target: ref<CMaterialParameterColor> = parameter as CMaterialParameterColor;
      if IsDefined(target) {
        if Equals(target.parameterName, n"BaseColorScale") {
          target.color = this.FromPalette(n"MainColors.Red", 255);
        } else if Equals(target.parameterName, n"EdgeColor") {
          target.color = this.FromPalette(n"MainColors.MildRed", 255);
        };
      };
    };
  }

  private cb func OnMetroLoaded(event: ref<ResourceEvent>) -> Void {
    let resource: ref<CMaterialTemplate> = event.GetResource() as CMaterialTemplate;
    if !IsDefined(resource) || ArraySize(resource.parameters) <= 2 { return; };

    let entries: array<ref<CMaterialParameter>> = resource.parameters[2];
    for parameter in entries {
      let target: ref<CMaterialParameterColor> = parameter as CMaterialParameterColor;
      if IsDefined(target) && Equals(target.parameterName, n"Color") {
        target.color = this.FromPalette(n"MainColors.Blue", 60);
        break;
      };
    };
  }

  private final func FromPalette(key: CName, alpha: Int32) -> Color {
    let color: ref<DreamUIColorValue> = DreamUIPalette.Resolve(key, false);
    if !IsDefined(color) {
      return Color(Cast<Uint8>(255), Cast<Uint8>(255), Cast<Uint8>(255), Cast<Uint8>(alpha));
    };

    let red: Int32 = Cast<Int32>(color.value.Red * 255.0);
    let green: Int32 = Cast<Int32>(color.value.Green * 255.0);
    let blue: Int32 = Cast<Int32>(color.value.Blue * 255.0);

    return Color(
      Cast<Uint8>(Max(0, Min(red, 255))),
      Cast<Uint8>(Max(0, Min(green, 255))),
      Cast<Uint8>(Max(0, Min(blue, 255))),
      Cast<Uint8>(alpha));
  }
}
