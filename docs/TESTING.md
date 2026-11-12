# DreamUI v0.1.0-alpha.4 — independent address resolver test

This is a **diagnostic prerelease**: no engine functions, memory writes, UI patches or menu widgets are enabled.

Install over alpha.3, using your existing ASI loader. Do not replace version.dll. Launch Cyberpunk, visit the main menu, then inspect dreamui/logs/DreamUI.log.

New expected log lines:

    Address DB: game linker map validated, entries_scanned=... supported_symbols=.../4
    Engine symbol hash=...: segment=... rva=... (address only, not invoked)
    ResourceLoader singleton: pointer present (not dereferenced)

A null pointer can be normal at early startup. A missing or mismatched database causes a logged refusal instead of an unsafe fallback:

    Address DB: rejected (...) ; native probe disabled

This build reads the game's own bin/x64/cyberpunk2077_addresses.json directly. It does not depend on RED4ext.dll, and the database is **not** bundled or redistributed. Standard PC distributions usually provide it, but availability depends on platform.

No vanilla Ink resource is read, and the UI should look unchanged. Do not upload copyrighted game executables or the full address table; the log is sufficient. The standalone ASI loader and native DreamUI main-menu tile remain future milestones.
