"""Static tests for the first DreamUI script-only candidate."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "r6" / "scripts" / "DreamUI"

class RuntimeSourceTests(unittest.TestCase):
    def test_palette_coverage(self):
        content = (SCRIPTS / "Palette.reds").read_text(encoding="utf-8")
        main = content.split("if johnny {", 1)[1].split("return null;", 1)[1]
        johnny = content.split("if johnny {", 1)[1].split("return null;", 1)[0]
        self.assertEqual(len(re.findall(r'if Equals\(path, n"MainColors\.', main)), 92)
        self.assertEqual(len(re.findall(r'if Equals\(path, n"MainColors\.', johnny)), 10)
        self.assertIn("return DreamUIAccent.Amber;", content)
    def test_no_preset_dependencies(self):
        content = "\n".join(x.read_text(encoding="utf-8") for x in SCRIPTS.glob("*.reds"))
        for marker in ("RedData", "RedFileSystem", "ModSettings", "FileSystemStorage", "HudPainter"):
            self.assertNotIn(marker, content)
    def test_services_registered(self):
        source = (SCRIPTS / "Runtime.reds").read_text(encoding="utf-8")
        self.assertIn("Resource/Loaded", source)
        self.assertIn("main_colors.inkstyle", source)
        self.assertIn("main_colors_johnny.inkstyle", source)
        self.assertIn("MiniMapStyle.RoadColor", source)

if __name__ == "__main__":
    unittest.main()
