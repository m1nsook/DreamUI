# DreamUI pre-alpha — native startup test ONLY

This prerelease is **not a functional HUD or UI recolor mod**. It verifies whether an early DreamUI Windows ASI prototype can load and detect valid `.dream` archives. No Ink hooks, color overrides, map-marker changes or custom backgrounds are implemented yet.

## Test on Cyberpunk 2077 2.5 (Windows x64)

1. Make a backup of your game directory and saves before testing native mods.
2. A compatible ASI loader must already be installed. **Do not replace an existing `version.dll`** just for DreamUI.
3. Extract the test ZIP into the Cyberpunk 2077 game directory. It contains `bin/x64/plugins/DreamUI.asi` and `dreamui/themes/Default.dream`.
4. Launch the game normally, then quit. The game should look unchanged.
5. Check `Cyberpunk 2077/dreamui/logs/DreamUI.log` for the native startup line and a recognized `Default.dream` archive.

If there is no log, the ASI may not have loaded (or your current loader uses a different location). If you see a crash or incompatibility, remove `DreamUI.asi` and the `dreamui` directory, and report your game version, launcher and existing loaders.

## Limitations

- The test ZIP does **not** bundle an ASI loader yet; a one-ZIP self-contained installer is a future goal.
- The runtime ZIP reader currently accepts **only uncompressed ZIP_STORED** members and validates ZIP CRCs; `.dream` authors should use the included Python packer.
- In-game UI and Ink styling have not been tested or implemented.
- No paid or proprietary mod files are included.

## Contributing

Please attach your log and reproduction details to a GitHub issue when Issues are enabled. Never upload game executables, save files containing personal information, or purchased mod assets.
