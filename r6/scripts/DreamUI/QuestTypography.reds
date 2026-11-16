module DreamUI

// The font resources are supplied by the WolvenKit-built DreamUI.archive,
// not by this script. This patch only selects the two families per widget.
public abstract class DreamUIQuestTypography {
  public static final func HeadingFont() -> String {
    return "base\\gameplay\\gui\\fonts\\dreamui\\blender.inkfontfamily";
  }

  public static final func BodyFont() -> String {
    return "base\\gameplay\\gui\\fonts\\dreamui\\inter.inkfontfamily";
  }
}

// Quest title: Blender Pro Heavy. The game's quest state and color remain intact.
@wrapMethod(QuestTrackerGameController)
protected cb func OnInitialize() -> Bool {
  let result: Bool = wrappedMethod();

  if IsDefined(inkTextRef.Get(this.m_QuestTitle)) {
    inkTextRef.SetFontFamily(
      this.m_QuestTitle,
      DreamUIQuestTypography.HeadingFont(),
      n"Heavy"
    );
  };

  return result;
}

// Each objective is spawned separately; give it Inter Medium.
@wrapMethod(QuestTrackerObjectiveLogicController)
protected cb func OnInitialize() -> Bool {
  let result: Bool = wrappedMethod();

  if IsDefined(inkTextRef.Get(this.m_objectiveTitle)) {
    inkTextRef.SetFontFamily(
      this.m_objectiveTitle,
      DreamUIQuestTypography.BodyFont(),
      n"Medium"
    );
  };

  return result;
}
