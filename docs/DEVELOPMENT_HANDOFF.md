# DreamUI development handoff — alpha.9

This file is the persistent starting point for subsequent development sessions. GitHub source and CI are authoritative; previous chat uploads are not guaranteed to remain accessible.

## Requirements and decisions

- DreamUI is a standalone Cyberpunk 2077 ASI. An ASI loader is needed, but CET, RED4ext, Codeware, ArchiveXL, HUD Painter and RedHotTools are NOT runtime dependencies.
- The experimental DreamEditor from the owner stays OUTSIDE this repository.
- Keep the existing foreground Tab probe and DreamUI-probe.log. The optional developer commands.txt inbox is NOT a DX12 overlay or a real in-game developer console.
- Never promise a UI recolor, immediate restoration, or a safe refresh without a real-game test. Native hooks, engine clones, color writes and full in-game UI customization are different milestones.
- The user's separate clean installation has only DreamUI and the game's bundled Steam REDmod ASI loader.

## Verified owner-provided clean-game loading observations

When a gated development ASI ran on the clean game, around 18.1 seconds into the session DreamUI-load.log reported:

    serializer_callbacks=4895
    candidates=4
    target_resources=1
    last_target_styles=10
    peak_target_styles=10
    last_matched_properties=3
    preflight_ready=1
    applied=0

The later Tab cache snapshot reported styles=0, themes=10, overrides=2. The serializer sees populated styles earlier, but the late cache inspection does not. The native resource was reached and properties validated; NO on-screen recolor has been verified. A later count of 7,429 callbacks was also observed. This evidence belongs to the earlier development test build, NOT the alpha.9 ZIP.

Target: base/gameplay/gui/common/main_colors.inkstyle (backslashes in the actual game path).
Known game PE timestamp 1792368047, image size 92385280; other executable fingerprints fail closed.

## PR milestones

- #19: automatic, bounded DreamUI-load.log serializer-stage counters. Tab unchanged.
- #20: pointer-free original HDRColor preimage.
- #21: full typed color-only style tree copy, rejecting all unsupported native Variant types.
- #22: engine ISerializable::Clone symbol and strongly owned handle abstraction.
- #23: separate one-shot Clone/Release experiment with nested allocation checks, NOT yet verified in the real game.
- Alpha.9: distribute a native-stage-enabled ASI for OPT-IN observation, while compiling OUT both native writes and native Clone allocation, regardless of marker files. Tab remains operational.

## Relevant implementation files

- src/plugin.cpp — theme loading, Tab probe, developer command inbox, automatic native trace sampler.
- src/win_load_hook.cpp — authenticated serializer callback, counters, preflight and build-time gates.
- src/win_clone_probe.cpp — development-only experimental engine Clone/Release roundtrip, not activated in alpha.9.
- src/ink_load_patch.cpp — HDRColor validation and journaled write/restore engine adapters (NOT enabled in alpha.9).
- src/ink_style_snapshot.cpp — typed all-or-nothing, read-only style tree extraction.
- src/address_db.cpp — independent Cyberpunk linker-map symbol resolver.
- docs/TESTING.md — installation and alpha.9 release instructions.
- .github/workflows/build.yml — Windows CI with experimental native stage.
- .github/workflows/release-testing.yml — prerelease ASI ZIP and SHA256 publishing.

## Reference mod research (never runtime dependencies)

- HUD Painter 1.4.1: r6/scripts/HUDPainter/HudPainterStorage.reds saves inkStyleResource.styles at Resource/Loaded. OnMainColorsReady restores cached styles and calls RefreshResource().
- Codeware 1.21.2: src/Red/Serialization.hpp and src/App/Scripting/ISerializableEx.hpp describe serializer handles and PostLoad, while src/Red/Scripting.hpp and src/Red/Shared/AddressLib.hpp publish native ISerializable::Clone ABI and hash.
- RED4ext.SDK: Handle.hpp, Memory/SharedPtr.hpp, Containers/DynArray.hpp, NativeTypes-inl.hpp, generated ink/StyleResource.hpp, Style.hpp and StyleProperty.hpp describe reference-counting and nested Variant storage. Header research does not imply RED4ext runtime installation.
- RedHotTools 1.3.0: Ink Inspector uses Game.GetInkInspector() and CollectInkLayers(). Its Lua entrypoint uses CET but DreamUI will not.
- ArchiveXL 1.28.6: research-only.

## Next steps after alpha.9

First test the actual published alpha.9 artifact using ONLY native-ink-preview.enable for load observation. Upload DreamUI-load.log and DreamUI.log if diagnosis is needed; no repeated external mod uploads. Then engineer a persistent engine-owned resource that can survive postload, followed by native game-thread refresh/restoration; confirm safe rollback before visible UI recolor. A standalone DX12 developer window is a separate task; do not conflate it with commands.txt.

Alpha.9 is a diagnostic milestone, not visible in-game theme support. The source-controlled file records verified findings and unresolved behavior; CI cannot replace a live game test.
