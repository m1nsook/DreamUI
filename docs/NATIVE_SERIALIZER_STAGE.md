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
- Applies only three documented native properties from `text.primary`,
  `text.secondary`, and `selection.active` on the exact vanilla
  `base\\gameplay\\gui\\common\\main_colors.inkstyle` resource.
  Other themes, including Johnny and accessibility palettes, are excluded
- Uses MinHook **statically linked** under its BSD-2-Clause license;
  no DLL or installed framework is required

**The bridge is not turned on in regular builds.** It now has two
separately gated modes:

1. Build with `-DDREAMUI_EXPERIMENTAL_NATIVE_STAGE=ON` (Windows CI
   compiles this configuration; normal local builds default OFF).
2. Place `dreamui/dev/native-ink-preview.enable` under the game's root.
   This installs the experimental hook in **read-only observation** mode.
   It checks only strongly held `main_colors.inkstyle` resources using
   the same complete native Variant validation as the patcher, without
   invoking a memory writer. The native hook automatically writes `serializer_callbacks`,
   `target_resources`, `last_target_styles`, `peak_target_styles`,
   `preflight_ready`, `preflight_no_styles`, `preflight_missing` and
   `preflight_rejected` to `dreamui/logs/DreamUI-load.log` as they change.
   No Tab press or `commands.txt` command is needed.
3. **Only when intentionally testing native mutation**, ALSO place
   `dreamui/dev/native-ink-write.enable` before game startup.
   This enables the pre-existing load-stage apply/restore intent path.
   Neither marker is re-read at runtime; removing or adding one requires
   a game restart. Do not use write mode on a normal playthrough.

The read-only mode never calls `MemoryWrite`, but **installing a native
function hook is still invasive**: a wrong callback ABI, build fingerprint,
or hook ordering could crash the process. Observation is not proof of a
fully safe hook, UI refresh, or stable native recoloring.

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
- Implement a safe game-driven `RefreshResource` call and **interactive
  restoration**. A generation-bound restoration journal now exists
  for successfully changed resources, but restoration can run only if
  the SAME authenticated serializer instance returns in a future
  callback. This is not a live UI refresh or a guaranteed restore.
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


## Three native palette targets in one atomic load-stage transaction

Rather than limiting the first standalone palette to the color key
`MainColors.Red`, the experimental stage now applies all three
verified DreamUI semantic mappings **atomically**:

| .dream token | Native inkStyleProperty |
| --- | --- |
| `text.primary` | `MainColors.Red` |
| `text.secondary` | `MainColors.PanelBlue` |
| `selection.active` | `MainColors.ActiveYellow` |

All three native properties must be found and have valid HDRColor
Variants **before any write**. If one is missing, no colors are
changed. A partial failure at the third property rolls back all three,
including the property which returned a failed write after modifying
memory. The full preimage is retained in an optional generation-bound
journal for a later authenticated restore. Each native color's peak
brightness and alpha are preserved.

## Pointer-free first-load HDRColor snapshot

After the observed clean-game serializer preflight confirmed ten direct
styles and three matching HDRColor targets, DreamUI now retains a
**plugin-owned, bounded copy of the validated color payloads**. The
`InkOwnedColorPreimage` contains each matched property hash, style
identity/state, style/property indices, original four-channel HDRColor,
and proposed four-channel preview, plus the resource path and instance ID.
It deliberately contains **no raw engine allocation pointer**; the
preflight clears its output completely on failure. The native observation
hook captures the first eligible preimage before the game continues
post-load processing, and status exposes `owned_original_colors` and
`source_styles`. The write-mode path captures before any native color
transaction.

This is **not the same as HUD Painter's engine-owned deep copy of
`array<inkStyle>`**. Each `inkStyle` has a nested property array and
each `Variant` may own an external allocation; memcpy, borrowed
DynArray headers, or copied property pointers would be unsafe.
DreamUI's value-only capture cannot repopulate `resource.styles` after
post-load consumption, call `RefreshResource()`, or guarantee live
restoration. These remain independent implementation milestones.

Reference: RED4ext.SDK `ink/Style.hpp`,
`ink/StyleProperty.hpp`, `ink/StyleResource.hpp`,
`NativeTypes-inl.hpp` and `Containers/DynArray.hpp`.
The SDK code serves as layout/ownership research; DreamUI does not
require RED4ext at runtime.

## Complete style-tree capture (HDRColor-only, read-only)

The separate `InkOwnedStyleTree` attempts to capture **every** style and
property in the authenticated main-colors resource at the serializer stage.
It copies ordered `inkStyle` IDs and states, property hashes, and
`HDRColor` values into ordinary C++ vectors owned by DreamUI. It rejects
invalid bounds, invalid float channels, inlined/misaligned HDRColor Variant
storage, unreadable type metadata, or more than 32 styles / 4096 total
properties.

