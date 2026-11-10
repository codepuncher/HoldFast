# Releasing

## Versioning

The version isn't stored in the repo.
The Release workflow computes it from the latest `vX.Y.Z` tag and its `bump` input (`scripts/next-version.sh`), then hands it to CMake as `HOLDFAST_VERSION`.
CMake embeds it in the DLL's file version and the SKSE plugin version, which the SKSE log prints on load.
Local builds use the `0.0.0` default.

## Making a release

1. Open **Actions > Release > Run workflow** on `main`.
   Leave `kind` as `release` and set `bump` to `patch`, `minor`, `major`, or an explicit `X.Y.Z`.
   The workflow builds the plugin with that version, packages it via `scripts/package.sh`, tags the commit `vX.Y.Z`, and publishes a GitHub Release with the zip and PDB.
   A release run from any branch other than `main` fails.
   Release and test runs both fail on a commit that already has a release tag.
   If a run fails after creating the tag (for example during the asset upload), delete the GitHub Release and the tag before running it again.
2. Run `nexus-upload.yml` via **workflow_dispatch** with the version (no `v` prefix).
   See [Nexus Mods upload](#nexus-mods-upload).
3. If the Nexus page changed, regenerate it (see below) and paste it into the Nexus Mods page editor.

## Test builds

To give someone a build of a branch, run **Actions > Release > Run workflow** on that branch with `kind` set to `test`.
Set `label` to a short `[a-z0-9-]+` name, for example `vr-support`.
The workflow publishes a GitHub prerelease tagged `vX.Y.Z-<label>.<run number>`, where `X.Y.Z` is the version `bump` gives.
The DLL inside reports `X.Y.Z.N`, with `N` from 1 to 15 taken from the run number, so the SKSE log tells a test build apart from the final release.
Test builds aren't marked as the latest release, never upload to Nexus, and don't count as the base for the next release.

## Nexus Mods page

The `<!-- nexus:start/end -->` block in [README.md](../README.md) is the **source of truth** for Requirements, Installation, and Configuration.
[nexus-page.md](nexus-page.md) holds the rest of the page.

To update the Nexus page:

1. Edit Requirements, Installation, or Configuration inside the `<!-- nexus:start/end -->` block in [README.md](../README.md).
2. Edit everything else directly in [nexus-page.md](nexus-page.md).
   > **Do not edit** the `<!-- generated:start/end -->` block in [nexus-page.md](nexus-page.md): it is overwritten every time the script runs.
3. Generate the combined BBCode output:

```bash
python3 scripts/generate-nexus-page.py
```

4. Paste the output into the Nexus Mods page editor.

## Nexus Mods upload

`nexus-upload.yml` runs manually via **workflow_dispatch** with the version (no `v` prefix).
It checks out the `vX.Y.Z` tag, generates release notes with git-cliff, downloads the zip from the GitHub Release, and uploads it to Nexus Mods.
It also posts the notes to the mod's Changelog tab.
That endpoint appends, so re-running a successful upload for the same version adds a duplicate entry.
Each upload archives the previous file version, marks the new file as the primary mod manager download, and shows the requirements pop-up.

**Prerequisites (one-time setup):**

1. Upload your first file manually via the [Nexus Mods web UI](https://www.nexusmods.com) to create the file group.
2. Add to the repository:
   - **Secret** `NEXUSMODS_API_KEY`: your Nexus Mods API key
   - **Variable** `NEXUSMODS_FILE_GROUP_ID`: the file group ID
   - **Variable** `NEXUSMODS_MOD_ID`: the mod's internal ID, used for the Changelog tab

## CI

| Workflow           | Trigger                                                                                                | What it does                                                                                                                                |
| ------------------ | ------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `ci.yml`           | PRs to `main` touching source/cmake/vcpkg                                                              | clang-format (ubuntu) → build + test (windows, parallel) → clang-tidy (windows)                                                             |
| `release.yml`      | Manual `workflow_dispatch` (see [Making a release](#making-a-release) and [Test builds](#test-builds)) | Computes the version, builds, packages via `scripts/package.sh`, tags the commit, publishes a GitHub Release (or prerelease) with zip + PDB |
| `nexus-upload.yml` | Manual `workflow_dispatch` (see [Nexus Mods upload](#nexus-mods-upload))                               | Downloads the release zip, generates cliff release notes, uploads to Nexus Mods                                                             |
| `lint.yml`         | PRs touching scripts, markdown docs, or markdown lint config/workflow files                            | shellcheck (scripts) + markdown checks (`dprint` and `vale`)                                                                                |
| `pr-title.yml`     | PR opened/edited/synchronised                                                                          | Validates PR title follows Conventional Commits                                                                                             |
