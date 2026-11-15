"""Local WolvenKit font staging: no private fonts are ever committed here."""
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_typography import FAMILY_ROOT, WEIGHTS, FontProjectError, make_family, stage


def template():
    return {
        "Header": {
            "WolvenKitVersion": "10.0.0-beta.1",
            "WKitJsonVersion": "0.0.9",
            "GameVersion": 2500,
            "ArchiveFileName": "C:\\Project\\source\\archive\\base\\gameplay\\gui\\fonts\\blender\\blender.inkfontfamily",
        },
        "Data": {
            "Version": 195, "BuildVersion": 0,
            "RootChunk": {
                "$type": "inkFontFamilyResource",
                "familyName": "Blender",
                "fontStyles": [
                    {
                        "$type": "inkFontStyle",
                        "font": {
                            "DepotPath": {
                                "$type": "ResourcePath", "$storage": "string",
                                "$value": "base\\gameplay\\gui\\fonts\\blender\\book\\blenderpro-book.fnt"
                            },
                            "Flags": "Default",
                        },
                        "styleName": {
                            "$type": "CName", "$storage": "string", "$value": "Book"
                        }
                    }
                ]
            },
            "EmbeddedFiles": [],
        },
    }


class TypographySourceTests(unittest.TestCase):
    def test_only_three_selected_weights_in_new_family(self):
        family = make_family(template())
        root = family["Data"]["RootChunk"]
        styles = root["fontStyles"]
        self.assertEqual(
            [entry["styleName"]["$value"] for entry in styles],
            ["Medium", "Bold", "Heavy"]
        )
        self.assertEqual(root["familyName"], "DreamUI Blender Pro")
        self.assertTrue(all(
            "dreamui" in entry["font"]["DepotPath"]["$value"]
            for entry in styles
        ))
        self.assertEqual(family["Data"]["Version"], 195)
        self.assertEqual(family["Header"]["GameVersion"], 2500)

    def test_does_not_mutate_game_template(self):
        original = template()
        make_family(original)
        self.assertEqual(original, template())

    def test_rejects_invalid_resource(self):
        with self.assertRaises(FontProjectError):
            make_family({"Data": {"RootChunk": {"$type": "notFont"}}})

    def test_stages_private_fonts_with_no_files_in_repo(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / "project" / "source"
            source.mkdir(parents=True)
            for _, _, seed in WEIGHTS:
                fnt = source / "archive" / seed
                fnt.parent.mkdir(parents=True, exist_ok=True)
                fnt.write_bytes(b"TEST-SEED-ONLY")
            zip_path = root / "licensed-fonts.zip"
            with zipfile.ZipFile(zip_path, "w") as z:
                for weight, filename, _ in WEIGHTS:
                    z.writestr(filename, b"\x00\x01\x00\x00" + (weight.encode() * 300))
            export_path = root / "blender.inkfontfamily.json"
            export_path.write_text(json.dumps(template()), encoding="utf-8")
            results = stage(source, zip_path, export_path)
            self.assertEqual(len(results), 7)
            for path in results:
                self.assertTrue(path.is_file())
                self.assertTrue(path.is_relative_to(source))
            family_file = source / "raw" / FAMILY_ROOT / "dreamui.inkfontfamily.json"
            family = json.loads(family_file.read_text(encoding="utf-8"))
            self.assertEqual(
                [s["styleName"]["$value"] for s in family["Data"]["RootChunk"]["fontStyles"]],
                ["Medium", "Bold", "Heavy"]
            )
            self.assertEqual((source / "raw" / FAMILY_ROOT / "heavy" / "dreamui-heavy.ttf").read_bytes()[:4], b"\x00\x01\x00\x00")
            with self.assertRaises(FontProjectError):
                stage(source, zip_path, export_path)
            self.assertEqual(len(stage(source, zip_path, export_path, True)), 7)

    def test_refuses_missing_seed_fonts(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            zip_path = source / "licensed-fonts.zip"
            with zipfile.ZipFile(zip_path, "w") as z:
                pass
            export_path = source / "template.json"
            export_path.write_text(json.dumps(template()), encoding="utf-8")
            with self.assertRaisesRegex(FontProjectError, "Add these vanilla"):
                stage(source, zip_path, export_path)


if __name__ == "__main__":
    unittest.main()
