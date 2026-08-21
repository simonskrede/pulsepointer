# PulsePointer

[![CI](https://github.com/simonskrede/pulsepointer/actions/workflows/ci.yml/badge.svg)](https://github.com/simonskrede/pulsepointer/actions/workflows/ci.yml)

PulsePointer makes the mouse pointer react to system audio in KDE Plasma 6.
The default elastic mode expands and squashes the current cursor and can emit a
ring on detected beats. Wobble cursor, spectrum halo, level bar, waveform
circle, spectrum, and oscilloscope modes are also included.

| Environment | Support |
|---|---|
| Kubuntu 26.04 LTS, Plasma Wayland | Supported |
| Kubuntu 26.04 LTS, Plasma X11 | Optional companion package |
| Kubuntu 24.04 LTS / Plasma 5 | Not supported |
| Other Wayland compositors | Not supported |

The Rust daemon captures the default PulseAudio monitor (including PipeWire's
PulseAudio compatibility service) and sends analyzed audio telemetry over a
private Unix socket. A native KWin effect renders the visualization. This is
necessary on Wayland, where ordinary applications cannot replace the global
cursor.

## Install on Kubuntu 26.04

GitHub releases currently provide `amd64` packages. Download the package from
the [latest release](https://github.com/simonskrede/pulsepointer/releases/latest)
and install it:

```bash
sudo apt install ./pulsepointer_0.2.0-1+ubuntu26.04_amd64.deb
```

For a Plasma X11 session, install both packages:

```bash
sudo apt install ./pulsepointer_0.2.0-1+ubuntu26.04_amd64.deb \
  ./pulsepointer-kwin-x11_0.2.0-1+ubuntu26.04_amd64.deb
```

Log out and back in once so KWin discovers the native effect. The package
enables the effect and the `pulsepointer.service` user service by default.

## Configure the effect

Open **System Settings**, search for **Desktop Effects**, find **PulsePointer**,
and select its configure button. The GUI controls the mode, intensity,
responsiveness, beat ring, color, and overlay size. Changes apply without
restarting the daemon.

The daemon itself has only audio-input and diagnostic options:

```text
pulsepointer [OPTIONS]

      --source <SOURCE>  PulseAudio source to capture instead of @DEFAULT_MONITOR@
      --log-levels       Log the normalized audio level once per second
```

To pass an option to the packaged service, run
`systemctl --user edit pulsepointer` and add, for example:

```ini
[Service]
ExecStart=
ExecStart=/usr/bin/pulsepointer --source alsa_output.example.monitor
```

Then run `systemctl --user restart pulsepointer`. The environment variables
`PULSEPOINTER_SOURCE` and `PULSEPOINTER_LOG=1` remain supported as alternatives.

## Build packages on Kubuntu 26.04

Build on the same fully updated Ubuntu/Kubuntu release and architecture as the
systems that will install the package. A native KWin effect is tied to the
exact KWin ABI against which it was built.

Install the complete top-level build requirements for the Wayland package:

```bash
sudo apt update
sudo apt install build-essential ca-certificates cargo cmake extra-cmake-modules \
  kwin-dev libkf6kcmutils-dev libpulse-dev ninja-build pkg-config
```

`kwin-dev` supplies its required Qt 6, KF6 Config/CoreAddons, Epoxy, and
Wayland development dependencies. `build-essential` supplies the C/C++ and
Debian package toolchain. The first build also needs access to crates.io unless
the locked Rust dependencies are already cached. To build the optional X11
package too, also install:

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

The script does not install anything and writes native-architecture packages
to `dist/`. GitHub's release workflow currently runs this build for Ubuntu
26.04 on `amd64`. Release revisions and additional distribution targets are
documented in [distribution-packaging.md](docs/distribution-packaging.md).

## Development checks

Install `rust-clippy` and `rustfmt` in addition to the package-build
requirements, then run:

```bash
cargo fmt --check
cargo clippy --locked --all-targets -- --deny warnings
cargo test --locked
./packaging/build-deb.sh --wayland-only
```

The IPC format is documented in [ipc-protocol.md](docs/ipc-protocol.md).

## Scope and security

- KDE Plasma 6 is required; other Wayland compositors need their own native
  integration.
- IPC stays under `$XDG_RUNTIME_DIR` with user-only permissions.
- KWin restores its native cursor if telemetry becomes stale, the screen
  locks, or the effect unloads.
- The default input is mixed system output, not the microphone.

## License

MIT
