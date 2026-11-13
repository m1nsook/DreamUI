# Native Ink event lifecycle and typed HDRColor session

## Status (post-alpha.8, not yet released as a playable build)

This milestone adds a portable C++ native Ink lifecycle **contract** and a
mock-tested, transactional single-color session. It does not claim the
Cyberpunk engine already invokes these callbacks. The standalone ASI currently
has no verified native callback subscription or engine-thread scheduler; **no
new game writes are enabled**.

This is a deliberate shift away from the alpha.2–alpha.7 series of startup
polling experiments. The user-provided HUD Painter 1.4.1 scripts show the
actual lifecycle: `Resource/Loaded` for
`base\gameplay\gui\common\main_colors.inkstyle`, an engine-provided
`inkStyleResource`, a populated `styles[0].properties`, typed `HDRColor`
values stored in `inkStyleProperty.value` as `Variant`, and a
`RefreshResource()` on preset changes. The `Resource/Loaded` API in those
scripts is provided by the larger REDscript/Codeware stack; it **cannot** be
called directly from an ASI without that stack.

## Implemented in DreamUI's own C++ (portable)

- `HdrColor` is a 16-byte, 16-byte-aligned value with four float channels.
  The original red/green/blue/alpha are preserved as-is; native values can
  exceed 1.0. Invalid/nonfinite channels are rejected.
- `InkResourceEvent` carries `Loaded`, `Ready`, or `Unloaded`, an identity
  and generation number, an exact resource path, and an explicit
  `verified_ink_style_resource` flag. Only a **verified Ready** event
  authorizes a typed operation, and an unload invalidates the saved handle.
- `InkEngineAccess` is a zero-implementation, engine-thread-only backend
  interface. Its `read_hdr_color` **must verify Variant HDRColor RTTI**;
  its `write_hdr_color` and `refresh_resource` **must use supported native
  engine APIs, allocators and thread context**.
- `NativeInkColorSession` begins with only `MainColors.Red` (documented as
  primary/main-menu text). It journals the original four-channel value
  **before** writing, refreshes on apply and restore, automatically rolls
  back a write or refresh error, and leaves the original in the journal
  when rollback cannot finish. Repeated application is refused until
  restoration succeeds.
- If the engine unloads that resource generation, the session drops the
  stale generation without writing to potentially freed memory.
- `preview_sdr_color` illustrates a provisional #RRGGBB -> 0..1 conversion;
  this is NOT certified as Cyberpunk's HDR color-space transform. It
  preserves original alpha. The game-facing adapter must verify
  the conversion before enabling a visible effect.

## Read-only type verification now available in the cache inspector

The mock-tested reader recognizes an inline `Variant` (tagged pointer to
RTTI type), reads the `CClass` name/size/alignment metadata via a bounded
read callback, and requires:

- type name `HDRColor` (CName FNV1a64)
- size `0x10`
- alignment `0x10`
- 16 bytes of finite color channels

If a name looks like `MainColors.Red` but its Variant is a different type,
the parser **fails closed** rather than interpreting arbitrary bytes as
RGB floats. Even a matching RTTI metadata layout does **not** give the
cache inspector ownership of the resource or make it safe to write.

## Automated tests

The independent suite exercises successful event readiness, wrong path/type,
wrong thread, no ready event, missing theme token, RGB preview and alpha
preservation, an HDR default greater than 1, invalid floats, a partial
write that returns failure, a refresh failure, rollback failure and retry,
failed restoration refresh and retry, repeated apply, and unloading or
changing the resource generation.

## The unresolved native engine hookup

An actual standalone game integration still needs independently verified:

1. Event subscription or a game-owned resource-token completion callback.
   RED4ext.SDK documents `ResourceLoader::LoadAsync` and
   `ResourceToken::OnLoaded` function types; RED4ext's `UniversalReloc`
   must NOT be used without RED4ext.dll. We have a separate read-only
   address-database resolver, but the callback ABI, allocation semantics,
   resource lifetime, and game thread dispatch **have not yet been
   validated** on the owner's executable.
2. A proper owning handle to the fully loaded `inkStyleResource`; a
   `ReadProcessMemory` snapshot is not sufficient.
3. Native `Variant` construction/copy and `HDRColor` type lookup by the
   game RTTI system, then a game-owned `RefreshResource()` call at a safe
   lifecycle point.
4. End-to-end apply and restore, including rollback after reload/unload
   and interaction with other UI mods.

Only after those prerequisites are met should DreamUI publish another
game-testing prerelease or turn on memory-changing behavior. The current
alpha.8 remains the safe read-only release.

## Reference documentation

- [HUD Painter investigation](HUD_PAINTER_REFERENCE.md)
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/ResourceLoader.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/NativeTypes.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/RTTITypes.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/HDRColor.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/Scripting/Natives/Generated/ink/StyleProperty.hpp
