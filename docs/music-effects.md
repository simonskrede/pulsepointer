# Combining music effects

Cursor animation, pointer visualizers, and screen effects are independent. Select
**Unchanged** for the cursor to keep the native pointer while using screen effects,
or enable Elastic/Wobble alongside any combination of the other layers. Multiple
pointer visualizers can also run together. The ring responds to musical attacks
regardless of the cursor animation choice.

Screen effects start disabled. Their checkboxes turn them on; the adjacent
percentage sets each effect's strength. **Screen effect intensity** scales all
screen layers together. **Pointer intensity** controls pointer animation and
visualizers. **Shared accent color** tints the pointer and decorative screen
layers; hue and saturation effects operate on the desktop's existing colors.

## Effects

| Effect | Behavior and musical input |
|---|---|
| Breathing frame | Soft edge glow; bass controls reach, overall energy controls brightness. |
| Razor frame | Spectrum-shaped teeth; musical attacks add a sharp kick. |
| Liquid border | Moving bass waves with fine treble ripples. |
| Perimeter oscilloscope | Live waveform around the frame, with traveling onset disturbances. |
| Corner shockwaves | Expanding arcs from all four corners on attacks. |
| Chasing sparks | Traveling perimeter lights, with treble controlling sparkle. |
| Chroma breathing | Muted-to-vivid desktop saturation following energy and attacks. |
| Bass bloom | Warm edge lighting following the bass envelope. |
| Spectral palette | Bass, mids, and treble mix warm, green, and blue lighting. |
| Hue tide | Slow desktop hue rotation; strong attacks reverse its current. |
| Selective color | A moving family of hues stays vivid while others lose saturation. |
| Color echoes | Colored outlines from expanding previous-frame imagery on attacks. |
| Gravity well | Bass bends the desktop toward the pointer; attacks add an elastic rebound. |
| Refractive ripples | Onsets launch expanding displacement waves centered on the mouse pointer, following it as it moves. |
| Prism kick | Brief horizontal red/blue channel separation on attacks. |
| Aurora curtains | Top-edge ribbons shaped by mids, with fine treble movement. |
| Spectral terrain | Spectrum mountains along the bottom edge. |
| Particle weather | Rising particles with bass-controlled height, mid-driven drift, and treble glitter. |
| Feedback ghosts | Faint expanding desktop copies, weighted by attacks and fade time. |
| Drop collapse | After music has played, quiet passages gather light into corners; returning energy launches a perimeter wave. |

**Border profile** changes Razor frame and Liquid border between their natural
shapes, sine waves, rounded scallops, triangles, or narrow spikes. **Edge reach** is
in logical pixels. **Wave / tooth count** controls detail; **Edge softness** ranges
from crisp edges to feathered transitions. Reach also sets the scale of edge
lighting, aurora ribbons, and terrain.

**Rise time** controls how quickly screen envelopes respond to louder music.
**Fade time** controls their release and afterglow. These controls do not change
cursor responsiveness. Onsets are detected musical attacks, not a fixed-tempo
clock. Drop collapse recognizes a quiet-to-loud transition, rather than attempting
to identify song structure.

## Presets

| Preset | Combination |
|---|---|
| Velvet | Elastic cursor, gentle breathing frame, saturation breathing, longer fade. |
| Voltage | Wobble cursor, crisp dense teeth, chasing sparks, prism kicks. |
| Underwater | Elastic cursor, liquid borders, subtle hue rotation, refraction. |
| Aurora | Elastic cursor, aurora curtains, sparse particles, spectral lighting, long fade. |
| Meltdown | Wobble cursor, deep razor edges, gravity, feedback, drop collapse. |

**Use preset** replaces the layer selection and screen timing/shape values in the
settings dialog. It preserves your accent color, display selection, master enable
switch, and pointer tuning. You can edit the result before pressing Apply.

Use **Meta+Ctrl+Alt+P** to temporarily bypass all layers, or customize **Toggle
PulsePointer** in KDE's KWin shortcuts. The master enable switch persists across
sessions. **Turn off all screen effects** clears screen layers while keeping your
pointer choices.

## Rendering and lifecycle

The daemon's version 2 telemetry already contains all inputs these effects need.
The native KWin effect smooths four audio bands using elapsed time, retains four
recent onset waves, and advances visual motion once per animation tick, independent
of output count. Silence fades back to an unmodified desktop. The optional quiet
collapse finishes after eight seconds instead of leaving a permanent vignette.

Screen effects share one shader pass per selected output. The renderer captures a
fresh complete scene into a floating-point texture, applies desktop color changes
and displacement, then combines decorative layers with bounded opacity. It keeps
an additional previous-frame texture only for color echoes or feedback. This
avoids treating a translucent tint as a saturation or refraction effect. Buffers
follow output size, scale, and transform. Echo history is invalidated on geometry,
color-description, settings, lock, stale-audio, and pointer-display changes. Removed
outputs are pruned on the next render.

Full-screen effects require OpenGL framebuffer support and temporarily block direct
scanout while animating. If shader or buffer initialization fails, screen effects
stop and pointer rendering remains available. Effects restore the native cursor
and hide every layer on stale telemetry, screen lock, bypass, disable, or unload.
No packages or live compositor configuration are changed by the build/tests.

Native tests render every effect, compare disabled/silent output with the original
scene, check border profiles, exercise settings migration and independent layer
persistence, and check envelope timing and drop transitions. Test previews use a
synthetic desktop; a live Plasma session is still needed to assess appearance and
performance on particular monitors, GPUs, and combinations of other KWin effects.
