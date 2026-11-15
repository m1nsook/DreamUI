"""Static integrity checks for the first DreamUI script-only candidate.

These tests cannot substitute for redscript compilation and an in-game load.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "r6" / "scripts" / "DreamUI"


class RuntimeSourceTests(unittest.TestCase):
    def read(self, filename):
        return (SCRIPTS / filename).read_text(encoding="utf-8")

    def test_palette_has_102_distinct_entries(self):
        palette = self.read("Palette.reds")
        before, after = palette.split("if johnny {", 1)
        johnny, main = after.split("return null;", 1)
        extract = lambda s: re.findall(r'if Equals\(path, n"MainColors\.([^"]+)"\)', s)
        self.assertEqual(len(extract(johnny)), 10)
        self.assertEqual(len(set(extract(johnny))), 10)
        self.assertEqual(len(extract(main)), 92)
        self.assertEqual(len(set(extract(main))), 92)
        self.assertIn("return DreamUIAccent.Amber;", palette)

    def test_no_editor_or_storage_dependencies(self):
        scripts = "\n".join(file.read_text(encoding="utf-8") for file in SCRIPTS.glob("*.reds"))
        for forbidden in ("RedData", "RedFileSystem", "ModSettings",
                          "FileSystemStorage", "HUDPainter", "HudPainter",
                          "MenuScenario_", "CustomColorPicker"):
            self.assertNotIn(forbidden, scripts)

    def test_resource_paths_use_double_backslashes(self):
        scripts = "\n".join(file.read_text(encoding="utf-8") for file in SCRIPTS.glob("*.reds"))
        paths = [path for path in re.findall(r'r"([^"]+)"', scripts)
                 if path.startswith("base")]
        self.assertEqual(len(paths), 7)
        for path in paths:
            self.assertFalse(re.search(r'(?<!\\)\\(?!\\)', path), path)

    def test_minimap_and_core_resources_are_wired(self):
        runtime = self.read("Runtime.reds")
        self.assertIn('r"base\\\\gameplay\\\\gui\\\\common\\\\main_colors.inkstyle"', runtime)
        self.assertIn('r"base\\\\gameplay\\\\gui\\\\common\\\\themes\\\\main_colors_johnny.inkstyle"', runtime)
        for key in ("MiniMapStyle.RoadColor", "MiniMapStyle.FloorColor",
                    "MiniMapStyle.ExteriorWallColor", "MiniMapStyle.InteriorWallColor"):
            self.assertIn(key, runtime)

    def test_minimap_colors_are_independent_of_main_colors(self):
        runtime = self.read("Runtime.reds")
        roles = self.read("ElementColors.reds")
        self.assertNotIn("WithReference(", runtime)
        self.assertIn("ToVariant(color)", runtime)
        for role in ("MinimapRoad", "MinimapFloor",
                     "MinimapExteriorWall", "MinimapInteriorWall"):
            self.assertIn(f"DreamUIElementColors.{role}()", runtime)
            self.assertIn(f"func {role}()", roles)
        self.assertEqual(runtime.count("this.WithColor(property,"), 4)
        self.assertNotIn("MainColors.", runtime.split("private final func RebindMinimap", 1)[1])
        self.assertNotIn("DreamUIPalette.Resolve(", roles)

    def test_dialogue_choice_uses_independent_color_roles(self):
        source = self.read("DialogueColors.reds")
        self.assertIn("@wrapMethod(DialogChoiceLogicController)", source)
        self.assertIn("private final func UpdateColors()", source)
        self.assertIn("wrappedMethod();", source)
        for role in ("NormalOption", "SelectedOption", "QuestOption",
                     "PossessedOption", "LockedOption", "OptionMarker"):
            self.assertIn(f"func {role}()", source)
            self.assertIn(f"DreamUIDialogueColors.{role}()", source)
        self.assertIn("this.m_isPhoneLockActive", source)
        self.assertIn("this.m_isSelected", source)
        self.assertIn("gameinteractionsChoiceType.QuestImportant", source)
        self.assertIn("gameinteractionsChoiceType.PossessedDialog", source)
        self.assertIn("inkTextRef.SetTintColor(this.m_ActiveTextRef", source)
        self.assertIn("inkTextRef.SetTintColor(this.m_tagTextRef", source)
        self.assertIn("IsDefined(inkTextRef.Get(this.m_ActiveTextRef))", source)
        self.assertNotIn("MainColors.", source)
        self.assertNotIn("DialogChoiceLogicController.OnInitialize", source)

    def test_sms_received_sent_and_replies_have_independent_colors(self):
        source = self.read("MessageColors.reds")
        self.assertIn("@wrapMethod(MessangerItemRenderer)", source)
        self.assertIn("@wrapMethod(MessangerReplyItemRenderer)", source)
        self.assertIn("func SetMessageView(", source)
        self.assertIn("func AnimateSelection()", source)
        self.assertEqual(source.count("wrappedMethod("), 2)
        for role in ("ReceivedText", "SentText", "ReplyDefault",
                     "ReplySelected", "ReplyQuest", "ReplyDisabled"):
            self.assertIn(f"func {role}()", source)
        self.assertIn("this.m_labelPathRef", source)
        self.assertIn("MessageViewType.Received", source)
        self.assertIn("this.m_isActive", source)
        self.assertIn("this.m_selectedState", source)
        self.assertNotIn("MainColors.", source)

    def test_sms_selected_reply_cannot_return_to_dark_text(self):
        # The vanilla selected reply bubble is a medium-gray surface. Guard
        # against repeating the near-black-on-gray contrast regression.
        source = self.read("MessageColors.reds")
        match = re.search(
            r"func ReplySelected\(\) -> HDRColor \{\s*"
            r"return HDRColor\(([^)]*)\);",
            source)
        self.assertIsNotNone(match)
        channels = [float(value.strip()) for value in match.group(1).split(",")]
        self.assertEqual(len(channels), 4)
        self.assertTrue(all(channel >= 0.90 for channel in channels[:3]))
        self.assertEqual(channels[3], 1.0)

    def test_sms_body_text_not_force_tinted_cyan(self):
        source = self.read("MessageColors.reds")
        for role in ("SentText", "ReceivedText", "ReplyDefault"):
            match = re.search(
                rf"func {role}\(\) -> HDRColor \{{\s*"
                rf"return HDRColor\(([^)]*)\);",
                source)
            self.assertIsNotNone(match)
            channels = [float(value.strip()) for value in match.group(1).split(",")]
            self.assertTrue(all(channel >= 0.90 for channel in channels[:3]))

    def test_spoken_subtitles_have_separate_body_and_speaker_colors(self):
        source = self.read("SubtitleColors.reds")
        self.assertIn("@wrapMethod(SubtitleLineLogicController)", source)
        self.assertIn("func SetLineData(const lineData: script_ref<scnDialogLineData>)", source)
        self.assertIn("wrappedMethod(lineData)", source)
        for role in ("SpokenText", "SpeakerName", "RadioText", "RadioSpeaker"):
            self.assertIn(f"func {role}()", source)
        for widget in ("m_subtitleWidget", "m_speakerNameWidget",
                       "m_radioSubtitle", "m_radioSpeaker"):
            self.assertIn(f"inkTextRef.SetTintColor(this.{widget}", source)
        self.assertIn("@wrapMethod(ChatterLineLogicController)", source)
        self.assertIn("func AmbientChatter()", source)
        self.assertIn("inkTextRef.SetTintColor(this.m_text_normal", source)
        self.assertIn("inkTextRef.SetTintColor(this.m_text_wide", source)
        self.assertEqual(source.count("wrappedMethod(lineData)"), 2)
        self.assertNotIn("SetFontSize(", source)
        self.assertNotIn("MainColors.", source)

    def test_widget_binding_has_null_guards(self):
        bindings = self.read("WidgetBindings.reds")
        self.assertIn("if !IsDefined(widget)", bindings)
        self.assertEqual(bindings.count("@wrapMethod("), 3)
        self.assertEqual(bindings.count("DreamUIWidgetBinding.BindTint("), 13)

    def test_world_map_uses_common_palette(self):
        map_script = self.read("WorldMap.reds")
        self.assertIn("DreamUIPalette.Resolve(", map_script)
        self.assertIn('n"BaseColorScale"', map_script)
        self.assertIn('n"EdgeColor"', map_script)
        self.assertIn('n"Color"', map_script)
        self.assertNotIn("hud_painter", map_script.lower())


if __name__ == "__main__":
    unittest.main()
