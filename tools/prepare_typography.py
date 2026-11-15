#!/usr/bin/env python3
"""Stage DreamUI's licensed three-weight font family for a local WolvenKit project.

Example:
    python tools/prepare_typography.py --source C:\WolvenKit\DreamUI\source \
        --font-zip C:\Private\blender-pro.zip \
        --template C:\WolvenKit\DreamUI\source\raw\base\gameplay\gui\fonts\blender\blender.inkfontfamily.json

Before running, add the three source .fnt files listed in SEED_ASSETS to
the WolvenKit project's source/archive using its Asset Browser. Export the
original Blender .inkfontfamily to JSON to use as --template.

The output includes local licensed TTFs, a new inkfontfamily JSON and seed
cooked .fnt containers ready for local WolvenKit import. None belong in Git.
This command never installs or cooks the assets itself.
"""
from __future__ import annotations

import argparse
import copy
import json
import shutil
import zipfile
from pathlib import Path

FAMILY_ROOT = Path("base/gameplay/gui/fonts/dreamui")
FONT_FAMILY_DEPOT = r"base\gameplay\gui\fonts\dreamui\dreamui.inkfontfamily"
WEIGHTS = (
    ("Medium", "BlenderPro-Medium.ttf",
     "base/gameplay/gui/fonts/raj/raj-medium.fnt"),
    ("Bold", "BlenderPro-Bold.ttf",
     "base/gameplay/gui/fonts/raj/raj-bold.fnt"),
    ("Heavy", "BlenderPro-Heavy.ttf",
     "base/gameplay/gui/fonts/orbitron/orbitron-black.fnt"),
)
SEED_ASSETS = tuple(item[2] for item in WEIGHTS)


class FontProjectError(ValueError):
    pass


def _replace_scalar(value: object, replacement: str, field: str) -> object:
    if isinstance(value, str):
        return replacement
    if isinstance(value, dict) and isinstance(value.get("$value"), str):
        updated = copy.deepcopy(value)
        updated["$value"] = replacement
        return updated
    raise FontProjectError(f"Unsupported WolvenKit {field} representation")


def make_family(exported: dict) -> dict:
    """Clone the exported game's schema, but expose exactly three chosen weights."""
    if not isinstance(exported, dict):
        raise FontProjectError("Expected a WolvenKit JSON object")
    try:
        root = exported["Data"]["RootChunk"]
        if root["$type"] != "inkFontFamilyResource":
            raise FontProjectError("Wrong root resource type")
        old_styles = root["fontStyles"]
        if not isinstance(old_styles, list) or not old_styles:
            raise FontProjectError("The game font family has no font styles")
        schema = next(
            style for style in old_styles
            if isinstance(style, dict) and "styleName" in style
            and isinstance(style.get("font"), dict)
            and "DepotPath" in style["font"]
        )
    except (KeyError, StopIteration, TypeError) as exc:
        raise FontProjectError(
            "Use the WolvenKit JSON export of blender.inkfontfamily"
        ) from exc

    updated = copy.deepcopy(exported)
    output_root = updated["Data"]["RootChunk"]
    output_root["familyName"] = _replace_scalar(
        root["familyName"], "DreamUI Blender Pro", "familyName"
    )
    new_styles = []
    for weight, _, _ in WEIGHTS:
        entry = copy.deepcopy(schema)
        entry["styleName"] = _replace_scalar(
            entry["styleName"], weight, "styleName"
        )
        target_fnt = (
            FAMILY_ROOT / weight.lower() / f"dreamui-{weight.lower()}.fnt"
        ).as_posix().replace("/", "\\")
        entry["font"]["DepotPath"] = _replace_scalar(
            entry["font"]["DepotPath"], target_fnt, "font.DepotPath"
        )
        new_styles.append(entry)
    output_root["fontStyles"] = new_styles

    header = updated.get("Header", {})
    if isinstance(header, dict) and isinstance(header.get("ArchiveFileName"), str):
        source = header["ArchiveFileName"]
        last = source.rfind("\\")
        if last >= 0:
            prefix = source[:last]
            # Preserve the actual project path up to the shared fonts folder.
            marker = prefix.lower().rfind("\\fonts\\")
            if marker >= 0:
                prefix = prefix[:marker]
                header["ArchiveFileName"] = (
                    prefix + r"\fonts\dreamui\dreamui.inkfontfamily"
                )
            else:
                header["ArchiveFileName"] = (
                    source[:last+1] + "dreamui.inkfontfamily"
                )
        else:
            header["ArchiveFileName"] = FONT_FAMILY_DEPOT
    return updated


