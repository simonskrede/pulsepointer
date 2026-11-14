# Distribution packaging

PulsePointer contains a native KWin effect. Packages therefore target a
distribution release and architecture, not merely a generic Linux ABI. Ubuntu and
Kubuntu of the same version use the same archive, so an `ubuntu26.04` package
is also the Kubuntu 26.04 package.

## Supported releases

| Distribution | Plasma | Status |
|---|---:|---|
| Ubuntu/Kubuntu 26.04 LTS | 6.6 | Supported; Wayland package and optional X11 package |
| Ubuntu/Kubuntu 26.10 | 6.7 | Supported; Wayland package and optional X11 package |
| Fedora KDE 45 | 6.8 | Wayland RPM build and CI target; desktop testing pending |
| Ubuntu/Kubuntu 25.10 | 6.4 | Not published; interim support ended in July 2026 |
| Ubuntu/Kubuntu 24.04 LTS | 5.27 | Unsupported; the effect uses the KWin 6 API |

Only add a release to the build matrix if its normal package repositories are
still supported and provide KWin 6 development headers. Old interim releases
move to archive servers and make poor long-term binary targets.

## GitHub release workflow

The release workflow builds for Ubuntu 26.04 and 26.10 on `amd64`, each inside
its official `ubuntu:<version>` container, and Fedora 45 on `x86_64` inside
`registry.fedoraproject.org/fedora:45`. Headers, libraries, dependency names,
and generated package metadata come from each target distribution's archives.

1. Update the version in `Cargo.toml`, `Cargo.lock`, the CMake project, and
   `packaging/pulsepointer.spec`. Add
   release notes under the matching version heading in `CHANGELOG.md` and update
   the README's highlights and installation examples.
2. Merge and verify that the CI workflow passes.
3. Create and push a matching tag, for example `v0.4.0`.
4. The release workflow builds the Wayland and X11 packages, generates GitHub
   build-provenance attestations, and attaches the `.deb` and `.rpm` files plus SHA-256
   checksums to a GitHub Release.

New releases use the matching `CHANGELOG.md` entry as their release notes.

The package names identify the target, for example:

```text
pulsepointer_0.4.0-1+ubuntu26.04_amd64.deb
pulsepointer-kwin-x11_0.4.0-1+ubuntu26.04_amd64.deb
pulsepointer_0.4.0-1+ubuntu26.10_amd64.deb
pulsepointer-kwin-x11_0.4.0-1+ubuntu26.10_amd64.deb
pulsepointer-0.4.0-1.fc45.x86_64.rpm
pulsepointer-0.4.0-1.fc45.src.rpm
```

For a later KWin update within the same distribution release, run **Build release
packages** manually against the same release tag, set the package revision to
`2`, and enable publishing. This produces a newer package version without
changing the PulsePointer application version. Never replace an already
published package asset; publish a higher revision. Compatibility metadata
filenames also include the revision so older build records remain available.

To add packaging support to an existing application version, first push the
support changes, then run the workflow from that branch. Set **release_tag**
to the existing application tag and **source_ref** to the exact new commit SHA.
The source version must still match the release tag. Leave **source_ref** empty
to build the tag itself. This keeps the published tag in place while recording
the actual source commit in each package's compatibility metadata and build
attestation. Leave **publish** disabled to download packages as Actions
artifacts, or enable it to attach a new revision to the existing release.

## Fedora RPMs

`packaging/pulsepointer.spec` builds the Rust daemon, native Wayland effect,
settings module, documentation, and systemd user service. The Qt plugins use
Fedora's Qt 6 plugin directory (`/usr/lib64/qt6/plugins` on `x86_64`), while
the unit uses `/usr/lib/systemd/user`. The graphical-session target starts the
daemon at login. Fedora systemd user scriptlets handle package transactions.
The existing audio implementation uses `pipewire-pulseaudio`, required by the
RPM, without a separate Fedora audio backend.

Build on a fully updated Fedora 45 system:

```bash
sudo dnf install cargo rpm-build dnf5-plugins
sudo dnf builddep packaging/pulsepointer.spec
./packaging/build-rpm.sh
```

The wrapper copies the current working source, checks matching application,
CMake, and spec versions, and vendors `Cargo.lock` dependencies into the source
archive. The RPM build and tests use Cargo's frozen mode, without network
access. The generated `.src.rpm` can be rebuilt using `rpmbuild --rebuild`
after installing the same build requirements. These are upstream release
packages; they are not submissions to Fedora's distribution repositories.

RPM automatically generates shared-library requirements. The spec additionally
requires the exact architecture-qualified `kwin-libs` epoch, version, and
release used during the build, because the KWin SONAME alone does not guarantee
native effect ABI compatibility. Rebuild and increment `PACKAGE_REVISION`
after a KWin update. Users may need to remove PulsePointer while waiting for
a matching rebuild to permit a KWin upgrade.

Fedora CI and release jobs run Rust tests and all native CTest suites, including
desktop OpenGL and GLES shader rendering with Mesa, as part of `rpmbuild`.
They also install the binary RPM with DNF, verify its files, and test removal.
Package metadata records
the KWin version and RPM requirements; checksums and attestations include both
binary and source RPMs. Inspect packages with:

```bash
rpm -qpi dist/pulsepointer-*.x86_64.rpm
rpm -qpl dist/pulsepointer-*.x86_64.rpm
rpm -qpR dist/pulsepointer-*.x86_64.rpm
```

Before declaring desktop validation complete, test in a disposable Fedora KDE
45 virtual machine: install the RPM, log out and back in, confirm the user
service captures system output, enable the effect and open its configuration,
try pointer and screen effects, then check upgrade and removal. Containers
cannot verify effect loading in a real KWin session. Fedora Workstation/GNOME,
Fedora Atomic desktops, and X11 are outside this RPM target.

## Adding another Ubuntu release

Add the version to the matrix in `.github/workflows/release.yml`, then add a
matching CI job or change the CI target. Test these assumptions inside that
release's container:

- `kwin-dev` provides the KWin 6 CMake target and headers.
- `kwin-x11-dev` exists if an X11 companion is being published.
- `libkwin6` and `libkwin-x11-6` are the corresponding runtime package names.
- `libkf6globalaccel-dev` provides the configurable quick-bypass shortcut API.
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

1. Download the package matching the distribution release (`ubuntu26.04`,
   `ubuntu26.10`, or `fc45`) and machine architecture.
2. Run `sudo apt install ./pulsepointer_…deb` on Ubuntu/Kubuntu, or
   `sudo dnf install ./pulsepointer-….rpm` on Fedora KDE.
3. Log out and back in once.

Release notes should state the distribution version, KWin package version, supported
session types, package revision, and SHA-256 checksum. The Actions build badge
and artifact attestation provide useful confidence that release binaries came
from the tagged source.

GitHub Releases are downloads, not an APT or DNF repository, so they do not provide
automatic upgrades. If the project gains regular users, the next distribution
step should be a signed APT repository or Launchpad PPA. That lets `apt upgrade`
deliver rebuilds whenever Ubuntu updates KWin. Repository signing keys and
publishing credentials should be stored as GitHub environment secrets, with
publishing restricted to protected tags.
