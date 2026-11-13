# Alpha.6 — native cached Ink style inspection

## Why this step

The owner's in-game alpha.5 log proves DreamUI's validated ResourceLoader global pointer transitions from null at +0/+3s to a mapped pointer by +8s. A mapped pointer is not yet an Ink resource. Alpha.6 checks the actual ResourceLoader token-cache layout, without calling any native function.

## Independently implemented read-only path

1. Resolve the native ResourceLoader singleton through the game's own linker address database (no RED4ext.dll).
2. Restrict memory layout inspection to the known game-executable fingerprint (timestamp 1792368047, image size 92385280). Requiring a version gate prevents applying unverified layouts on unrelated versions.
3. Copy the 0x30-byte `ResourceLoader.tokens` HashMap header using a guarded `ReadProcessMemory` callback.
4. Calculate a `ResourcePath` FNV-1a64 hash for `base\gameplay\gui\common\main_colors.inkstyle`. Read only its expected bucket and at most 128 chain nodes. Validate bounds, counts, stride, keys and weak token reference.
5. If cached and still referenced, copy the matching `ResourceToken` header, examine `finished`/`error` and its resource handle.
6. If loaded, copy `inkStyleResource` metadata (`styles` array at +0x40). For each style, inspect bounded property entries and search for the `CName` hash of `MainColors.Red`.
7. If a matching property uses an inlined Variant, validate the RTTI CClass name/size/alignment against `HDRColor` before reading its four float channels. A read-only type match still cannot grant ownership of the resource.

No game engine functions, memory writes, hook insertion, reference counting operations or locks are performed. Read errors and structural inconsistencies are failures, not fatal game exceptions. The cache may change concurrently and a transient miss is not a correctness guarantee.

## SDK layout references, not redistributed

- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/ResourceLoader.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/HashMap.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/ResourcePath.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/ink/StyleResource.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/ink/Style.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/ink/StyleProperty.hpp

DreamUI's cache reader and tests are independently authored. No licensed game assets or paid mod code are included.

## Next acceptance

A valid cached resource lookup and an RTTI-authenticated `HDRColor` read on the user's installed game build. If the resource is never cached, use an engine-supported load request on an appropriate game thread only after safely verifying its calling convention and lifetime. Then implement the first rollback-safe color override. Continue using the game's existing Ink UI, not a new overlay.

## New read-only RTTI validation

A matching Ink property no longer qualifies simply because its first four value slots look like floats. DreamUI checks the inline Variant bit and reads the engine's RTTI descriptor, requiring the `HDRColor` CName, 16-byte size, and 16-byte alignment. Fake-memory tests include wrong RTTI types and non-inline variants. A matching descriptor is not sufficient for safe modification from the Tab worker thread. See [native lifecycle contract](NATIVE_RESOURCE_LIFECYCLE.md).

## Corrected native Variant storage

RED4ext.SDK NativeTypes-inl.hpp shows Variant::CanBeInlined requires
alignment <= 8, while HDRColor is alignas(16), size 16.
**Therefore a real native HDRColor Variant is externally allocated**:
Variant.type at +0x00 has no InlineFlag, and Variant.instance at +0x08
is a 16-byte-aligned pointer to the four float channels. A previous
read-only inspector incorrectly treated it as inline and could never
read a real HDRColor. That test fixture was corrected.

## Linked style resources

The main_colors.inkstyle resource can contain zero direct styles and
nonempty arrays of native inkStyleTheme and inkStyleOverride references.
The new bounded, read-only traversal follows only already-loaded
Ref<inkStyleResource> entries in themes/overrides/imports, checks token
path identities, strong reference counts, completion/error states, and
child resource paths, then searches their style properties for
MainColors.Red with authenticated external HDRColor storage.

The walk has depth, reference, style and property limits and a
cycle-detection set; failures disable observation rather than editing
memory. It does not acquire owning handles, refresh the game UI, or
install a Resource/Loaded callback. It does not qualify as a live
theme implementation.
