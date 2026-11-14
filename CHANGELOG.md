# Changelog

## [Unreleased]

- Add Fedora KDE 45 Wayland RPM packaging, including binary and source RPMs,
  an exact KWin runtime dependency, and the graphical-session audio service.
- Build, test, and install Fedora RPMs in CI; include Fedora packages,
  compatibility metadata, checksums, and attestations in future releases.
- Document Fedora installation, local builds, and rebuilding after KWin updates.
  Validation in a real Fedora KDE desktop session remains pending.
- Use fixed-size audio frames to pass Fedora's newer Rust lint checks while
  preserving stereo sample decoding.
- Allow manual package builds from a source commit for an existing application
  release, recording the commit and keeping compatibility metadata per revision.

## [0.4.0] — 2026-11-07

**A major expansion: PulsePointer brings the whole desktop into the music.**
Cursor animation, pointer visualizers, and 20 new screen effects can now run
together. Mix a gentle glow with an elastic pointer, turn the borders into a
spectrum, or combine ripples, auroras, particles, and feedback for something much
more dramatic.

### New effects and combinations

- **Six border effects:** Breathing frame, Razor frame, Liquid border, Perimeter
  oscilloscope, Corner shockwaves, and Chasing sparks.
- **Six color effects:** Chroma breathing, Bass bloom, Spectral palette, Hue tide,
  Selective color, and Color echoes.
- **Eight atmosphere and distortion effects:** Gravity well, Refractive ripples,
  Prism kick, Aurora curtains, Spectral terrain, Particle weather, Feedback ghosts,
  and Drop collapse.
- Combine Elastic or Wobble cursor animation with any of the five pointer
  visualizers. Multiple visualizers can run together, and the attack ring works
  independently of cursor animation.
- Five editable presets: **Velvet, Voltage, Underwater, Aurora, and Meltdown**.

### Controls and behavior

- A redesigned settings dialog with pages for the pointer, borders, desktop color,
  atmosphere and distortion, and timing and displays.
- Individual screen-effect strength controls, separate pointer and screen
  intensity, shared accent color, border profiles, reach, density, and softness.
- Adjustable rise and fade times, plus a choice of all displays or only the
  display containing the pointer.
- A persistent master enable switch and **Meta+Ctrl+Alt+P** to temporarily bypass
  or resume all layers. The shortcut is configurable in KWin's settings.
- Screen effects fade during silence and clear on stale audio or screen lock.
  Pointer effects remain available if the OpenGL screen renderer cannot start.

### Pointer refinements

- Refractive ripples are centered on the mouse pointer and follow it as it moves.
- The optional hotspot locator is a smooth **5-logical-pixel red circle** centered
  on the exact click position, with no outline. It follows display scaling,
  keeps its size during animation, and works with Unchanged, Elastic, and Wobble.

### Upgrading and packages

- Existing cursor and visualizer settings migrate automatically. Screen effects
  start disabled; select a preset or enable effects to try the new features.
- Ubuntu/Kubuntu **26.04 and 26.10**, `amd64`: a Wayland package and optional X11
  companion package for each release. Packages require the exact KWin library
  version recorded in the attached package metadata. SHA-256 checksums and build
  provenance accompany the downloads.
- Arch users build against their installed KWin development files using
  `./install.sh`.
- After installing the updated package, **log out of Plasma and back in** to load
  the new native plugin. Building a package alone does not update the installed
  effect.
- Expanded native tests cover settings migration, combinations and presets,
  settings-dialog persistence, hotspot shape and placement, audio timing, and
  rendering every screen effect with desktop OpenGL and OpenGL ES.

See the [effect guide](https://github.com/simonskrede/pulsepointer/blob/v0.4.0/docs/music-effects.md)
for the full catalog and presets.

[0.4.0]: https://github.com/simonskrede/pulsepointer/compare/v0.3.0...v0.4.0
