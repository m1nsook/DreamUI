# DreamUI v0.1.0-alpha.5 — deferred ResourceLoader initialization probe

**Still a diagnostic release, not a HUD recoloring mod.** Cyberpunk's existing UI should remain unchanged.

## Why this build exists

Alpha.4 successfully read the game's linker address database, matched the executable build timestamp and found all four target symbols. The resource-loader global pointer was **null** during the earliest check. That can happen when the ASI loads before the engine finishes initializing.

The public RED4ext.SDK definition confirms that ResourceLoader::Get() reads a global pointer from the address associated with hash `0x783C1034`. Alpha.5 checks that same validated address at four startup points: **0, 3, 8 and 15 seconds**. It stops early if a mapped pointer is observed.

## Test

Extract the GitHub prerelease ZIP over alpha.4, keeping your existing ASI loader intact. Start Cyberpunk 2077 and leave it at the main menu for at least 20 seconds before exiting. Then open `dreamui/logs/DreamUI.log`.

Look for:

```text
DreamUI v0.1.0-alpha.5 | read-only engine reconnaissance; no patches enabled
Address DB: game linker map validated, entries_scanned=... supported_symbols=4/4
ResourceLoader: checking startup initialization at +0,+3,+8,+15 seconds
ResourceLoader sample +0s: null/not initialized
ResourceLoader sample +3s: pointer present (not dereferenced)
```

The later sample might instead stay `null/not initialized`, report `unverified pointer`, or have a different time. Those are meaningful diagnostic outcomes. No game APIs are invoked and no engine objects are dereferenced.

The log is append-only and retains earlier alpha.2–alpha.4 entries. Share the newest **alpha.5 section** rather than deleting or editing the old log.

## Limitations

A pointer being mapped does not prove that an Ink style resource can be safely loaded or inspected. The next milestone is verifying the loader's native object layout and threading requirements, then **reading a real vanilla Ink style value**. Only after that can the first reversible recolor be enabled.

No mandatory RED4ext, Codeware, redscript or paid add-ons are bundled. The mod still needs an already installed ASI loader; never overwrite `version.dll`.
