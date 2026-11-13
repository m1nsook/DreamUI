# Native Ink integration — evidence and status

DreamUI intends to recolor **Cyberpunk's native Ink resources**, without introducing HTML/CSS, an overlay or a mandatory externally installed scripting framework.

## Confirmed behavior from HUD Painter 1.4.1

After examining the actual supplied HUD Painter, RedFileSystem and RedData archives, the native resources are handled through **Resource/Loaded and Resource/Ready lifecycle callbacks**. HUD Painter caches the existing style array, changes selected inkStyleProperty Variant values to typed HDRColor, and calls RefreshResource on preset refresh. FileSystem and JSON dependencies only handle preset persistence.

See [source-level findings](HUD_PAINTER_REFERENCE.md) for exact source file names, line ranges and the distinction between palette resources, minimap bindings, world-map materials and menu entries.

## DreamUI alpha.8 status

The independent Windows ASI loads, validates .dream packages and responds to on-demand Tab diagnostics with no RED4ext installation required by DreamUI itself. In the owner's real game, it located the target cache entry with 7,298 cached tokens; the read-only inspector found **0 styles, 10 themes, and 2 overrides**. These are useful structure observations, NOT a successfully read Ink color, working resource adapter, or a patch.

The old fixed-time probes and repeated version-only diagnostic releases are no longer the preferred workflow.

## Corrected palette targets

Initial candidate mappings to investigate, grounded by the official HUD Painter resource guide:

| DreamUI semantic purpose | Documented game palette property | Evidence |
| --- | --- | --- |
| Primary text, headers and main menu | MainColors.Red | Main and pause menus, headings, primary text |
| Secondary text and icons | MainColors.PanelBlue | Secondary labels and icons |
| Subdued labels | MainColors.Grey | Settings, quickhack and journal text |
| Selection/accent | MainColors.ActiveYellow | Launch highlight text and active UI accents |

The earlier mapping of primary text to MainColors.PanelWhite was a poor proof target: the documentation marks it W.I.P. MainColors.DarkGrey is documented for locked cyberware outlines rather than general disabled text; treat that binding as **unverified**.

Reference: https://github.com/CDPR-Modding-Documentation/Cyberpunk-Modding-Docs/blob/main/modding-guides/hud-painter-resource.md

## Next implementation checkpoint

No more in-game diagnosis based solely on timeouts or the presence of a style-file name. Implement and unit-test an independent game-native resource event/load bridge, validate the actual loaded inkStyleResource and HDRColor Variant, then demonstrate one reversible MainColors.Red change. Game lifecycle and main-thread constraints must be verified before any engine method calls. A compatible ASI loader remains required to load DreamUI.asi.

## Dependencies and licensing

RED4ext.SDK headers and third-party scripts are useful research references, but RED4ext's default UniversalReloc requires RED4ext.dll. Do not redistribute HUD Painter's scripts, compiled archive, native dependencies or paid assets as DreamUI. All DreamUI implementation must be independently authored, and fallback must safely disable changes on unsupported game builds.

## C++ typed lifecycle model under development

DreamUI now has an independently authored, unit-tested `NativeInkColorSession` which accepts authenticated Loaded/Ready/Unloaded events, retains exact `HDRColor` original values, gates native actions to the game thread, refreshes when applying/restoring, and keeps a rollback journal after partial failures. **No live engine callback or writer backend is connected to the ASI**. Read the [native lifecycle contract](NATIVE_RESOURCE_LIFECYCLE.md) before enabling any patch.


## Native PostLoad mechanism found in the supplied Codeware distribution

HUD Painter's live preset switching works by restoring its separately
cached native `inkStyle` array to the resource and requesting
`RefreshResource()`. This convenience method is defined by Codeware
as a call to native `ISerializable::PostLoad`, whose virtual slot is
documented in RED4ext.SDK. Therefore DreamUI can eventually perform
the same operation independently, **but only after holding a real
owning resource handle and native-allocator-backed style arrays**.
Simply calling the virtual method against the earlier `styles=0`
snapshot would be unsafe and ineffective.
