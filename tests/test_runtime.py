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
