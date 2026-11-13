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
