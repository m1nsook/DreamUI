module DreamUI

// Apply DreamUI colors when the game's existing Ink style resources load.
public class DreamUIStyleService extends ScriptableService {
  private cb func OnLoad() {
    GameInstance.GetCallbackSystem()
      .RegisterCallback(n"Resource/Loaded", this, n"OnStyleResourceLoaded")
      .AddTarget(ResourceTarget.Path(r"base\\gameplay\\gui\\common\\main_colors.inkstyle"))
      .AddTarget(ResourceTarget.Path(r"base\\gameplay\\gui\\common\\themes\\main_colors_johnny.inkstyle"));
  }

  private cb func OnStyleResourceLoaded(event: ref<ResourceEvent>) -> Void {
    let resource: ref<inkStyleResource> = event.GetResource() as inkStyleResource;
    if !IsDefined(resource) { return; };
    if Equals(ArraySize(resource.styles), 0) { return; };

    let isJohnny: Bool = Equals(event.GetPath(), r"base\\gameplay\\gui\\common\\themes\\main_colors_johnny.inkstyle");
    if !isJohnny {
      this.RebindMinimap(resource);
    };

    let style: inkStyle = resource.styles[0];
    let editedProperties: array<inkStyleProperty>;
    let patched: Int32 = 0;

    for property in style.properties {
      let color: ref<DreamUIColorValue> = DreamUIPalette.Resolve(property.propertyPath, isJohnny);
      if IsDefined(color) {
        let edited: inkStyleProperty;
        edited.propertyPath = property.propertyPath;
        edited.value = ToVariant(color.value);
        ArrayPush(editedProperties, edited);
        patched += 1;
      } else {
        ArrayPush(editedProperties, property);
      };
    };

    style.properties = editedProperties;
    resource.styles[0] = style;
    ModLog(n"DreamUI", s"Colorized \(patched) properties (Johnny: \(isJohnny))");
  }

  // The minimap uses local HDR values rather than global MainColors references.
  // As a result, tuning dialogue/UI text colors cannot recolor these surfaces.
  private final func RebindMinimap(resource: ref<inkStyleResource>) -> Void {
    let index: Int32 = 0;
    while index < ArraySize(resource.styles) {
      if Equals(resource.styles[index].styleID, n"MiniMapStyle") {
        let minimap: inkStyle = resource.styles[index];
        let mapped: array<inkStyleProperty>;

        for property in minimap.properties {
          if Equals(property.propertyPath, n"MiniMapStyle.RoadColor") {
            ArrayPush(mapped, this.WithColor(property, DreamUIElementColors.MinimapRoad()));
          } else if Equals(property.propertyPath, n"MiniMapStyle.FloorColor") {
            ArrayPush(mapped, this.WithColor(property, DreamUIElementColors.MinimapFloor()));
          } else if Equals(property.propertyPath, n"MiniMapStyle.ExteriorWallColor") {
            ArrayPush(mapped, this.WithColor(property, DreamUIElementColors.MinimapExteriorWall()));
          } else if Equals(property.propertyPath, n"MiniMapStyle.InteriorWallColor") {
            ArrayPush(mapped, this.WithColor(property, DreamUIElementColors.MinimapInteriorWall()));
          } else {
            ArrayPush(mapped, property);
          };
        };

        minimap.properties = mapped;
        resource.styles[index] = minimap;
        return;
      };
      index += 1;
    };
  }

  private final func WithColor(property: inkStyleProperty, color: HDRColor) -> inkStyleProperty {
    let result: inkStyleProperty;
    result.propertyPath = property.propertyPath;
    result.value = ToVariant(color);
    return result;
  }
}
