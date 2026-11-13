#!/usr/bin/env python3
"""Export a .dream palette to an OPTIONAL HUD Painter JSON preset.

This is a compatibility tool, not a substitute for DreamUI's standalone
native renderer. It independently generates data in HUD Painter's documented
JSON preset schema; no mod scripts/assets are copied or redistributed.

Usage:
  python tools/hudpainter_export.py themes/Default \
      "/path/to/r6/storages/HUDPainter/DEFAULT.json" \
      "/path/to/r6/storages/HUDPainter/DreamUI Default.json"
"""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
import zipfile
from copy import deepcopy
from pathlib import Path

from dreamtool import validate as validate_dream_folder

MAX_BASELINE_BYTES = 512 * 1024
MAX_DREAM_BYTES = 16 * 1024 * 1024
REQUIRED_KEYS = ("schemaVersion", "id", "name", "version", "type")

# Deliberately small, verified-to-exist palette targets. These are native
# inkStyleProperty paths, not CSS selectors or widget IDs.
PROPERTY_BINDINGS = {
    "MainColors.Red": ("text", "primary"),
    "MainColors.PanelBlue": ("text", "secondary"),
    "MainColors.ActiveYellow": ("selection", "active"),
}


def _load_json_bytes(content: bytes, label: str) -> dict:
    if len(content) > MAX_BASELINE_BYTES:
        raise ValueError(f"{label}: JSON member larger than {MAX_BASELINE_BYTES} bytes")
    try:
        obj = json.loads(content.decode("utf-8-sig"))
    except (ValueError, UnicodeError) as exc:
        raise ValueError(f"{label}: invalid JSON or UTF-8") from exc
    if not isinstance(obj, dict):
        raise ValueError(f"{label}: expected JSON object")
    return obj


def _read_dream(path: Path) -> tuple[dict, dict]:
    if path.is_dir():
        validate_dream_folder(path)
        metadata = _load_json_bytes((path / "manifest.json").read_bytes(), "manifest")
        colors = _load_json_bytes((path / "theme/colors.json").read_bytes(), "colors")
    else:
        if path.suffix.lower() != ".dream" or path.stat().st_size > MAX_DREAM_BYTES:
            raise ValueError("source must be a .dream archive (up to 16 MiB) or theme folder")
        with zipfile.ZipFile(path, "r") as archive:
            names: set[str] = set()
            size = 0
            for info in archive.infolist():
                if info.is_dir():
                    continue
                name = info.filename
                if (
                    name.startswith("/") or "\\" in name or
                    any(p in ("", ".", "..") for p in name.split("/")) or
                    info.compress_type != zipfile.ZIP_STORED or
                    ((info.external_attr >> 16) & 0o170000) == 0o120000 or
                    name in names
                ):
                    raise ValueError(f"unsafe or unsupported .dream entry: {name}")
                names.add(name)
                size += info.file_size
                if size > MAX_DREAM_BYTES or len(names) > 128:
                    raise ValueError(".dream archive exceeds member/size limit")
            if not {"manifest.json", "theme/colors.json"} <= names:
                raise ValueError(".dream archive missing manifest.json or theme/colors.json")
            metadata = _load_json_bytes(archive.read("manifest.json"), "manifest")
            colors = _load_json_bytes(archive.read("theme/colors.json"), "colors")
    if any(k not in metadata for k in REQUIRED_KEYS):
        raise ValueError(".dream manifest missing required fields")
    if (
        metadata["schemaVersion"] != 1 or metadata["type"] != "theme" or
        not isinstance(metadata["id"], str) or
        not re.fullmatch(r"[a-z0-9]+(?:[.-][a-z0-9]+)*", metadata["id"]) or
        not isinstance(metadata["name"], str) or not metadata["name"].strip() or
        not isinstance(metadata["version"], str) or
        not re.fullmatch(r"\d+\.\d+\.\d+", metadata["version"])
    ):
        raise ValueError(".dream manifest has invalid schema or identity")
    return metadata, colors


