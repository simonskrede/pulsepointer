# PulsePointer

[pulsepointer_example.webm](https://github.com/user-attachments/assets/e91d36bb-18a7-48e4-85da-b2df4e3bec0a)

[![CI](https://github.com/simonskrede/pulsepointer/actions/workflows/ci.yml/badge.svg)](https://github.com/simonskrede/pulsepointer/actions/workflows/ci.yml)

PulsePointer makes the pointer and desktop react to system audio in KDE Plasma 6. Combine Elastic or Wobble cursor animation with pointer visualizers and 20 independent screen effects: breathing or spiked borders, saturation and hue changes, ripples, auroras, particles, and more. The default remains an elastic cursor with a ring on musical attacks; screen effects start off.

## New in 0.4.0: the whole desktop joins in

**This is a major feature expansion.** Your music can light up the screen edges,
send glass-like ripples out from the mouse pointer, sweep auroras across the
desktop, and leave echoes of color behind.

- **20 screen effects to mix:** breathing frames, spectrum-driven spikes, liquid
  borders, corner shockwaves, hue tides, gravity wells, refractive ripples, prism
  kicks, particles, feedback ghosts, and more.
- **Build your own combination:** run Elastic or Wobble alongside any of the five
  pointer visualizers, the attack ring, and your favorite screen effects. Each
  screen effect has its own strength control.
- **Five starting points:** try Velvet, Voltage, Underwater, Aurora, or Meltdown,
  then tune the result to your music.
- **More control:** adjust border shapes, rise and fade times, separate pointer
  and screen intensity, and whether effects cover all displays or just the one
  containing the pointer. Bypass everything with **Meta+Ctrl+Alt+P**.
- **A precise pointer:** refractive ripples follow the mouse, and the optional
  hotspot locator is now a small, smooth red circle centered on the click point.

Existing settings migrate automatically, and screen effects stay off until you
enable them. Start with a preset under **System Settings → Desktop Effects →
PulsePointer**. See the [effect guide](docs/music-effects.md) for every effect and
the [changelog](CHANGELOG.md) for release details.

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
sudo apt install ./pulsepointer_0.4.0-1+ubuntu26.04_amd64.deb
```

On 26.10, choose the package whose filename contains `ubuntu26.10` instead.

For a Plasma X11 session, install both packages:

```bash
sudo apt install ./pulsepointer_0.4.0-1+ubuntu26.04_amd64.deb \
  ./pulsepointer-kwin-x11_0.4.0-1+ubuntu26.04_amd64.deb
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

Choose a preset or combine cursor, border, desktop color, and atmosphere effects independently. Each screen effect has its own strength and description.

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

The settings have five pages:

- **Mouse pointer:** choose Unchanged, Elastic, or Wobble, and freely combine level bar, waveform circle, spectrum bars, oscilloscope, and spectrum halo. The attack ring works independently too.
- **Screen borders:** enable any of six border effects, choose a smooth, rounded, triangular, or spiked profile, and adjust reach, density, and softness.
- **Desktop color:** combine saturation breathing, bass bloom, spectral lighting, hue tides, selective color, and color echoes.
- **Atmosphere & distortion:** mix gravity, refraction, prism kicks, aurora curtains, terrain, particles, feedback, and drop collapse.
- **Timing & displays:** set screen intensity, rise and fade times, and all displays or only the display containing the pointer.

The shared accent color applies to the pointer and decorative screen layers. Screen intensity is separate from pointer intensity. Every screen effect has an on/off checkbox, a description, and its own strength. **Use preset** fills in a combination that you can then adjust: Velvet, Voltage, Underwater, Aurora, or Meltdown. Press **Apply** to use your changes without restarting the daemon.

**Meta+Ctrl+Alt+P** temporarily bypasses or resumes every layer. Customize **Toggle PulsePointer** under System Settings → Shortcuts → KWin. The settings also have a persistent master enable switch. Existing configurations migrate automatically, preserving the previous cursor or pointer visualization without enabling screen effects.

Screen effects use OpenGL; pointer effects remain available if the screen renderer cannot initialize. Effects fade during silence and disappear when audio disconnects or the screen locks. See [music-effects.md](docs/music-effects.md) for the effect guide and rendering details.

Elastic and Wobble animations keep the cursor hotspot (the exact click
position) fixed while the cursor changes shape. Enable **Show a red marker at
the cursor hotspot** to mark that point with a small red circle. The marker
is centered on the click position, measures 5 logical pixels across, follows
display scaling, and stays the same size during animation. This option is off
by default and works with Unchanged, Elastic, and Wobble cursors.

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
  kwin-dev libkf6globalaccel-dev libkf6kcmutils-dev libpulse-dev ninja-build pkg-config
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

Native tests cover cursor geometry, hotspot-marker shape and placement across display scales, settings migration and combinations, settings-dialog persistence, time-based audio envelopes, and actual shader rendering for every screen effect. Shader tests use surfaceless EGL with Mesa software rendering and skip when EGL is unavailable. To save test previews, set `PULSEPOINTER_TEST_ARTIFACTS=/tmp/pulsepointer-previews` when running `ctest`.

The IPC format is documented in [ipc-protocol.md](docs/ipc-protocol.md).

## Scope and security

- KDE Plasma 6 is required; other Wayland compositors need their own native integration.
- IPC stays under `$XDG_RUNTIME_DIR` with user-only permissions.
- KWin restores its native cursor if telemetry becomes stale, the screen locks, or the effect unloads.
- The default input is mixed system output, not the microphone.

## License

MIT
