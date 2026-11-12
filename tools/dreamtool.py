#!/usr/bin/env python3
"""Build and validate version-1 DreamUI packages. Uses only the Python standard library."""
from __future__ import annotations
import argparse
import json
import re
import sys
import zipfile
from pathlib import Path

MAX_SIZE = 16 * 1024 * 1024
VALID_ID = re.compile(r"^[a-z0-9]+(?:[.-][a-z0-9]+)*$")


def validate(directory: Path) -> list[tuple[str, bytes]]:
    manifest_file = directory / "manifest.json"
    manifest = json.loads(manifest_file.read_text(encoding="utf-8"))
    required = ("schemaVersion", "id", "name", "version", "type")
    if any(key not in manifest for key in required):
        raise ValueError("manifest.json is missing one of: " + ", ".join(required))
    if manifest["schemaVersion"] != 1 or manifest["type"] != "theme":
        raise ValueError("only schemaVersion 1 and type=theme are supported")
    if not isinstance(manifest["id"], str) or not VALID_ID.fullmatch(manifest["id"]):
        raise ValueError("invalid theme id")
    if not isinstance(manifest["name"], str) or not manifest["name"].strip():
        raise ValueError("theme name must be nonempty")
    if not isinstance(manifest["version"], str) or not re.fullmatch(r"\d+\.\d+\.\d+", manifest["version"]):
        raise ValueError("theme version must be MAJOR.MINOR.PATCH")
    colors_path = directory / "theme" / "colors.json"
    colors = json.loads(colors_path.read_text(encoding="utf-8"))
    if not isinstance(colors, dict):
        raise ValueError("colors.json must be an object")
    def visit(node: object, key: str) -> None:
        if isinstance(node, dict):
            for child_key, child in node.items():
                if not isinstance(child_key, str) or not child_key:
                    raise ValueError("invalid token name")
                visit(child, f"{key}.{child_key}")
        elif not isinstance(node, str) or not re.fullmatch(r"#[0-9a-fA-F]{6}", node):
            raise ValueError(f"{key} must be a #RRGGBB color")
    visit(colors, "colors")
    results: list[tuple[str, bytes]] = []
    total = 0
    for file in sorted(directory.rglob("*")):
        if file.is_symlink():
            raise ValueError(f"symlinks are not allowed: {file}")
        if not file.is_file():
            continue
        rel = file.relative_to(directory).as_posix()
        if not rel or any(part in (".", "..") for part in rel.split("/")):
            raise ValueError("unsafe package member")
        content = file.read_bytes()
        total += len(content)
        if len(results) >= 128 or total > MAX_SIZE:
            raise ValueError("package too large or too many files")
        results.append((rel, content))
    return results


def build(source: Path, destination: Path) -> None:
    entries = validate(source)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(destination, "w", compression=zipfile.ZIP_STORED) as archive:
        for path, content in entries:
            info = zipfile.ZipInfo(path, date_time=(2020, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            info.external_attr = 0o644 << 16
            archive.writestr(info, content)
    if destination.stat().st_size > MAX_SIZE:
        destination.unlink()
        raise ValueError("built package exceeds size limit")
    print(f"Created {destination} ({len(entries)} members)")


def main() -> None:
    parser = argparse.ArgumentParser(prog="dreamtool", description=__doc__)
    parser.add_argument("command", choices=["validate", "pack"])
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", nargs="?", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "validate":
            print(f"Valid: {len(validate(args.source))} entries")
        else:
            if args.destination is None or args.destination.suffix != ".dream":
                parser.error("pack requires destination ending in .dream")
            build(args.source, args.destination)
    except (ValueError, OSError, json.JSONDecodeError, KeyError) as error:
        print(f"Error: {error}", file=sys.stderr)
        raise SystemExit(1)


if __name__ == "__main__":
    main()
