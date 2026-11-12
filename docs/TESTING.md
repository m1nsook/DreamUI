# DreamUI v0.1.0-alpha.7 — native Ink style-array readiness probe

**Read-only development test. There are still no visible UI changes.**

Alpha.6 established that Cyberpunk's ResourceLoader token cache contains the requested vanilla main_colors.inkstyle resource, but its Ink style array was reported as empty. Alpha.7 distinguishes an empty style array from a genuinely populated style, reports additional metadata, and keeps checking after the initial cache match.

## Test

1. Extract the GitHub prerelease ZIP over alpha.6, keeping the compatible ASI loader you already use.
2. Start Cyberpunk 2077, press **Space** to pass the title screen, and enter the main menu.
3. Stay at the main menu for **50 seconds** before closing the game. This permits the final +32-second sample after native loader initialization.
4. Open `dreamui/logs/DreamUI.log` and share **only the latest alpha.7 block**.

Expected log pattern:

```text
DreamUI v0.1.0-alpha.7 | read-only engine reconnaissance; no patches enabled
ResourceLoader sample +8s: pointer present (not dereferenced)
Ink style cache check +12s after loader initialization: cached resource path matched, but Ink styles array is empty | cached_tokens=... styles=0 properties=0 style_capacity=... themes=... overrides=... imports=...
Ink style cache check +20s after loader initialization: ...
Ink style cache check +32s after loader initialization: ...
```

What the results mean:

- `cached resource path matched, but Ink styles array is empty`: the cache entry exists, but DreamUI **cannot yet** read a color value. We need to distinguish initialization timing from a resource-layout problem.
- `Ink styles array populated; inspecting properties`: native style entries are present, but the requested property might be absent.
- `property CName matched; inline RGBA floats are candidates (RTTI type unverified)`: a candidate color was found; it is **not authenticated** as HDRColor until RTTI confirms the Variant type.
- `... failed validation`: the inspector stopped safely on unexpected data; do not force it.

This probe uses bounded ReadProcessMemory on the game version fingerprint verified by the project owner. It never calls a native game function, changes game memory, or patches HUD elements. No RED4ext, redscript or Codeware runtime dependency has been added.

The DreamUI main-menu control tile and a self-contained ASI loader remain future features.