**This attempt is all-or-nothing.** When a non-HDRColor Variant is
present, it reports `unsupported_variant` and discards the complete
tree instead of memcpy'ing a potentially ownership-bearing native
Variant. If every property passes, the automatic load trace reports
`full_tree_capture=captured`, with style/property counts. The existing
three-color original-value snapshot is independent and still works if
the complete tree cannot be captured.

This is a **host-owned value representation**, not a native `DynArray`.
Reconstructing `inkStyle` and `inkStyleProperty` for a refresh still
requires authenticated engine allocator and RTTI copy/destruct operations
in an owned resource/lifecycle context. The test branch does **not**
invoke them or perform any native refresh.

References: RED4ext.SDK `Containers/DynArray.hpp`,
`NativeTypes-inl.hpp`, `ink/Style.hpp`, and
`ink/StyleProperty.hpp`. HUD Painter's `HudPainterStorage.reds`
uses script-side typed copying in `OnStyleLoaded` and typed resource
assignment in `OnMainColorsReady`; raw C++ memory copying would not
provide equivalent ownership.

## Why a live refresh needs an owning style snapshot

A source-level review of **the owner's HUD Painter 1.4.1 ZIP** and
Codeware 1.21.2 establishes:

1. In HUD Painter `HudPainterStorage.reds`, `OnStyleLoaded` copies
   the engine's initial `resource.styles` into a separate owned
   `defaultStyles` array **before post-load finalization**.
2. For interactive preset changes, HUD Painter obtains a genuine game
   resource handle with `ResourceDepot.LoadResource`, assigns the copied
   `defaultStyles` back into `resource.styles`, replaces selected native
   `Variant/HDRColor` fields, then calls `RefreshResource()`.
3. `RefreshResource()` is **not an independent native engine function
   that we must locate by name**. In Codeware's REDscript it delegates
   to `ISerializable.ProcessPostLoad()`. The C++ implementation calls
   `ISerializable::PostLoad({disablePreInitialization})`, a virtual
   method in the RED4ext.SDK vtable at offset `0x28`.

References:
- https://github.com/psiberx/cp2077-codeware/blob/main/scripts/Scripting/ISerializable.reds
- https://github.com/psiberx/cp2077-codeware/blob/main/src/App/Scripting/ISerializableEx.hpp
- https://github.com/wopss/RED4ext.SDK/blob/master/include/RED4ext/ISerializable.hpp

**Crucial:** merely invoking `PostLoad` on the style resource with
`styles=0` will not recreate color properties. DreamUI needs a genuine
engine-allocated, lifetime-safe copy of the styles/Variants and a
game-owned callback context **before** it can safely issue an
interactive refresh or a reliable immediate restoration. The initial
load-stage patch needs no extra refresh call: the engine's own normal
post-load work follows it. We do not invoke PostLoad twice.

This work remains part of the standalone ASI and does not import
Codeware or HUD Painter into the game.


## Validated .dream theme selection

The game-side ASI now retains the actual validated colors of all loaded
`.dream` packages, not only their catalog names. `select <theme-id>`
updates the active catalog selection, persists it to
`dreamui/active-theme.txt` and supplies the three mapped colors to
the conditional native serializer bridge. The bridge snapshots its
palette under a mutex on the game callback thread, avoiding a data
race with the console worker.

Theme switching is refused if a native restoration journal is still
active. The preference stores only a validated theme ID, not any
resource address or arbitrary file path. Neither the preference
nor the console worker directly changes live game memory.

## Inspecting the native load path on a clean installation

The preflight is deliberately conservative. A `preflight_ready` count
greater than zero means a matching, strongly-held serializer resource
contained the three expected typed HDRColor properties, with valid native
values and a nonzero instance ID, when the hook inspected it. It does
**not** mean colors were written or propagated to on-screen widgets.

`preflight_no_styles` means the matching resource had no direct styles
at that point in its lifecycle. `preflight_missing` means at least one
named target could not be found. `preflight_rejected` means metadata,
memory, resource identity, or a target HDRColor failed validation.
`target_resources` counts only the exact `main_colors.inkstyle` resource,
while `candidates` counts every type-validated inkStyleResource.

Use a dedicated test installation. `DreamUI-load.log` is generated
without manual input when the experimental hook is enabled; leave
`native-ink-write.enable` absent. See
[automatic native load trace](AUTOMATIC_LOAD_TRACE.md) for interpretation.
No other mod framework or asset is bundled or required by DreamUI.
