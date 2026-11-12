import json
import shutil
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from dreamtool import build, validate

ROOT = Path(__file__).resolve().parents[1]

class PackageTests(unittest.TestCase):
    def test_default_is_valid_and_packs(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "Default.dream"
            build(ROOT / "themes" / "Default", path)
            with zipfile.ZipFile(path) as file:
                self.assertEqual(file.namelist(), ["manifest.json", "theme/colors.json"])
                self.assertTrue(all(info.compress_type == 0 for info in file.infolist()))
                self.assertEqual(json.loads(file.read("manifest.json"))["schemaVersion"], 1)

    def test_invalid_color_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp) / "theme"
            shutil.copytree(ROOT / "themes" / "Default", folder)
            colors = folder / "theme" / "colors.json"
            value = json.loads(colors.read_text())
            value["text"]["primary"] = "not a color"
            colors.write_text(json.dumps(value))
            with self.assertRaises(ValueError):
                validate(folder)

    def test_unsafe_id_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp) / "theme"
            shutil.copytree(ROOT / "themes" / "Default", folder)
            manifest = folder / "manifest.json"
            value = json.loads(manifest.read_text())
            value["id"] = "../danger"
            manifest.write_text(json.dumps(value))
            with self.assertRaises(ValueError):
                validate(folder)

if __name__ == "__main__":
    unittest.main()
