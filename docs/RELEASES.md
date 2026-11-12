# Testing release process

DreamUI is in the experimental bootstrap stage. Only publish **prereleases** until native Ink hooks and real gameplay integration have passed acceptance tests.

## First bootstrap release

On the first merge introducing `.github/workflows/release-testing.yml`, the workflow creates `v0.1.0-alpha.1` after building and running both Python and C++ tests on `windows-latest`. If the build fails, no GitHub Release should be published.

## Future prereleases

In GitHub, select **Actions → Experimental test releases → Run workflow**, select branch `main`, and enter a new tag such as `v0.1.0-alpha.2`. It will:

1. Validate the prerelease tag.
2. Run Python and native C++ tests.
3. Build the Windows x64 `DreamUI.asi`.
4. Pack the sample `Default.dream`.
5. Create a ZIP laid out for extraction into the game directory.
6. Produce a SHA-256 checksum, upload an Actions artifact, and publish a **GitHub prerelease**.

Tags are never reused or overwritten; re-running a previously released tag skips publication. The workflow does not publish full releases or mark anything as the latest stable version.

The test ZIP does not contain a native UI recoloring feature or an external ASI loader. See [TESTING.md](TESTING.md) for safety notes and instructions.

## Labels

`.github/labels.json` is the source of truth for repository labels. The **Initialize and sync labels** workflow creates/updates labels on the default branch and **does not delete** existing labels. Pull requests are categorized by changed file paths using `actions/labeler`; issues are categorized by keywords when GitHub Issues are enabled.

This automation intentionally avoids creating a new issue or test release on every CI run.
