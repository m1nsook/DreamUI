#!/usr/bin/env python3
"""Design-data integrity check; no game runtime is involved."""
import json
import re
import unittest
from pathlib import Path

PALETTE = Path(__file__).resolve().parents[1] / "design" / "palette.json"

class PaletteDesignTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(PALETTE.read_text(encoding="utf-8"))

    def test_default_is_available(self):
        self.assertIn(self.data["defaultAccent"], self.data["accents"])

    def test_base_palette_and_accents(self):
        self.assertEqual(len(self.data["base"]), 6)
        self.assertEqual(set(self.data["accents"]), {"amber", "pink", "cyan", "lavender"})
        for color in self.data["base"].values():
            self.assertRegex(color, r"^#[0-9A-Fa-f]{6}$")
        for accent in self.data["accents"].values():
            self.assertEqual({k for k in accent if k in {"light", "primary", "dark"}}, {"light", "primary", "dark"})
            for tone in ("light", "primary", "dark"):
                self.assertRegex(accent[tone], r"^#[0-9A-Fa-f]{6}$")

    def test_semantic_roles_not_overwritten_by_accent_roles(self):
        rules = self.data["rules"]
        self.assertTrue(set(rules["accentRoles"]).isdisjoint(rules["reservedSemanticRoles"]))

if __name__ == "__main__":
    unittest.main()
