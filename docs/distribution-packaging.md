# Distribution packaging

PulsePointer contains a native KWin effect. Packages therefore target an
Ubuntu release and architecture, not merely a generic Linux ABI. Ubuntu and
Kubuntu of the same version use the same archive, so an `ubuntu26.04` package
is also the Kubuntu 26.04 package.

## Supported releases

| Distribution | Plasma | Status |
|---|---:|---|
| Ubuntu/Kubuntu 26.04 LTS | 6.6 | Supported; Wayland package and optional X11 package |
| Ubuntu/Kubuntu 25.10 | 6.4 | Not published; interim support ended in July 2026 |
| Ubuntu/Kubuntu 24.04 LTS | 5.27 | Unsupported; the effect uses the KWin 6 API |

Only add a release to the build matrix if its normal package repositories are
still supported and provide KWin 6 development headers. Old interim releases
move to archive servers and make poor long-term binary targets.

## GitHub release workflow

The release workflow builds inside an official `ubuntu:<version>` container.
This ensures the headers, libraries, dependency names, and generated package
metadata all come from the target Ubuntu archive.

1. Update the version in `Cargo.toml` and the CMake project.
2. Merge and verify that the CI workflow passes.
3. Create and push a matching tag, for example `v0.2.0`.
4. The release workflow builds the Wayland and X11 packages, generates GitHub
   build-provenance attestations, and attaches the `.deb` files plus SHA-256
   checksums to a GitHub Release.

The package names identify the target, for example:

```text
pulsepointer_0.2.0-1+ubuntu26.04_amd64.deb
pulsepointer-kwin-x11_0.2.0-1+ubuntu26.04_amd64.deb
```

For a later KWin update within the same Ubuntu release, run **Build release
packages** manually against the same release tag, set the package revision to
`2`, and enable publishing. This produces a newer Debian version without
changing the PulsePointer application version. Never replace an already
published package asset; publish a higher revision.

## Adding another Ubuntu release

Add the version to the matrix in `.github/workflows/release.yml`, then add a
matching CI job or change the CI target. Test these assumptions inside that
release's container:

- `kwin-dev` provides the KWin 6 CMake target and headers.
- `kwin-x11-dev` exists if an X11 companion is being published.
- `libkwin6` and `libkwin-x11-6` are the corresponding runtime package names.
- `libkf6kcmutils-dev` provides the native Desktop Effects configuration module
  API.
- The generated package can be installed on a fully updated Kubuntu system of
  that exact release.

The build script uses `dpkg-shlibdeps` for ordinary shared-library dependencies
and adds an exact dependency for the KWin library. Inspect every artifact with:

```bash
dpkg-deb --info dist/*.deb
dpkg-deb --contents dist/*.deb
```

Installation testing should happen in a disposable Kubuntu virtual machine,
because a container cannot start a real KWin graphical session.

For rootless local build experiments, development packages can instead be
extracted under a temporary directory and supplied as
`PULSEPOINTER_SDK_ROOT=/path/to/sdk`. Normal release builds should use packages
installed inside the clean target container so dependency checks remain active.

## Presenting the project on GitHub

The README should lead with a short Wayland recording or screenshot, followed
by a prominent compatibility statement and a link to the latest release. Keep
the first install path simple:

1. Download the `ubuntu26.04` package for the machine architecture.
2. Run `sudo apt install ./pulsepointer_…deb`.
3. Log out and back in once.

Release notes should state the Ubuntu version, KWin package version, supported
session types, package revision, and SHA-256 checksum. The Actions build badge
and artifact attestation provide useful confidence that release binaries came
from the tagged source.

GitHub Releases are downloads, not an APT repository, so they do not provide
automatic upgrades. If the project gains regular users, the next distribution
step should be a signed APT repository or Launchpad PPA. That lets `apt upgrade`
deliver rebuilds whenever Ubuntu updates KWin. Repository signing keys and
publishing credentials should be stored as GitHub environment secrets, with
publishing restricted to protected tags.
