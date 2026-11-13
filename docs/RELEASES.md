# Testing release process

DreamUI is in the experimental bootstrap stage. Only publish **prereleases** until native Ink hooks and real gameplay integration have passed acceptance tests.

## Bootstrap releases

Alpha.1 through alpha.8 are historical test releases. Alpha.9 is the first release whose ZIP is built with the guarded native serializer observer **compiled in**. Every release still requires Windows CI and is published only as a prerelease.

## Future prereleases

In GitHub, select **Actions → Experimental test releases → Run workflow**, select branch `main`, and enter a new tag such as `v0.1.0-alpha.9`. It will:

1. Validate the prerelease tag.
2. Run Python and native C++ tests.
3. Build the Windows x64 `DreamUI.asi`. For alpha.9, compile the guarded serializer stage ON but both unsafe write and clone options OFF. Only the `native-ink-preview.enable` file can activate its read-only observer.
4. Pack the sample `Default.dream`.
5. Create a ZIP laid out for extraction into the game directory.
6. Produce a SHA-256 checksum, upload an Actions artifact, and publish a **GitHub prerelease**.

Tags are never reused or overwritten; re-running a previously released tag skips publication. The workflow does not publish full releases or mark anything as the latest stable version.

The alpha.9 test ZIP does not contain a working native UI recoloring feature or an external ASI loader. It carries an **opt-in** native loading observer; a test marker is needed to activate that hook. See [TESTING.md](TESTING.md) for safety notes and instructions.

## Labels

`.github/labels.json` is the source of truth for repository labels. The **Initialize and sync labels** workflow creates/updates labels on the default branch and **does not delete** existing labels. Pull requests are categorized by changed file paths using `actions/labeler`; issues are categorized by keywords when GitHub Issues are enabled.

This automation intentionally avoids creating a new issue or test release on every CI run.
