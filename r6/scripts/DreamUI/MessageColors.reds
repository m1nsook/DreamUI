module DreamUI

// The Phone Messages list uses its own item renderers, independently of both
// open-world dialogue choices and the main Ink color sheet.
public abstract class DreamUIMessageColors {
  public static final func ReceivedText() -> HDRColor {
    return HDRColor(0.97, 0.96, 0.90, 1.0);
  }

  public static final func SentText() -> HDRColor {
    return HDRColor(0.76, 1.00, 1.10, 1.0);
  }

  public static final func ReplyDefault() -> HDRColor {
    return HDRColor(0.92, 0.97, 1.03, 1.0);
  }

  public static final func ReplySelected() -> HDRColor {
    return HDRColor(0.09, 0.13, 0.16, 1.0);
  }

  public static final func ReplyQuest() -> HDRColor {
    return HDRColor(1.10, 0.82, 0.47, 1.0);
  }

  public static final func ReplyDisabled() -> HDRColor {
    return HDRColor(0.56, 0.60, 0.65, 1.0);
  }
}

// SetMessageView is called after the message text and bubble layout are set.
// Reapply the SMS-only text color last so shared styles do not own the tint.
@wrapMethod(MessangerItemRenderer)
private final func SetMessageView(const txt: script_ref<String>, type: MessageViewType, const contactName: script_ref<String>) -> Void {
  wrappedMethod(txt, type, contactName);

  if !IsDefined(inkTextRef.Get(this.m_labelPathRef)) { return; };

  if Equals(type, MessageViewType.Received) {
    inkTextRef.SetTintColor(this.m_labelPathRef, DreamUIMessageColors.ReceivedText());
  } else {
    inkTextRef.SetTintColor(this.m_labelPathRef, DreamUIMessageColors.SentText());
  };
}

// SMS responses are distinct from dialogue choices in the 3D world.
// The game chooses selection/quest/disabled states first; DreamUI only
// adjusts the label, preserving navigation, state, and animations.
@wrapMethod(MessangerReplyItemRenderer)
public func AnimateSelection() -> Void {
  wrappedMethod();

  if !IsDefined(inkTextRef.Get(this.m_labelPathRef)) { return; };

  let color: HDRColor = DreamUIMessageColors.ReplyDefault();
  if !this.m_isActive {
    color = DreamUIMessageColors.ReplyDisabled();
  } else if this.m_selectedState {
    color = DreamUIMessageColors.ReplySelected();
  } else if this.m_isQuestImportant {
    color = DreamUIMessageColors.ReplyQuest();
  };

  inkTextRef.SetTintColor(this.m_labelPathRef, color);
}