def _color_token(data: dict, group: str, name: str) -> tuple[float, float, float]:
    item = data.get(group)
    raw = item.get(name) if isinstance(item, dict) else None
    if not isinstance(raw, str) or not re.fullmatch(r"#[0-9a-fA-F]{6}", raw):
        raise ValueError(f"theme missing valid {group}.{name} token")
    return tuple(int(raw[i:i+2], 16) / 255.0 for i in (1, 3, 5))


def _validate_baseline(preset: dict) -> dict[str, dict]:
    if "propertiesDefault" not in preset or "propertiesJohnny" not in preset:
        raise ValueError("not a HUD Painter preset: missing default/Johnny property arrays")
    names: dict[str, dict] = {}
    for key in ("propertiesDefault", "propertiesJohnny"):
        entries = preset[key]
        if not isinstance(entries, list) or len(entries) > 256:
            raise ValueError(f"{key}: expected a bounded property array")
        seen: set[str] = set()
        for item in entries:
            if not isinstance(item, dict) or not isinstance(item.get("name"), str):
                raise ValueError(f"{key}: invalid property record")
            name = item["name"]
            if not name.startswith("MainColors.") or len(name) > 128 or name in seen:
                raise ValueError(f"{key}: duplicated/invalid property name")
            seen.add(name)
            for channel in ("red", "green", "blue", "alpha"):
                number = item.get(channel)
                if (
                    type(number) not in (float, int) or
                    not math.isfinite(number) or abs(number) > 65504
                ):
                    raise ValueError(f"{key}/{name}: invalid HDRColor {channel}")
            if key == "propertiesDefault":
                names[name] = item
    if not set(PROPERTY_BINDINGS) <= set(names):
        missing = sorted(set(PROPERTY_BINDINGS) - set(names))
        raise ValueError(f"default preset missing required native properties: {missing}")
    return names


def export_preset(theme: Path, baseline: Path, destination: Path) -> list[str]:
    """Generate a complete HUD Painter-compatible preset, preserving other colors."""
    if destination.resolve() == baseline.resolve():
        raise ValueError("refusing to overwrite the installed vanilla DEFAULT preset")
    _metadata, colors = _read_dream(theme)
    if not baseline.is_file() or baseline.stat().st_size > MAX_BASELINE_BYTES:
        raise ValueError("baseline must be an existing HUD Painter DEFAULT.json (<=512 KiB)")
    original = _load_json_bytes(baseline.read_bytes(), "baseline")
    result = deepcopy(original)
    originals = _validate_baseline(result)

    changes: list[str] = []
    for name, token in PROPERTY_BINDINGS.items():
        property_entry = originals[name]
        red, green, blue = _color_token(colors, *token)
        # Preserve the property's original relative HDR intensity instead of
        # forcing all tokens to be equally bright. Preserve ORIGINAL alpha.
        peak = max(float(property_entry[c]) for c in ("red", "green", "blue"))
        if peak <= 0.0:
            raise ValueError(f"native property {name} has no positive brightness")
        property_entry["red"] = round(red * peak, 7)
        property_entry["green"] = round(green * peak, 7)
        property_entry["blue"] = round(blue * peak, 7)
        changes.append(f"{name} <- {'.'.join(token)} (original HDR peak {peak:.4f})")

    # No overwrites or destructive installation; the user chooses this preset
    # explicitly from HUD Painter's own in-game preset menu.
    if destination.exists():
        raise ValueError("destination already exists; choose a new output filename")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return changes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("theme", type=Path, help="DreamUI source theme folder or .dream archive")
    parser.add_argument("baseline", type=Path, help="your installed HUD Painter DEFAULT.json")
    parser.add_argument("output", type=Path, help="new HUD Painter preset filename (.json)")
    args = parser.parse_args()
    if args.output.suffix.lower() != ".json":
        parser.error("output filename must end with .json")
    try:
        changes = export_preset(args.theme, args.baseline, args.output)
    except (ValueError, OSError, zipfile.BadZipFile, RuntimeError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    print(f"Exported {args.output} ({len(changes)} native Ink palette properties updated):")
    for change in changes:
        print("  " + change)
    print("Select the generated preset inside HUD Painter to apply it. "
          "The standalone DreamUI ASI does not require HUD Painter.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
