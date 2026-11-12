# DreamUI

Free, open-source theme framework for Cyberpunk 2077's **existing** UI.

**Status: pre-alpha native theme preflight. It does not recolor the game yet.**

DreamUI aims to improve the game's own HUD, maps, item colors, notifications and backgrounds, without rendering a parallel UI.

## Goals

- One self-contained installation once the native adapter and installer are ready.
- No custom widget framework or premium theme features.
- Portable, validated .dream theme packages.
- Semantic colors for primary/secondary text, hostile/friendly markers, loot rarity and selections.
- MIT-licensed, independently authored implementation.

## Implemented

- Dependency-free Python .dream packer and C++ ZIP reader with CRC/path validation.
- ASI startup (confirmed working in Cyberpunk 2.5 for alpha.1).
- Native validation of JSON manifest, semantic #RRGGBB tokens and candidate vanilla Ink binding names.
- Tested injectable InkColorWriter interface and reversible color session, with an in-memory mock writer.
- Windows x64 builds, tests and prerelease workflow.

**No native game Ink resource accessor or write hooks are implemented yet.** RED4ext.SDK's default resolver needs RED4ext; DreamUI will not call it without an independently verified resolver. See docs/INK_INTEGRATION.md.

## Windows build

~~~powershell
python -m unittest discover -s tests -p 'test_*.py' -v
python tools/dreamtool.py pack themes/Default dist/Default.dream
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
~~~

Test ZIP needs an existing compatible ASI loader at present. Never overwrite a working version.dll. See docs/TESTING.md.

## Licensing

DreamUI is MIT licensed. It does not embed HUD Painter code, paid menu assets, Codeware, RED4ext, redscript or other independently installed mod frameworks.
