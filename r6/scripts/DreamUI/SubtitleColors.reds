module DreamUI

// Subtitle text and speaker labels have dedicated color roles.
// Keep all game accessibility sizing, radio display rules, and animations.
public abstract class DreamUISubtitleColors {
  public static final func SpokenText() -> HDRColor {
    return HDRColor(0.99, 0.98, 0.92, 1.0);
  }

  public static final func SpeakerName() -> HDRColor {
    return HDRColor(1.10, 0.83, 0.47, 1.0);
  }

  public static final func RadioText() -> HDRColor {
    return HDRColor(0.79, 0.97, 1.05, 1.0);
  }

  public static final func RadioSpeaker() -> HDRColor {
    return HDRColor(0.98, 0.82, 0.56, 1.0);
  }
}

// This callback is used for authored cinematic and spoken lines (including
// the bottom-of-screen dialogue shown during conversations).
@wrapMethod(SubtitleLineLogicController)
public func SetLineData(const lineData: script_ref<scnDialogLineData>) -> Void {
  wrappedMethod(lineData);

  if IsDefined(inkTextRef.Get(this.m_subtitleWidget)) {
    inkTextRef.SetTintColor(this.m_subtitleWidget, DreamUISubtitleColors.SpokenText());
  };
  if IsDefined(inkTextRef.Get(this.m_speakerNameWidget)) {
    inkTextRef.SetTintColor(this.m_speakerNameWidget, DreamUISubtitleColors.SpeakerName());
  };
  if IsDefined(inkTextRef.Get(this.m_radioSubtitle)) {
    inkTextRef.SetTintColor(this.m_radioSubtitle, DreamUISubtitleColors.RadioText());
  };
  if IsDefined(inkTextRef.Get(this.m_radioSpeaker)) {
    inkTextRef.SetTintColor(this.m_radioSpeaker, DreamUISubtitleColors.RadioSpeaker());
  };
}
