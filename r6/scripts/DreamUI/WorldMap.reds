module DreamUI

// First map pass: vanilla world-map materials only. Road assets come later.
public class DreamUIWorldMapService extends ScriptableService {
  private cb func OnLoad() {
    let callbacks: ref<CallbackSystem> = GameInstance.GetCallbackSystem();
    callbacks
      .RegisterCallback(n"Resource/Ready", this, n"OnEnvironmentReady")
      .AddTarget(ResourceTarget.Path(r"base\weather\24h_basic\3dmap.envparam"));
    callbacks
      .RegisterCallback(n"Resource/Ready", this, n"OnBuildingsReady")
      .AddTarget(ResourceTarget.Path(r"base\materials\3d_map_cubes.mt"));
    callbacks
      .RegisterCallback(n"Resource/Loaded", this, n"OnMetroLoaded")
      .AddTarget(ResourceTarget.Path(r"base\materials\3d_map_metro.mt"));
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
    if !IsDefined(resource) { return; };
    if ArraySize(resource.parameters) <= 2 { return; };
    let entries: array<ref<CMaterialParameter>> = resource.parameters[2];

    for parameter in entries {
      let target: ref<CMaterialParameterColor> = parameter as CMaterialParameterColor;
      if IsDefined(target) {
        if Equals(target.parameterName, n"BaseColorScale") {
          target.color = this.ColorRGBA(156, 176, 189, 255);
        };
        if Equals(target.parameterName, n"EdgeColor") {
          target.color = this.ColorRGBA(91, 111, 126, 255);
        };
      };
    };
  }

  private cb func OnMetroLoaded(event: ref<ResourceEvent>) -> Void {
    let resource: ref<CMaterialTemplate> = event.GetResource() as CMaterialTemplate;
    if !IsDefined(resource) { return; };
    if ArraySize(resource.parameters) <= 2 { return; };
    let entries: array<ref<CMaterialParameter>> = resource.parameters[2];

    for parameter in entries {
      let target: ref<CMaterialParameterColor> = parameter as CMaterialParameterColor;
      if IsDefined(target) && Equals(target.parameterName, n"Color") {
        target.color = this.ColorRGBA(140, 190, 210, 60);
        break;
      };
    };
  }

  private final func ColorRGBA(r: Int32, g: Int32, b: Int32, a: Int32) -> Color {
    return Color(Cast<Uint8>(r), Cast<Uint8>(g), Cast<Uint8>(b), Cast<Uint8>(a));
  }
}
