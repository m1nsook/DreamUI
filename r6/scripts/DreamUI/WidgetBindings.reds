module DreamUI

// Widgets with fixed tint colors do not always react to changes in main_colors.
// Bind the relevant widgets to the same palette as the HUD and menus.
public abstract class DreamUIWidgetBinding {
  public static final func BindTint(widget: ref<inkWidget>, color: CName) -> Void {
    if !IsDefined(widget) { return; };

    widget.SetStyle(r"base\\gameplay\\gui\\common\\main_colors.inkstyle");
    widget.BindProperty(n"tintColor", color);
  }
}

@wrapMethod(healthbarWidgetGameController)
protected cb func OnInitialize() -> Bool {
  wrappedMethod();

  let root: ref<inkCompoundWidget> = this.GetRootCompoundWidget();
  if IsDefined(root) {
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"buffsHolder/barsLayout/xpBar/exp_bar_cotianer/exp_bar_full"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"buffsHolder/barsLayout/xpBar/flugg_stripes_exp/bar"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"buffsHolder/barsLayout/xpBar/exp_bar_cotianer/textpos/xp_text/exp_value_plus"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"buffsHolder/barsLayout/xpBar/exp_bar_cotianer/textpos/xp_text/exp_value"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"buffsHolder/barsLayout/xpBar/exp_bar_cotianer/textpos/xp_text/exp_value_label"), n"MainColors.NPC_Chatter");
  };
}

@wrapMethod(MenuHubGameController)
protected cb func OnInitialize() -> Bool {
  wrappedMethod();

  let root: ref<inkCompoundWidget> = this.GetRootCompoundWidget();
  if IsDefined(root) {
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"topPanels/holder/left_holder/level_holder/text_holder/expBarWrapper/bars"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"topPanels/holder/left_holder/level_holder/text_holder/expBarWrapper/barWrapper/bar"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"topPanels/holder/left_holder/level_holder/text_holder/expBarWrapper/barWrapper/thumb"), n"MainColors.NPC_Chatter");
  };
}

@wrapMethod(NewPerksCategoriesGameController)
protected cb func OnInitialize() -> Bool {
  wrappedMethod();

  let root: ref<inkCompoundWidget> = this.GetRootCompoundWidget();
  if IsDefined(root) {
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"attributes_canvas/body/attribute_button/container/wrapper/itemWrapper/itemContentWrapper/detailsWrapper/levelWrapper/value"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"attributes_canvas/reflexes/attribute_button/container/wrapper/itemWrapper/itemContentWrapper/detailsWrapper/levelWrapper/value"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"attributes_canvas/technical_ability/attribute_button/container/wrapper/itemWrapper/itemContentWrapper/detailsWrapper/levelWrapper/value"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"attributes_canvas/intelligence/attribute_button/container/wrapper/itemWrapper/itemContentWrapper/detailsWrapper/levelWrapper/value"), n"MainColors.NPC_Chatter");
    DreamUIWidgetBinding.BindTint(root.GetWidgetByPathName(n"attributes_canvas/cool/attribute_button/container/wrapper/itemWrapper/itemContentWrapper/detailsWrapper/levelWrapper/value"), n"MainColors.NPC_Chatter");
  };
}
