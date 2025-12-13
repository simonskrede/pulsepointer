# PulsePointer

A fun Linux utility written in Rust that turns your mouse cursor into a dynamic audio visualizer. It captures system audio (via PulseAudio/PipeWire) and overlays real-time visualizations (like spectrum bars, oscilloscopes, or volume indicators) directly onto your current cursor shape.

## Features

-   **Dynamic Overlay:** Draws visualizations on top of your existing cursor (pointer, text I-beam, resize arrows, etc.).
-   **Multiple Modes:** Choose between Spectrum, Oscilloscope, Bar, and Circle visualizations.
-   **Context Aware:** Detects cursor shape changes (hovering links, text) and adapts the animation to the new shape.
-   **PulseAudio/PipeWire Support:** Captures audio from your default monitoring source.
-   **Low Latency:** Uses `libpulse-simple` for efficient audio capture and XFixes for cursor updates.

## Requirements

-   Linux with X11 (Wayland is not supported as it restricts cursor modification).
-   PulseAudio or PipeWire (with `pipewire-pulse`).
-   Standard build tools (`gcc`, `pkg-config`, etc.).
-   Rust toolchain.

### System Dependencies (Ubuntu/Debian)

```bash
sudo apt install libx11-dev libxfixes-dev libxcursor-dev libpulse-dev
```

## Installation & Usage

1.  Clone the repository:
    ```bash
    git clone https://github.com/simonskrede/pulsepointer.git
    cd pulsepointer
    ```

2.  **Run with default settings (Spectrum mode):**
    ```bash
    cargo run --release
    ```

3.  **Command Line Options:**

    **Visualization Mode:**
    Use the `--mode` (or `-m`) flag to select a specific visualizer.

    ```bash
    cargo run --release -- --mode <MODE>
    ```

    **Available Modes:**
    -   `spectrum` (Default: Frequency bars)
    -   `oscilloscope` (Waveform line)
    -   `circle` (Radial volume visualizer)
    -   `bar` (Simple volume level bar)

    **Color Customization:**
    Use the `--color` (or `-c`) flag to set the visualization color in hex format (RRGGBB or AARRGGBB). The default is an intense bright orange (`CCFF4500`).

    ```bash
    cargo run --release -- --color FF0000        # Solid Red
    cargo run --release -- --color 8000FF00      # Semi-transparent Green
    ```

    **Example:**
    ```bash
    cargo run --release -- --mode oscilloscope --color 00FFFF
    ```

## Configuration

Currently, configuration is done via environment variables or by modifying `src/config.rs`:

-   `CURSOR_EQ_SOURCE`: Set this environment variable to specify a specific PulseAudio source (default is the system monitor).
-   `CURSOR_EQ_LOG`: Set to `1` to enable debug logging of audio levels to the console.

## Architecture

The project is structured into modular components:

-   `src/main.rs`: Application entry point and main event loop.
-   `src/audio.rs`: Audio capture thread using `libpulse-simple`.
-   `src/x11_interface.rs`: Wrappers for X11/XFixes/XCursor interaction.
-   `src/image.rs`: Pixel manipulation and blending logic.
-   `src/visualization/`: Visualization implementations.
-   `src/config.rs`: Compile-time constants.

## License

MIT
