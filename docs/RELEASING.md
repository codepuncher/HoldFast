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
2. `nexus-upload.yml` starts once `release.yml` finishes.
   Approve the pending `nexus` deployment with **Review deployments** on the workflow run's page to let the upload proceed.
   If it doesn't start, or you need to re-run a failed upload, run it manually via **workflow_dispatch** with the version (no `v` prefix).
   The version must match an existing `vX.Y.Z` tag exactly, since a manual run checks out that tag.
   See [Nexus Mods upload](#nexus-mods-upload) for the environment setup.
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

`nexus-upload.yml` runs on a `workflow_run` trigger chained off `release.yml`.
`release.yml` publishes the GitHub Release with the default `GITHUB_TOKEN`, and GitHub doesn't start a `release: published` workflow from events caused by `GITHUB_TOKEN`.
`workflow_run` fires when `release.yml` itself completes, whatever token published inside it.
Test builds are skipped.
The `upload` job runs under the `nexus` [GitHub environment](https://docs.github.com/en/actions/deployment/targeting-different-environments/using-environments-for-deployment), which requires manual approval before the job proceeds and scopes the Nexus secrets to that environment.
A manual **workflow_dispatch** run with the version (no `v` prefix), for example to re-run a failed upload, needs the same approval.

Each upload archives the previous file version, sets the mod's version on Nexus, marks the new file as the primary mod manager download, and shows the requirements pop-up.
It also posts the git-cliff notes to the mod's Changelog tab.
That endpoint appends, so re-running a successful upload for the same version adds a duplicate entry.

`workflow_run` always runs the copy of `nexus-upload.yml` committed to `main`, so changes to this workflow only take effect after merging to `main`.

Do not restrict the `nexus` environment to tags.
`workflow_run` jobs always run against the default branch's ref (`refs/heads/main`), not the tag from the upstream `release.yml` run, so a tag-only policy blocks every automatic upload.
A deployment-branch policy limited to `main` works with the automatic trigger and keeps `workflow_dispatch` runs from other branches away from the secrets.

**Prerequisites (one-time setup):**

1. Upload your first file manually via the [Nexus Mods web UI](https://www.nexusmods.com): this creates the file that later uploads add versions to.
2. Note its file ID and the mod ID from the **Advanced** option on the mod page's Files tab, or from the file's edit menu on the Manage Files page.
3. Create the `nexus` environment (Settings → Environments → New environment) with a required reviewer, before the workflow referencing it is merged to `main`.
   Otherwise GitHub creates it unprotected on first reference.
4. Add to the `nexus` environment as secrets (Settings → Environments → `nexus` → Environment secrets):
   - `NEXUSMODS_API_KEY`: your Nexus Mods API key
   - `NEXUSMODS_FILE_ID`: the file ID
   - `NEXUSMODS_MOD_ID`: the mod ID, used to post each release's notes to the mod's Changelog tab.
     It is **not** the number in the mod page URL.
5. Add to the `nexus` environment as a variable (Settings → Environments → `nexus` → Environment variables):
   - `NEXUSMODS_DISPLAY_NAME`: the file name shown on Nexus

## CI

| Workflow           | Trigger                                                                                                                                                                             | What it does                                                                                                                                |
| ------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `ci.yml`           | PRs to `main` touching source/cmake/vcpkg                                                                                                                                           | clang-format (ubuntu) → build + test (windows, parallel) → clang-tidy (windows)                                                             |
| `release.yml`      | Manual `workflow_dispatch` (see [Making a release](#making-a-release) and [Test builds](#test-builds))                                                                              | Computes the version, builds, packages via `scripts/package.sh`, tags the commit, publishes a GitHub Release (or prerelease) with zip + PDB |
| `nexus-upload.yml` | Starts via `workflow_run` once `release.yml` completes, gated on approval in the `nexus` environment; also manual `workflow_dispatch` (see [Nexus Mods upload](#nexus-mods-upload)) | Downloads the release zip, generates cliff release notes, uploads to Nexus Mods                                                             |
| `lint.yml`         | PRs touching scripts, markdown docs, or markdown lint config/workflow files                                                                                                         | shellcheck (scripts) + markdown checks (`dprint` and `vale`)                                                                                |
| `pr-title.yml`     | PR opened/edited/synchronised                                                                                                                                                       | Validates PR title follows Conventional Commits                                                                                             |
