# DreamUI v0.1.0-alpha.6 — read-only vanilla Ink cache inspection

**Pre-alpha, read-only.** No color changes, game-function calls, custom menu widgets, or writes to game memory.

## Test alpha.6

1. Extract the test ZIP into Cyberpunk 2077, replacing alpha.5, keeping your existing ASI loader unchanged.
2. Launch the game, press **Space** past the title screen into the **main menu**, and leave it there for at least **25 seconds**.
3. Exit and open `dreamui/logs/DreamUI.log`. The file is append-only; share the latest alpha.6 block.

## Expected log

After the address-table validation and the ResourceLoader startup checks, alpha.6 logs the first native cache result:

```text
DreamUI v0.1.0-alpha.6 | read-only engine reconnaissance; no patches enabled
Address DB: game linker map validated, entries_scanned=... supported_symbols=4/4
ResourceLoader sample +8s: pointer present (not dereferenced)
Ink style cache check +0s after loader initialization: ...
```

Possible results:

- `style resource not present in ResourceLoader token cache`: the target resource was not cached at the time of reading. This is NOT proof the game does not have that resource.
- `resource token still loading`: a cached token exists but isn't ready.
- `cached inkStyleResource read successfully`: the cache contained a matching, loaded native resource and Ink style metadata.
- `property CName matched; inline RGBA floats are candidates (RTTI type unverified)`: the relevant property name was found and the bytes appear consistent with an inline color.
- `... failed validation`: a guard rejected unexpectedly structured data. Do not force it.

Alpha.6 uses only `ReadProcessMemory` to copy bounded structures, without acquiring engine locks, calling functions or modifying references. Concurrent engine cache changes might make a read inconsistent; in that case it should safely refuse the result.

**Supported game build:** the read-only Ink cache inspector is intentionally gated to the verified alpha.3 fingerprint: COFF timestamp 1792368047, image size 92385280. Other builds skip that step rather than assuming the same object layout.

Even if a candidate RGBA value appears, no theme change is activated. The variant's RTTI type must be verified before treating it as an authoritative HDRColor.

The main-menu Dream Manager tile and one-ZIP self-contained ASI loader remain later milestones.
