# Independent native address resolver — alpha.4

DreamUI reads Cyberpunk's own bin/x64/cyberpunk2077_addresses.json without RED4ext. The address file is shipped with normal PC game distributions and is not redistributed by DreamUI.

The engine probe first validates the on-disk PE32+ executable and extracts .text, .rdata and .data section bounds and the COFF timestamp. The resolver streams the game's 100+ MiB address table using bounded memory, requires the table's Linker map timestamp to match the executable's COFF timestamp, and extracts four known hashes:

- 0x783C1034: ResourceLoader global pointer
- 0xC86F39B7: ResourceLoader_FindTokenFast
- 0x4A610F64: CRTTISystem_Get
- 0x9D14104F: InkSystem singleton pointer candidate

Each selected segment:offset is converted to an RVA using the corresponding PE section; out-of-bounds offsets are rejected. These are candidate native symbols, not verified game function call sites.

The only live-process operation is a guarded ReadProcessMemory of the ResourceLoader pointer variable, if present in .data. VirtualQuery checks whether a nonnull pointer is mapped; no game object is dereferenced, no function called, and no memory written.

The database may be absent from nonstandard distributions. Missing or mismatched data is logged and engine probing stops; it must never fall back to unverified offsets.

Unit tests cover expected values, timestamp mismatch and section bounds. Windows CI verifies compilation and test execution. **Real Ink resource access, color changes, and the DreamUI tile remain unimplemented.**

Next: verify ResourceLoader lifetime and engine thread requirements, then read one actual Ink style property. Only after that may the engine adapter implement reversible color writes.

Technical references:

- https://github.com/wopss/RED4ext/blob/master/src/dll/Addresses.cpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/ResourceLoader.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Detail/AddressHashes.hpp
