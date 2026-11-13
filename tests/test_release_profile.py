"""Alpha.9 distribution contracts: observer present but writes and Clone compiled out."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseProfileTests(unittest.TestCase):
    def test_release_version_and_notes_agree(self):
        header = (ROOT / "include/dreamui/control_hub.hpp").read_text(encoding="utf-8")
        workflow = (ROOT / ".github/workflows/release-testing.yml").read_text(encoding="utf-8")
        testing = (ROOT / "docs/TESTING.md").read_text(encoding="utf-8")
        match = re.search(r'kDreamUIVersion\s*=\s*"(v[\d.]+-alpha\.\d+)"', header)
        self.assertIsNotNone(match)
        version = match.group(1)
        self.assertEqual(version, "v0.1.0-alpha.9")
        self.assertIn(f"|| '{version}'", workflow)
        self.assertIn(version, testing)

    def test_alpha9_release_compiles_only_observe_stage(self):
        workflow = (ROOT / ".github/workflows/release-testing.yml").read_text(encoding="utf-8")
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        stage = (ROOT / "src/win_load_hook.cpp").read_text(encoding="utf-8")
        self.assertIn("-DDREAMUI_EXPERIMENTAL_NATIVE_STAGE=ON", workflow)
        self.assertIn("-DDREAMUI_ENABLE_NATIVE_WRITES=OFF", workflow)
        self.assertIn("-DDREAMUI_ENABLE_NATIVE_CLONE=OFF", workflow)
        for option in ("DREAMUI_ENABLE_NATIVE_WRITES", "DREAMUI_ENABLE_NATIVE_CLONE"):
            self.assertRegex(cmake, rf'option\({option}\s+[\s\S]*? OFF\)')
            self.assertIn(f"#if defined({option})", stage)
        self.assertIn("native-ink-preview.enable", stage)
        self.assertIn("native-ink-write.enable", stage)
        self.assertIn("native-ink-clone.enable", stage)

    def test_package_never_includes_unsafe_markers(self):
        workflow = (ROOT / ".github/workflows/release-testing.yml").read_text(encoding="utf-8")
        self.assertIn('$root/dreamui/dev', workflow)
        self.assertIn('Release ZIP must never contain native opt-in marker files', workflow)
        self.assertIn('Copy-Item "dist/Default.dream"', workflow)
        self.assertIn('Copy-Item "build/Release/DreamUI.asi"', workflow)
        self.assertEqual(workflow.count("- name: Unit tests"), 1)
        self.assertEqual(workflow.count("- name: Publish GitHub prerelease"), 1)
        self.assertEqual(workflow.count("gh release create"), 1)


if __name__ == "__main__":
    unittest.main()
