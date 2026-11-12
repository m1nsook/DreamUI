# DreamUI v0.1.0-alpha.8 — on-demand Tab diagnostics

**This release is a read-only diagnostic tool.** It does not recolor the game or add UI widgets.

## What's different

There are no automatic 3/8/15/32-second probes and no automatic engine scans at startup. DreamUI loads the .dream theme package, then listens for Tab **without intercepting it**.

**Press Tab whenever you want another read.** The game can stay running for hours: every distinct press makes one fresh ResourceLoader/Ink cache inspection. You do **not** need to exit or restart Cyberpunk to collect more results.

## How to test

1. Extract this ZIP over your current DreamUI.asi and Default.dream; keep your existing ASI loader. Do not overwrite version.dll.
2. Start Cyberpunk normally. Press Space past its title screen, enter the actual main menu and wait until it finishes loading.
3. Press **Tab once**, then open Cyberpunk 2077/dreamui/logs/DreamUI-probe.log. If the game takes time to initialize, press Tab again later; there is no deadline and no automatic stopping.
4. You can press Tab multiple times, enter gameplay, come back to the menu, and press Tab again **without restarting**. Each key press creates an individually numbered probe section.
5. Send the latest DreamUI-probe.log when you're ready.

The diagnostic file is **overwritten at the beginning of each game launch**, rather than indefinitely appending older alpha logs. It is empty until the first Tab press. DreamUI.log still contains a short startup record confirming ASI and theme loading.

## Controls and safety

- Plain **Tab**, while the Cyberpunk window is focused, triggers one probe on the key-down edge. Hold Tab and it won't continuously repeat.
- Alt+Tab, Shift+Tab, and Windows+Tab are ignored, so normal window switching and Steam overlay shortcuts don't trigger DreamUI.
- This does not suppress Cyberpunk's usual Tab behavior (for example, the scanner during gameplay).
- On the first Tab press, DreamUI reads the executable fingerprint and game-provided address table once. **Further presses reuse the resolved pointer address** and only perform the current read-only cache inspection.
- Every probe checks whether ResourceLoader is initialized. An empty cache at the title screen simply means you should press Tab again after the main menu is ready.
- Other executable versions fail closed. No game-engine methods, hooks or memory writes are made.

A matching cache token is not yet proof of a populated style or valid HDRColor. Values are treated as unverified candidates. The native DreamUI menu control tile and one-ZIP integrated loader are still future features.
