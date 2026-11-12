import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class RepositoryConfigTests(unittest.TestCase):
    def test_label_catalog_is_unique_and_valid(self):
        defs = json.loads((ROOT / ".github" / "labels.json").read_text(encoding="utf-8"))
        names = [entry["name"] for entry in defs]
        self.assertEqual(len(names), len(set(names)))
        self.assertGreaterEqual(len(names), 10)
        for entry in defs:
            self.assertTrue(entry["name"].strip())
            self.assertTrue(entry["description"].strip())
            self.assertRegex(entry["color"], r"^[0-9A-Fa-f]{6}$")

    def test_pr_labeler_uses_defined_labels(self):
        names = {entry["name"] for entry in json.loads(
            (ROOT / ".github" / "labels.json").read_text(encoding="utf-8"))}
        config = (ROOT / ".github" / "labeler.yml").read_text(encoding="utf-8")
        assigned = re.findall(r'^"([^"]+)":', config, flags=re.MULTILINE)
        self.assertTrue(assigned)
        self.assertTrue(set(assigned).issubset(names))

if __name__ == "__main__":
    unittest.main()