def _font_from_zip(archive: zipfile.ZipFile, name: str) -> bytes:
    paths = (name, f"blender-pro/{name}")
    selected = next((p for p in paths if p in archive.namelist()), None)
    if selected is None:
        raise FontProjectError(f"Missing licensed font: {name}")
    contents = archive.read(selected)
    if len(contents) < 1000 or contents[:4] not in (
        b"\x00\x01\x00\x00", b"OTTO", b"true"
    ):
        raise FontProjectError(f"Not a valid TrueType/OpenType font: {name}")
    return contents


def stage(source_root: Path, font_zip: Path, template_json: Path,
          overwrite: bool = False) -> list[Path]:
    source_root = source_root.resolve()
    if not source_root.is_dir():
        raise FontProjectError(f"WolvenKit source directory not found: {source_root}")

    missing = [
        source_root / "archive" / Path(path)
        for path in SEED_ASSETS
        if not (source_root / "archive" / Path(path)).is_file()
    ]
    if missing:
        names = "\n".join(str(item) for item in missing)
        raise FontProjectError(
            "Add these vanilla .fnt files to your WolvenKit project first:\n" + names
        )
    source = json.loads(template_json.read_text(encoding="utf-8-sig"))
    family = make_family(source)

    outputs: list[tuple[Path, bytes]] = []
    with zipfile.ZipFile(font_zip) as z:
        for weight, filename, seed in WEIGHTS:
            raw_path = source_root / "raw" / FAMILY_ROOT / weight.lower()
            cooked_path = source_root / "archive" / FAMILY_ROOT / weight.lower()
            base_name = f"dreamui-{weight.lower()}"
            outputs.append((
                raw_path / f"{base_name}.ttf",
                _font_from_zip(z, filename)
            ))
            outputs.append((
                cooked_path / f"{base_name}.fnt",
                (source_root / "archive" / Path(seed)).read_bytes()
            ))
    outputs.append((
        source_root / "raw" / FAMILY_ROOT / "dreamui.inkfontfamily.json",
        (json.dumps(family, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    ))
    if not overwrite:
        already = [path for path, _ in outputs if path.exists()]
        if already:
            raise FontProjectError(
                "Output files already exist; use --overwrite deliberately:\n"
                + "\n".join(str(path) for path in already)
            )
    for path, data in outputs:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    return [path for path, _ in outputs]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True,
                        help="Your WolvenKit project's source directory")
    parser.add_argument("--font-zip", type=Path, required=True,
                        help="The original licensed blender-pro.zip (never commit)")
    parser.add_argument("--template", type=Path, required=True,
                        help="WolvenKit's exported blender.inkfontfamily.json")
    parser.add_argument("--overwrite", action="store_true")
    args = parser.parse_args()
    try:
        files = stage(args.source, args.font_zip, args.template, args.overwrite)
    except (FontProjectError, FileNotFoundError, zipfile.BadZipFile) as exc:
        parser.error(str(exc))
    print(f"Staged {len(files)} local-only WolvenKit font source files.")
    print("Import each staged TTF to replace its seeded .fnt container;")
    print("then Convert from JSON on the DreamUI .inkfontfamily.json.")
    print("Only Medium, Bold and Heavy are present in the resulting family.")
    print("Use the game's existing fonts until the new resources are cooked.")


if __name__ == "__main__":
    main()
