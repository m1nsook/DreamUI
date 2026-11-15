"""Offline safety tests for dark menu parameter patching."""
import copy
import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from build_dark_menu import TARGET_STATES, PRESETS, InvalidComposition, patch_composition

def fixture():
    states = ["inkGameState", "inkPhotoModeState", "inkInitialLoadingState",
              "inkInGameMenuState", "inkInGameMenuStateSolid",
              "inkPauseMenuState", "inkVendorState"]
    return {
        "Header": {"WolvenKitVersion": "10.0.0", "GameVersion": 2500},
        "Data": {"RootChunk": {
            "$type": "inkFullscreenCompositionResource",
            "compositionPresets": [
                {"stateName": {"$value": state}, "shaderParams": {
                    "backgroundBlurRadius": 0, "blurredRenderFactor": 1,
                    "blurredRenderSaturation": 1, "vignetteIntensity": 0,
                    "vignetteStart": 0, "vignetteEnd": 0,
                    "uiFactor": 1, "mainRenderFactor": 0,
                }, "useBackgroundTexture": 0, "other": [1, 2]}
                for state in states]
        }}
    }

class CompositionTests(unittest.TestCase):
    def test_only_two_menu_states_are_changed(self):
        sample = fixture()
        for strength, expected in PRESETS.items():
            with self.subTest(strength=strength):
                output = patch_composition(sample, strength)
                for original, modified in zip(
                        sample["Data"]["RootChunk"]["compositionPresets"],
                        output["Data"]["RootChunk"]["compositionPresets"]):
                    state = original["stateName"]["$value"]
                    if state in TARGET_STATES:
                        for name, value in expected.items():
                            self.assertEqual(modified["shaderParams"][name], value)
                        self.assertEqual(modified["shaderParams"]["uiFactor"], 1)
                        self.assertEqual(modified["useBackgroundTexture"], 0)
                    else:
                        self.assertEqual(original, modified)
                self.assertEqual(sample, fixture())

    def test_idempotent(self):
        result = patch_composition(fixture())
        self.assertEqual(patch_composition(result), result)

    def test_no_missing_states(self):
        bad = fixture()
        bad["Data"]["RootChunk"]["compositionPresets"].pop(4)
        with self.assertRaises(InvalidComposition):
            patch_composition(bad)

    def test_unknown_resource_refused(self):
        with self.assertRaises(InvalidComposition):
            patch_composition({})

    def test_unknown_strength_refused(self):
        with self.assertRaises(ValueError):
            patch_composition(fixture(), "extreme")

    def test_invalid_shader_rejected(self):
        bad = fixture()
        del bad["Data"]["RootChunk"]["compositionPresets"][3]["shaderParams"]["vignetteStart"]
        with self.assertRaises(InvalidComposition):
            patch_composition(bad)

if __name__ == "__main__":
    unittest.main()
