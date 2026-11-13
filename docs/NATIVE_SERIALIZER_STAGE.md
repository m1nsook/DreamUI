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
- Applies only the `MainColors.Red` property on the exact vanilla
  `base\\gameplay\\gui\\common\\main_colors.inkstyle` resource, from
  DreamUI's existing `text.primary` token. Other themes, including Johnny
  and accessibility palettes, are excluded from this initial preview
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

## Verified Cyberpunk 2.5 serialized style data

The independently extracted ink corpus at
https://github.com/spuddeh/cp2077-ink-archive (base+Phantom Liberty, 2.5)
contains the root path
`base\gameplay\gui\common\main_colors.inkstyle` with **10 direct inkStyle
entries**, **10 theme references** and **2 override references**. Its
first `MainColors` style includes a typed `HDRColor` for `MainColors.Red`
approximately RGBA (1.1761, 0.3809, 0.3476, 1.0).

In contrast, the owner's earlier alpha.8 *runtime* cache read reported
`styles=0, themes=10, overrides=2`. This is NOT evidence that the original
asset has no styles. It may reflect post-load movement/consumption of
the styles array, an observer timing issue, or a wrong assumption about
the runtime layout. The serializer-stage hook is intended to operate
before such a runtime difference can erase the source data, but **that
ordering is still unverified in the running game**.


## Authenticated restoration journal (source milestone)

The next implementation introduces an explicit `InkLoadJournal`, recording
*every* matching `MainColors.Red` four-float preimage, expected changed value,
style index, property index and native Variant allocation identity.
It also stores the ISerializable serialized instance ID (`+0x28`).

`restore_loaded_ink_color` is callable **only from an authenticated native
resource callback containing the same live, strongly-owned resource**.
Before restoring a single color, it validates all the current property
hashes, resource identity, native HDRColor RTTI, external storage pointers
and exact applied float values. It does not overwrite changes made by other
mods. On a partial restore failure it attempts to roll back every touched
property to its applied value; a failed restore never silently discards the
journal. On success, the owning callback clears the journal.

An optional `ink-apply` or `ink-restore` developer command merely records
intent in an atomic flag. The worker thread NEVER writes to the game.
The serializer callback can apply on a *future* load or restore when the
**same instance** is passed to it again. **This is not a live-refresh
mechanism**, and no game callback has been observed in our clean install yet.
An Ink resource may not be scheduled a second time, so a restore request can
remain pending indefinitely. Do not advertise these commands as reliable
instantaneous theme switching.

The native bridge is still compiled only when the experimental build
option is enabled and additionally gated by a per-install marker. The
released alpha.8 is unaffected.

## ABI/threading and release blockers

The CODE-level callback ABI and type metadata have been checked against
the RED4ext SDK and Codeware source. But the user's separate clean Steam
installation has not exercised the new hook yet. A passing mock test does
not validate the game's serializer thread affinity or guarantee the UI will
consume the changed property. Before a public working release we need
a verified engine-owned rescheduling/refresh mechanism and an immediate
safe restoration route for a retained game resource.
