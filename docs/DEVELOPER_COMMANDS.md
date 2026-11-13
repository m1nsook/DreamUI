# DreamUI developer commands — no ImGui overlay required

The project owner supplied a DreamUI developer ZIP with an ImGui panel,
DirectX 12 Present hooks, and a portable CommandRegistry. DreamUI has
adapted the command registry and worker-thread dispatch ideas, but does
not install the proposed render or input hooks.

## Why the DirectX panel is off

The prototype creates its OWN DX12 command queue while using Cyberpunk's
swap-chain buffers. It does not synchronize with the game's presentation
queue or account for other installed renderer hooks. This is unsafe for
general gameplay. A future overlay would require the game's actual queue,
per-back-buffer command allocators/fences, correct resource-state tracking
and reliable input ownership.

DreamUI's goal remains native Ink UI recoloring, not a replacement HUD.

## Run diagnostic commands on any screen

Start the game, enter the screen to inspect, and create or edit the file

`Cyberpunk 2077/dreamui/dev/commands.txt`

Save ONE of the commands below (the file may contain multiple commands,
but they will execute on the same screen when saved together):

`probe main-menu`
`probe inventory`
`probe gameplay`
`probe map`

DreamUI processes edits without restarting the game, from its worker
thread. The label records where YOU say you are; the engine does not
verify that menu/gameplay phase automatically.

Read the results in `dreamui/logs/DreamUI-probe.log`, and command
acknowledgements in `dreamui/logs/DreamUI-dev.log`.

The plain Tab key still triggers a new read-only probe while the game
window has focus, with the manual phase label `hotkey`.

## Commands

- `help`: list commands
- `version`: build version
- `themes`: show validated .dream packages
- `status`: print probe count, last manual phase label and theme count
- `probe [phase]`: read-only Ink cache and linked-style snapshot
- `clear`: clear only DreamUI-dev.log

An inbox left from a previous game launch is NOT executed. Subsequent
saves are debounced until two identical filesystem observations, limited
to 4096 bytes, 16 lines per save, 512 bytes per command and at most
16 arguments. There are no game calls, memory writes or network
listeners in the command registry.

No ImGui/MinHook/redscript/RED4ext SDK dependency is introduced.
The optional developer commands are pure C++ and can later serve
a native Ink interface instead of an external DX12 overlay.

The latest released game build remains alpha.8 until DreamUI actually
has a working, reversible native color override.


## Experimental native intent commands (source only)

The guarded serializer-stage build also recognizes `ink-apply` and
`ink-restore`. They are unavailable in the public alpha.8 and normal
builds. They **queue** a change for a later game-owned serializer callback.
`ink-restore` only succeeds when the same native resource generation is
authenticated again; a command acknowledgement does **not** mean that
the UI has been refreshed or the original color restored. Status exposes
pending intent, snapshot count and restoration failure count. No
developer command ever writes directly from the file watcher thread.


## Selecting a theme from installed .dream files

Place valid `.dream` archives in `Cyberpunk 2077/dreamui/themes/`
and start the game. The standalone ASI validates each package and lists
accepted identities with `themes`. Then write a command such as:

`select dreamui.default`

or use any accepted installed theme ID. The choice is saved in the bounded
file `dreamui/active-theme.txt` and loaded on the next launch. An invalid
ID falls back to the default theme. The file is atomically replaced,
and only catalog-validated IDs are accepted.

**Selection is not an immediate in-game UI change.** In ordinary builds
the preference is all that changes. In explicitly enabled experimental
builds the selected palette is applied only if the engine's authenticated
main-colors serializer callback next runs. An active restoration journal
must be cleared by a verified restore before changing themes.

No HUD Painter, DreamTools editor, Codeware, redscript, RED4ext or ImGui
dependency is introduced.

## Read-only experimental native Ink preflight (source builds only)

In a dedicated test installation, compile the ASI with
`-DDREAMUI_EXPERIMENTAL_NATIVE_STAGE=ON`, and before launch create:

`Cyberpunk 2077/dreamui/dev/native-ink-preview.enable`

When a validated `.dream` is installed, this enables **read-only**
serializer-stage checks. It does not write native colors. After launching,
`dreamui/logs/DreamUI-load.log` automatically captures the native status
and changed callback/preflight counters about once per second; no
`commands.txt` file or Tab press is required. The optional `status`
command is still available if desired.
A positive `preflight_ready` is an authenticated typed-color inspection,
not proof of a rendered color change.

An independent, additional marker `dreamui/dev/native-ink-write.enable`
must exist before launch to permit actual color mutation. Do not create
this marker for observation-only testing. `ink-apply` and `ink-restore`
are refused in observe-only mode. The marker state is fixed at launch.

**Caution:** Even the read-only mode installs an experimental native
function hook. It can still crash the game if the callback ABI or
lifecycle assumptions are wrong. It is unavailable in public alpha.8.
