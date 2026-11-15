#!/usr/bin/env python3
"""Patch WolvenKit-exported fullscreen composition JSON for DreamUI.

Input must be a legitimate local, WolvenKit-exported game resource. This tool
modifies only two in-game menu states. It does not cook .archive files.
"""
from __future__ import annotations
import argparse
import copy
import json
from pathlib import Path

TARGET_STATES = frozenset({"inkInGameMenuState", "inkInGameMenuStateSolid"})
# Design-stage candidates. Actual rendering must be verified in game.
PRESETS = {
    "balanced": {
        "backgroundBlurRadius": 0.014,
        "blurredRenderFactor": 0.64,
        "blurredRenderSaturation": 0.72,
        "vignetteIntensity": 0.40,
        "vignetteStart": 0.30,
        "vignetteEnd": 0.95,
    },
    "high-contrast": {
        "backgroundBlurRadius": 0.018,
        "blurredRenderFactor": 0.51,
        "blurredRenderSaturation": 0.61,
        "vignetteIntensity": 0.53,
        "vignetteStart": 0.25,
        "vignetteEnd": 0.94,
    },
}

class InvalidComposition(ValueError):
    """Input does not contain the expected WolvenKit composition resource."""

def patch_composition(original: dict, strength: str = "balanced") -> dict:
    if strength not in PRESETS:
        raise ValueError(f"Unknown preset: {strength}")
    if not isinstance(original, dict):
        raise InvalidComposition("Input must be a JSON object")
    try:
        root = original["Data"]["RootChunk"]
        assert root["$type"] == "inkFullscreenCompositionResource"
        presets = root["compositionPresets"]
        assert isinstance(presets, list)
    except (AssertionError, KeyError, TypeError) as exc:
        raise InvalidComposition("Expected a WolvenKit inkFullscreenCompositionResource JSON export") from exc

    patched = copy.deepcopy(original)
    found: set[str] = set()
    for item in patched["Data"]["RootChunk"]["compositionPresets"]:
        state = item.get("stateName", {}).get("$value")
        if state not in TARGET_STATES:
            continue
        if state in found:
            raise InvalidComposition(f"Duplicate composition state: {state}")
        found.add(state)
        shader = item.get("shaderParams")
        if not isinstance(shader, dict):
            raise InvalidComposition(f"Missing shader settings for {state}")
        missing = PRESETS[strength].keys() - shader.keys()
        if missing:
            raise InvalidComposition(f"Missing shader settings for {state}: {sorted(missing)}")
        # UI rendering and menu-background selection are deliberately preserved.
        shader.update(PRESETS[strength])
    if found != TARGET_STATES:
        raise InvalidComposition(f"Missing target states: {sorted(TARGET_STATES - found)}")
    return patched

def main() -> None:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("input", type=Path, help="WolvenKit-exported composition_params.inkfullscreencomposition.json")
    cli.add_argument("output", type=Path, help="output JSON at the original game resource path")
    cli.add_argument("--strength", choices=tuple(PRESETS), default="balanced")
    args = cli.parse_args()
    if args.input.resolve() == args.output.resolve():
        cli.error("Input and output must differ, so the source is never overwritten")
    source = json.loads(args.input.read_text(encoding="utf-8-sig"))
    result = patch_composition(source, args.strength)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Wrote {args.strength} composition: {args.output}")

if __name__ == "__main__":
    main()
