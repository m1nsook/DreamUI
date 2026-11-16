"""Static checks for the two-font quest tracker integration.

These do NOT substitute for a compiled redscript/game test.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "r6/scripts/DreamUI/QuestTypography.reds"

class QuestTypographyTests(unittest.TestCase):
    def test_two_distinct_family_depot_paths(self):
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertIn(r"blender.inkfontfamily", source)
        self.assertIn(r"inter.inkfontfamily", source)
        self.assertIn('DreamUIQuestTypography.HeadingFont()', source)
        self.assertIn('DreamUIQuestTypography.BodyFont()', source)

    def test_heading_and_objective_are_independent(self):
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertIn("@wrapMethod(QuestTrackerGameController)", source)
        self.assertIn("@wrapMethod(QuestTrackerObjectiveLogicController)", source)
        self.assertIn("inkTextRef.SetFontFamily(", source)
        self.assertEqual(source.count("inkTextRef.SetFontFamily("), 2)
        self.assertIn('n"Heavy"', source)
        self.assertIn('n"Medium"', source)
        self.assertIn("this.m_QuestTitle", source)
        self.assertIn("this.m_objectiveTitle", source)
        self.assertEqual(source.count("wrappedMethod()"), 2)
        self.assertEqual(source.count("return result;"), 2)

    def test_no_global_font_replacement_or_book(self):
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertNotIn("Book", source)
        self.assertNotIn("MainColors.", source)
        self.assertNotIn("SetFontSize(", source)
        self.assertNotIn("SetTintColor(", source)

if __name__ == "__main__":
    unittest.main()
