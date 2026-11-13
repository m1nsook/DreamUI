# Native standalone Ink resource serializer stage — implementation status

This is an independent C++ integration point for **DreamUI.asi**, not a
dependency on Codeware, HUD Painter, RED4ext.dll, redscript or ImGui.

## The actual engine lifecycle

The MIT-licensed Codeware source publishes the function ABI and hook
strategy for REDengine's serializer stage:

- `ResourceSerializer::SchedulePostLoadJobs`, symbol hash `3599579551`
  (same ABI as `void(uint64_t, uint8_t, uint64_t, uint64_t,
  ResourceSerializer**)`).
- The serializer's `DynArray<Handle<ISerializable>> serializables` is at
  offset `0x230`.
- A resource is identifiable by its native `CClass` RTTI and `CResource.path`.
- Codeware's `Resource/Loaded` callbacks run as scheduled engine jobs,
  *not* as arbitrary background polling.

References:
- https://github.com/Cyberpunk2077-Mods/Codeware/blob/main/src/Red/Serialization.hpp
- https://github.com/Cyberpunk2077-Mods/Codeware/blob/main/src/App/Callback/Controllers/ResourceLoadHook.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/NativeTypes.hpp

DreamUI independently implements a **load-stage transaction** in
`src/ink_load_patch.cpp`. The candidate caller can invoke it from the
serializer stage while a game's own strong `Handle<ISerializable>` still
protects the resource. It checks the resource identity/path, native
property name, external-aligned `HDRColor` Variant RTTI and all array
bounds before touching anything. It changes only the existing four-float
payload (no Variant allocator replacement), keeps the native HDR peak,
and preserves alpha.

A fixed-size rollback journal records every value before the first
write. If a write fails or verification fails, the journal is restored,
including the write that may have *partially succeeded*.

## Windows stage bridge

`src/win_load_hook.cpp` implements an experimental, guarded hook:
- Never hooks from DllMain
- Allows **only** the owner's confirmed PE fingerprint
  (`1792368047`, image size `92385280`)
- Resolves the engine function RVA through the game's **own** timestamp-
  matched linker address DB, and verifies the RVA is in executable `.text`
- Verifies `ISerializable::GetNativeType` is an executable native vtable
  entry and CClass name == `inkStyleResource`
- Reads only engine-owned objects from the live serializer's strong
  handles, never weak ResourceLoader cache pointers
- Applies only the `MainColors.Red` property from DreamUI's existing
  `text.primary` theme token
- Uses MinHook **statically linked** under its BSD-2-Clause license;
  no DLL or installed framework is required

**The bridge is not turned on in regular builds.** Two gates must be
satisfied:

1. Build with `-DDREAMUI_EXPERIMENTAL_NATIVE_STAGE=ON` (Windows CI
   compiles this configuration; normal local builds default OFF)
2. Explicitly place a marker file at
   `dreamui/dev/native-ink-preview.enable`

There is **no distributed executable with this feature enabled yet**.
The current public alpha.8 remains read-only.

## What is NOT yet verified

A successful C++/Windows build does not prove that the game callback can
be safely intercepted or that the newly written style is consumed on
the desired game screen. Critical remaining work before public release:

- Verify the *exact* serializer-stage callback ABI, ordering and native
  resource ownership on the clean 2.5 installation. The method ABI is
  documented by Codeware, but DreamUI has not observed it running.
- Verify the target's game-thread synchronization, including possible
  parallel resource-postload jobs. Current hook uses its own
  non-blocking reentrancy guard and never writes from Tab worker.
- Verify the color actually propagates to the native Ink widgets.
- Implement a safe game-driven `RefreshResource` call and **live restore
  path**. The current stage transaction rolls back write failures but
  does not implement interactive restoration of successfully changed
  resources.
- Validate compatibility and fail-closed disabling when game updates
  replace the known symbol layout.

Do not equate the conditional code in this branch with a working
standalone theme engine. No new release is justified until visible
recolor + refresh + restoration work in the clean game, safely.
