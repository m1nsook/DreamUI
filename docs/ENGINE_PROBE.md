# Alpha.3: read-only executable reconnaissance

DreamUI alpha.3 reads **the game executable file on disk**, not live game memory, to gather a compatibility fingerprint.

Implemented:
- Windows VersionInfo file-version extraction where available.
- Bounds-checked PE32+ executable header parsing, x64 architecture verification, COFF timestamp and image size.
- Bounded (up to 64 MiB per section) on-disk `.rdata` string-presence checks for `inkStyleResource`, `inkStyleProperty`, and `inkWidget`.
- Strict no-write behavior and safe exit if the file is absent, malformed or can't be opened.
- Portable C++ tests for ordinary and malformed PE section tables.

**Important:** String matches are merely reconnaissance, not validated function signatures, RTTI objects, resource handles, loaded Ink widgets or actual native memory addresses. An absent match is not proof the game lacks that class (names may be hashed/stripped). No engine resource handles are acquired or color writes attempted.

A real native Ink resource reader requires independently validated engine access and lifecycle/thread safety. RED4ext.SDK is a reference for public type definitions, but the default universal relocator expects RED4ext.dll. DreamUI must not introduce that as a mandatory dependency or use unsupported game offsets.

Inspect `dreamui/logs/DreamUI.log` after testing alpha.3. Never provide copyrighted game executables in issue reports; the diagnostic fields are sufficient for compatibility discussion.

See [Ink integration](INK_INTEGRATION.md) and [Control Hub](CONTROL_HUB.md).
