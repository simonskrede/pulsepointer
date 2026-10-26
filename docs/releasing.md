# Releasing PulsePointer

Use **Release** for a new application version, or **Build release packages**
to rebuild an existing version against updated KWin libraries. Both actions
build `amd64` packages for Ubuntu/Kubuntu 26.04 and 26.10, including the
Wayland package and optional X11 companion, and publish them to GitHub Releases.

## Publish a new version

1. Merge the changes you want to release into `main` and wait for CI to pass.
2. Open [GitHub Actions](https://github.com/simonskrede/pulsepointer/actions).
3. Select **Release** in the workflow list, then **Run workflow**.
4. Leave the branch set to **main** and choose the version increment:

   | Increment | Example | Use for |
   |---|---|---|
   | `patch` | `0.2.1` → `0.2.2` | Fixes and compatibility updates |
   | `minor` | `0.2.1` → `0.3.0` | New features |
   | `major` | `0.2.1` → `1.0.0` | A major release or incompatible changes |

5. Click **Run workflow** and open the new run to follow its progress.
6. When the run succeeds, download the packages from
   [GitHub Releases](https://github.com/simonskrede/pulsepointer/releases).

The action updates `Cargo.toml`, the application entry in `Cargo.lock`, the
KWin CMake project version, and the package examples in the README and
packaging guide. It checks formatting, runs Clippy and the Rust tests, then
commits the version bump and creates an annotated `vX.Y.Z` tag. It builds
packages for both Ubuntu versions and publishes them with compatibility
metadata, build-provenance attestations, and SHA-256 checksums.

You do not need to edit version files, open a release PR, or push a tag.
Afterward, update your local checkout with:

```bash
git switch main
git pull --ff-only origin main
git fetch --tags origin
```

The workflow must be merged into `main` before its button is available. It
uses the built-in `GITHUB_TOKEN`; no personal access token or extra secret is
needed. The token must be allowed to write repository contents, as requested
by the workflow. Branch rules must allow the action to push release commits
to `main`.

## Rebuild packages for an existing version

Use this when KWin has changed but the application source can stay the same.

1. Open **Actions → Build release packages → Run workflow**.
2. Leave the workflow branch set to **main**, so the current packaging
   workflow is used.
3. Enter the existing release tag, such as `v0.2.1`. The build checks out that
   tag's source, not the latest application code on `main`.
4. Set **Package rebuild number** to a revision higher than any already
   published for that tag, for example `2` after revision `1`.
5. Enable **Upload packages to the GitHub Release** and click **Run workflow**.
6. Check the completed run and download the new assets from the existing release.

For example, revision `2` produces
`pulsepointer_0.2.1-2+ubuntu26.10_amd64.deb`, plus the 26.04 and X11 packages.
The higher revision lets APT recognize the package as newer. Published files
are never overwritten. Leave publishing disabled if you only want to build
and inspect the Actions artifacts.

If you need application source changes, use **Release** for a new version.

## If a release run fails

- If **Check and tag the new version** fails before its push, no release
  commit or tag was published. Fix the problem and run **Release** again.
- If someone updates `main` while preparation is running, its atomic push
  fails without publishing either the commit or tag. Start a fresh run from
  the updated `main`.
- If **Build and publish packages** fails after the tag was pushed, use
  **Re-run failed jobs** on that run. This rebuilds the same tagged version
  instead of incrementing it again. The existing tag can also be built with
  **Build release packages** using revision `1` if no assets were published,
  or a higher revision if any assets already exist.
- If the tagged source needs a fix, merge that fix and run **Release** to
  publish the next version.

New-version release runs are serialized. Package publication happens only
after both Ubuntu package builds succeed. Packages are tied to the exact
KWin library version recorded in the release's compatibility metadata.

## Command-line alternative

With GitHub CLI authenticated, start the same actions from this checkout:

```bash
# Publish the next patch version.
gh workflow run new-release.yml --ref main -f bump=patch

# Rebuild an existing release with a higher package revision.
gh workflow run release.yml --ref main \
  -f release_tag=v0.2.1 -f package_revision=2 -f publish=true
```

Follow the runs with `gh run list` or the GitHub Actions page. See
[Distribution packaging](distribution-packaging.md) for package internals
and adding another Ubuntu release.
