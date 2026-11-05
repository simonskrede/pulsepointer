# PulsePointer

[pulsepointer_example.webm](https://github.com/user-attachments/assets/e91d36bb-18a7-48e4-85da-b2df4e3bec0a)

[![CI](https://github.com/simonskrede/pulsepointer/actions/workflows/ci.yml/badge.svg)](https://github.com/simonskrede/pulsepointer/actions/workflows/ci.yml)

PulsePointer makes the mouse pointer react to system audio in KDE Plasma 6. The default elastic mode expands and squashes the current cursor and can emit a ring on detected beats. Wobble cursor, spectrum halo, level bar, waveform circle, spectrum, and oscilloscope modes are also included.

## Supported environments

| Environment | Support |
|---|---|
| Kubuntu 26.04 LTS, Plasma Wayland | Supported |
| Kubuntu 26.04 LTS, Plasma X11 | Optional companion package |
| Kubuntu 26.10, Plasma Wayland | Supported |
| Kubuntu 26.10, Plasma X11 | Optional companion package |
| Arch Linux and compatible Arch-based distributions | Supported |
| KDE Plasma 6 / KWin 6.6, 6.7, 6.8 | Supported by the Arch build |
| Kubuntu 24.04 LTS / Plasma 5 | Not supported |
| Other Wayland compositors | Not supported |

The Rust daemon captures the default PulseAudio monitor (including PipeWire's PulseAudio compatibility service) and sends analyzed audio telemetry over a private Unix socket. A native KWin effect renders the visualization. This is necessary on Wayland, where ordinary applications cannot replace the global cursor.

## Install on Kubuntu 26.04 or 26.10

Release builds target Ubuntu 26.04 and 26.10 on `amd64`. Download the package matching your Ubuntu/Kubuntu release from the [latest release](https://github.com/simonskrede/pulsepointer/releases/latest) and install it.

For example, on 26.04:

```bash
sudo apt install ./pulsepointer_0.3.0-1+ubuntu26.04_amd64.deb
```

On 26.10, choose the package whose filename contains `ubuntu26.10` instead.

For a Plasma X11 session, install both packages:

```bash
sudo apt install ./pulsepointer_0.3.0-1+ubuntu26.04_amd64.deb \
  ./pulsepointer-kwin-x11_0.3.0-1+ubuntu26.04_amd64.deb
```

Log out and back in once so KWin discovers the native effect. The package enables the effect and the `pulsepointer.service` user service by default.

## Install on Arch Linux

PulsePointer supports Arch Linux and compatible Arch-based distributions through a dedicated installation script.

Unlike the Ubuntu packages, the Arch installation builds PulsePointer locally against the KWin development files installed on the target system. This avoids compatibility problems caused by distributing native KWin effect binaries built against a different KWin ABI.

### Compatibility

| Component | Supported versions |
|---|---|
| Arch Linux | Supported |
| Arch-based distributions | Supported, provided the required packages and KWin development files are available |
| KDE Plasma | 6.x |
| KWin | 6.6, 6.7, 6.8 |
| Qt | Qt 6 |
| Wayland | Supported |
| X11 | Requires a distribution providing `kwin-x11`; desktop testing pending |

Tested configuration: CachyOS, KDE Plasma 6, KWin 6.8.2, Wayland.

Current Arch Plasma 6.8 packages support Wayland. The X11 installation path
is for compatible distributions that still provide `kwin-x11`; the installer
stops before changing files if that package is neither installed nor available
in your repositories. See [Arch's KDE documentation](https://wiki.archlinux.org/title/KDE#Starting_Plasma).

Other Wayland compositors are not supported.

KWin effects use native APIs and are tied to the KWin version against which they are built. Rebuild PulsePointer after significant KWin upgrades if necessary.

### Requirements

- Arch Linux or a compatible Arch-based distribution.
- KDE Plasma 6.
- KWin 6.6, 6.7, or 6.8.
- An active Wayland or X11 session.
- Internet connection for installing dependencies and downloading Rust dependencies during the first build.
- `sudo` privileges.

### Quick start

Clone the repository and run the installer:

```bash
git clone https://github.com/simonskrede/pulsepointer.git
cd pulsepointer
./install.sh
```

The installer automatically:

1. Detects the Linux distribution and graphical session.
2. Installs the required build dependencies using pacman.
3. Checks the build environment.
4. Builds the Rust daemon.
5. Compiles the appropriate native KWin effect.
6. Installs PulsePointer and enables its user service.

No manual compilation or configuration is required.

### After installation

Log out of KDE Plasma and log back in once so KWin discovers the native effect.

The installer enables the KWin effect and the `pulsepointer.service` user service for graphical sessions.

Open:

**System Settings → Desktop Effects → PulsePointer**

You can configure the visualization mode, intensity, responsiveness, beat ring, color, and overlay size.

### Uninstallation

Run the same installer:

```bash
./install.sh
```

Select **Uninstall PulsePointer** from the interactive menu.

The uninstaller removes PulsePointer's installed files, restores previously backed-up files, and cleans up its installation records.

System dependencies are intentionally preserved.

Rust, Cargo, CMake, Ninja, Qt, KDE Frameworks, KWin, and other system packages are never automatically removed.

The source repository and user configuration are also left untouched.

### Building and installation details

The Arch installer builds the project directly on the target machine using the locally installed development libraries and tools.

This approach avoids distributing KWin effect binaries built against a different distribution or KWin ABI.

The installation is local and does not require an AUR package, a third-party package repository, or a prebuilt binary release.

## Configure the effect

Open System Settings, search for Desktop Effects, find PulsePointer, and select its configure button.

The GUI controls the mode, intensity, responsiveness, beat ring, color, and overlay size. Changes apply without restarting the daemon.

Elastic and Wobble animations keep the cursor hotspot (the exact click
position) fixed while the cursor changes shape. Enable **Show a red pixel at
the cursor hotspot** to mark that point with a tiny red locator that stays
the same size during animation. This option is off by default and applies to
the two cursor modes.

The daemon itself has only audio-input and diagnostic options:

```text
pulsepointer [OPTIONS]

      --source <SOURCE>  PulseAudio source to capture instead of @DEFAULT_MONITOR@
      --log-levels       Log the normalized audio level once per second
```

To pass an option to the packaged service, run:

```bash
systemctl --user edit pulsepointer
```

Add, for example:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/pulsepointer --source alsa_output.example.monitor
```

Then run:

```bash
systemctl --user restart pulsepointer
```

The environment variables `PULSEPOINTER_SOURCE` and `PULSEPOINTER_LOG=1` remain supported as alternatives.

## Build packages on Kubuntu 26.04 or 26.10

Build on the same fully updated Ubuntu/Kubuntu release and architecture as the systems that will install the package. A native KWin effect is tied to the exact KWin ABI against which it was built.

Source builds support both KWin 6.6 and 6.7. The effect detects the available image-item API at compile time, and CI builds on Ubuntu 26.04 and 26.10. Rebuild the native effect after upgrading KWin, even when the source is compatible.

Install the complete top-level build requirements for the Wayland package:

```bash
sudo apt update
sudo apt install build-essential ca-certificates cargo cmake extra-cmake-modules \
  kwin-dev libkf6kcmutils-dev libpulse-dev ninja-build pkg-config
```

`kwin-dev` supplies its required Qt 6, KF6 Config/CoreAddons, Epoxy, and Wayland development dependencies. `build-essential` supplies the C/C++ and Debian package toolchain. The first build also needs access to crates.io unless the locked Rust dependencies are already cached.

To build the optional X11 package too, also install:

```bash
sudo apt install kwin-x11-dev
```

Build only the main Wayland package:

```bash
./packaging/build-deb.sh --wayland-only
```

Or build the Wayland and X11 packages together:

```bash
./packaging/build-deb.sh
```

The script does not install anything and writes native-architecture packages to `dist/`. GitHub's release workflow runs this build for Ubuntu 26.04 and 26.10 on `amd64`. Release revisions and additional distribution targets are documented in [distribution-packaging.md](docs/distribution-packaging.md).

## Development checks

Install `rust-clippy` and `rustfmt` in addition to the package-build requirements, then run:

```bash
cargo fmt --check
cargo clippy --locked --all-targets -- --deny warnings
cargo test --locked
./packaging/build-deb.sh --wayland-only
ctest --test-dir target/package-build/kwin-wayland --output-on-failure
```

The IPC format is documented in [ipc-protocol.md](docs/ipc-protocol.md).

## Scope and security

- KDE Plasma 6 is required; other Wayland compositors need their own native integration.
- IPC stays under `$XDG_RUNTIME_DIR` with user-only permissions.
- KWin restores its native cursor if telemetry becomes stale, the screen locks, or the effect unloads.
- The default input is mixed system output, not the microphone.

## License

MIT
