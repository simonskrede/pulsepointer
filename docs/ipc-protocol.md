# PulsePointer IPC protocol

The Rust daemon sends one Unix datagram per audio update to:

```text
$XDG_RUNTIME_DIR/pulsepointer/kwin-effect.sock
```

The KWin effect owns the socket. The containing directory is mode `0700` and
the socket is mode `0600`. Both sides are intentionally single-user and
local-only. The daemon sends analysis data only; visualization selection,
color, sizing, and animation behavior belong to the KWin effect.

All integers and IEEE-754 floating-point values are little-endian. Version 2
has a fixed size of 676 bytes:

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | ASCII magic `PPAU` |
| 4 | 2 | Protocol version (`2`) |
| 6 | 2 | Reserved flags (`0`) |
| 8 | 8 | Monotonically wrapping update sequence (`u64`) |
| 16 | 4 | Overall level (`f32`, `0.0..=1.0`) |
| 20 | 4 | Bass energy (`f32`, `0.0..=1.0`) |
| 24 | 4 | Midrange energy (`f32`, `0.0..=1.0`) |
| 28 | 4 | Treble energy (`f32`, `0.0..=1.0`) |
| 32 | 4 | Spectral-onset impulse (`f32`, `0.0..=1.0`) |
| 36 | 128 | 32 low-to-high logarithmic spectrum buckets (`f32[]`, each `0.0..=1.0`) |
| 164 | 512 | 128 time-ordered waveform samples (`f32[]`, each `-1.0..=1.0`) |

The sender replaces non-finite values with zero and clamps every value to its
documented range. The receiver rejects malformed packets, drains all queued
datagrams on each notification, and applies only the newest complete valid
packet. This bounds latency when rendering falls behind audio capture.

If no valid packet arrives for one second, the screen locks, or the effect is
unloaded, the effect hides its visualization and restores KWin's native cursor.
