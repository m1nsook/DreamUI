module DreamUI

// Dialogue colors are local to dialogue choices. They do not change
// MainColors entries used by the HUD, minimap, inventory or subtitles.
public abstract class DreamUIDialogueColors {
  public static final func NormalOption() -> HDRColor {
    return HDRColor(0.78, 1.00, 0.94, 1.0);
  }

  public static final func SelectedOption() -> HDRColor {
    // Selected text needs to contrast with the highlighted choice background.
    return HDRColor(0.055, 0.070, 0.080, 1.0);
  }

  public static final func QuestOption() -> HDRColor {
    return HDRColor(1.0, 0.73, 0.34, 1.0);
  }

  public static final func PossessedOption() -> HDRColor {
    return HDRColor(0.81, 0.65, 1.10, 1.0);
  }

  public static final func LockedOption() -> HDRColor {
    return HDRColor(0.48, 0.53, 0.57, 1.0);
  }

  public static final func OptionMarker() -> HDRColor {
    return HDRColor(1.10, 0.84, 0.45, 1.0);
  }
}

// This exact game controller provides choice type, selected state and text
// widget references. Keep the original state, animations, and caption logic.
@wrapMethod(DialogChoiceLogicController)
private final func UpdateColors() -> Void {
  wrappedMethod();

  let textColor: HDRColor = DreamUIDialogueColors.NormalOption();

  if this.m_isPhoneLockActive {
    textColor = DreamUIDialogueColors.LockedOption();
  } else if this.m_isSelected {
    textColor = DreamUIDialogueColors.SelectedOption();
  } else if ChoiceTypeWrapper.IsType(this.m_type, gameinteractionsChoiceType.QuestImportant) {
    textColor = DreamUIDialogueColors.QuestOption();
  } else if ChoiceTypeWrapper.IsType(this.m_type, gameinteractionsChoiceType.PossessedDialog) {
    textColor = DreamUIDialogueColors.PossessedOption();
  };

  if IsDefined(inkTextRef.Get(this.m_ActiveTextRef)) {
    inkTextRef.SetTintColor(this.m_ActiveTextRef, textColor);
  };

  if IsDefined(inkTextRef.Get(this.m_tagTextRef)) {
    inkTextRef.SetTintColor(this.m_tagTextRef, textColor);
  };

  // The vanilla controller still owns visibility, selection and icon states.
  if IsDefined(inkWidgetRef.Get(this.m_VerticalLineWidget)) {
    inkWidgetRef.SetTintColor(this.m_VerticalLineWidget, DreamUIDialogueColors.OptionMarker());
  };
}
