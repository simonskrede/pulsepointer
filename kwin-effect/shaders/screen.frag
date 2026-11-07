// Strength order matches ScreenEffect in effectcatalog.h.
in vec2 uv;
out vec4 fragColor;
uniform sampler2D scene;
uniform sampler2D history;
uniform sampler2D audioData;
uniform vec2 size;
uniform vec2 pointer;
uniform vec4 bands; // level, bass, mid, treble
uniform vec4 waveAge;
uniform vec4 waveStrength;
uniform vec4 fx0; // breathing, razor, liquid, perimeter
uniform vec4 fx1; // corners, sparks, chroma, bloom
uniform vec4 fx2; // palette, hue, selective, color echoes
uniform vec4 fx3; // gravity, refraction, prism, aurora
uniform vec4 fx4; // terrain, particles, feedback, drop
uniform vec4 tint;
uniform float time;
uniform float hue;
uniform float pulse;
uniform float activity;
uniform float reach;
uniform float density;
uniform float softness;
uniform int profile;
uniform float collapse;
uniform float dropAge;
uniform float dropStrength;
uniform float historyValid;
uniform float historyDecay;
uniform float historyStep;
const float PI = 3.14159265359;

float spectrum(float x)
{
    return texture(audioData, vec2((0.5 + clamp(x, 0.0, 1.0) * 31.0) / 160.0, 0.5)).r;
}
float waveform(float x)
{
    return texture(audioData, vec2((32.5 + fract(x) * 127.0) / 160.0, 0.5)).r;
}
float hash(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float line(float distance, float width) { return 1.0 - smoothstep(width, width + 1.5, abs(distance)); }
float feather(float distance, float depth)
{
    return 1.0 - smoothstep(max(0.0, depth - max(1.0, depth * softness)), max(1.0, depth), distance);
}
float borderShape(float phase)
{
    float triangle = 1.0 - abs(fract(phase) * 2.0 - 1.0);
    if (profile == 1) return 0.5 - 0.5 * cos(phase * 2.0 * PI);
    if (profile == 2) return sqrt(max(0.0, 1.0 - pow(fract(phase) * 2.0 - 1.0, 2.0)));
    if (profile == 4) return pow(triangle, 5.0);
    return triangle;
}
vec3 palette(float x)
{
    return 0.5 + 0.5 * cos(2.0 * PI * (x + vec3(0.0, 0.33, 0.67)));
}
vec3 rotateHue(vec3 color, float angle)
{
    vec3 axis = normalize(vec3(1.0));
    return color * cos(angle) + cross(axis, color) * sin(angle)
        + axis * dot(axis, color) * (1.0 - cos(angle));
}
vec3 sampleScene(vec2 pos) { return texture(scene, clamp(pos, vec2(0.0), vec2(1.0))).rgb; }
void main()
{
    // Captures are normalized by blitFromRenderTarget; y=0 is the bottom of the screen.
    vec2 p = vec2(uv.x, 1.0 - uv.y);
    vec2 px = p * size;
    float minSize = min(size.x, size.y);
    vec2 nearEdge = min(px, size - px);
    float edge = min(nearEdge.x, nearEdge.y);
    float perimeterLength = 2.0 * (size.x + size.y);
    float along;
    if (nearEdge.y <= nearEdge.x) {
        along = p.y < 0.5 ? px.x : size.x + size.y + size.x - px.x;
    } else {
        along = p.x > 0.5 ? size.x + px.y : perimeterLength - px.y;
    }
    float around = along / perimeterLength;
    float phase = along / minSize * density;
    vec2 source = uv;
    vec2 delta = (p - pointer) * size / minSize;
    float radius = length(delta);
    if (fx3.x > 0.0) {
        float rebound = 0.0;
        for (int i = 0; i < 4; ++i) {
            rebound += waveStrength[i] * sin(waveAge[i] * 18.0) * exp(-waveAge[i] * 5.0);
        }
        source += vec2(delta.x, -delta.y) * exp(-radius * radius * 8.0)
            * fx3.x * (bands.y * 0.10 + rebound * 0.025);
    }
    if (fx3.y > 0.0) {
        float ripple = 0.0;
        for (int i = 0; i < 4; ++i) {
            float age = waveAge[i];
            ripple += sin((radius - age * 0.7) * 60.0) * exp(-abs(radius - age * 0.7) * 18.0)
                * exp(-age * 2.0) * waveStrength[i];
        }
        source += vec2(delta.x, -delta.y) / max(radius, 0.02) * ripple * 0.012 * fx3.y;
    }
    vec3 color = sampleScene(source);
    if (fx3.z > 0.0) {
        vec2 split = vec2(fx3.z * pulse * 8.0 / size.x, 0.0);
        color.r = sampleScene(source + split).r;
        color.b = sampleScene(source - split).b;
    }
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (fx1.z > 0.0) {
        float breath = sin(time * (1.2 + bands.y * 2.0)) * 0.35;
        float saturation = 1.0 + fx1.z * activity * (breath + bands.x + pulse * 0.5 - 0.35);
        color = mix(vec3(luminance), color, saturation);
    }
    if (fx2.y > 0.0) {
        color = mix(color, rotateHue(color, hue * 2.0 * PI), min(1.0, fx2.y * activity));
    }
    if (fx2.z > 0.0) {
        vec3 direction = palette(hue);
        float similarity = dot(normalize(max(color - vec3(luminance * 0.7), vec3(0.001))),
                               normalize(direction + vec3(0.001)));
        float keep = smoothstep(0.85, 0.98, similarity);
        color = mix(color, mix(vec3(luminance), color, keep), min(1.0, fx2.z * activity));
    }
    if (historyValid > 0.5 && (fx2.w > 0.0 || fx4.z > 0.0)) {
        vec2 echoUV = (uv - 0.5) / exp((0.005 + pulse * 0.018) * historyStep) + 0.5;
        vec3 old = texture(history, echoUV).rgb;
        if (fx4.z > 0.0) {
            float persistence = pow(max(0.0001, min(0.78, fx4.z * pulse)), historyStep) * historyDecay;
            color = mix(color, old, persistence);
        }
        if (fx2.w > 0.0) {
            vec3 offset = sampleScene(source + vec2(2.0 / size.x, 0.0));
            vec3 outlines = abs(old - offset);
            color += outlines * palette(hue + 0.2) * fx2.w * pulse * 0.7;
        }
    }
    vec3 light = vec3(0.0);
    vec3 base = tint.rgb;
    if (fx0.x > 0.0) {
        float depth = reach * (0.12 + 0.88 * bands.y)
            * (0.8 + 0.2 * sin(time * (1.5 + bands.y)));
        light += base * fx0.x * exp(-edge / max(1.0, depth * 0.4)) * activity * (0.3 + bands.x);
    }
    if (fx0.y > 0.0) {
        float triangle = borderShape(phase);
        float tooth = reach * triangle * (spectrum(fract(around * 4.0)) * 0.8 + pulse * 0.2);
        light += base * fx0.y * feather(edge, tooth) * activity;
    }
    if (fx0.z > 0.0) {
        float liquid = 0.45 + 0.22 * sin(phase * 0.7 + time * (1.0 + bands.z))
            + 0.08 * bands.w * sin(phase * 4.0 - time * 3.0);
        if (profile != 0) liquid = 0.15 + 0.55 * borderShape(phase * 0.15 + time * 0.15);
        float depth = reach * liquid * (0.1 + bands.y);
        light += mix(base, palette(around + time * 0.02), 0.4) * fx0.z
            * feather(edge, depth) * activity;
    }
    if (fx0.w > 0.0) {
        float disturbance = 0.0;
        for (int i = 0; i < 4; ++i) {
            float d = abs(fract(around - waveAge[i] * 0.3 + 0.5) - 0.5);
            disturbance += exp(-d * 50.0) * exp(-waveAge[i] * 2.0) * waveStrength[i];
        }
        float offset = reach * (0.3 + waveform(around) * 0.2 + disturbance * 0.4);
        light += base * fx0.w * line(edge - offset, 1.3) * activity;
    }
    if (fx1.x > 0.0) {
        for (int corner = 0; corner < 4; ++corner) {
            vec2 center = vec2(float(corner % 2), float(corner / 2));
            float distance = length((p - center) * size);
            for (int i = 0; i < 4; ++i) {
                float age = waveAge[i];
                float wave = line(distance - age * minSize * 0.9, 2.0 + softness * 6.0);
                light += base * fx1.x * wave * waveStrength[i] * exp(-age * 2.0);
            }
        }
    }
    if (fx1.y > 0.0) {
        float sparks = 0.0;
        for (int i = 0; i < 6; ++i) {
            float tail = fract(around - time * (0.08 + bands.x * 0.1) - float(i) / 6.0);
            sparks += exp(-tail * 160.0) * (0.3 + bands.w);
        }
        light += palette(around + hue) * fx1.y * sparks * exp(-edge / 5.0) * activity;
    }
    if (fx1.w > 0.0) {
        light += vec3(1.0, 0.22, 0.04) * fx1.w * bands.y
            * exp(-edge / max(1.0, reach * 0.7));
    }
    if (fx2.x > 0.0) {
        vec3 spectral = bands.y * vec3(1.0, 0.16, 0.03)
            + bands.z * vec3(0.05, 0.8, 0.2) + bands.w * vec3(0.1, 0.25, 1.0);
        light += spectral * fx2.x * (0.08 + 0.4 * exp(-edge / reach));
    }
    if (fx3.w > 0.0) {
        float curtain = sin(p.x * density + time * (0.4 + bands.z));
        float fine = sin(p.x * density * 5.0 - time * 2.0) * bands.w * 0.12;
        float depth = reach * (0.9 + curtain * 0.35 + fine) * (0.5 + bands.z);
        float folds = pow(0.5 + 0.5 * sin(p.x * density * 2.5 + curtain), 3.0);
        light += palette(p.x * 0.5 + hue) * fx3.w * folds
            * exp(-px.y / max(1.0, depth)) * activity;
    }
    if (fx4.x > 0.0) {
        float terrain = reach * spectrum(p.x) * (0.65 + 0.35 * sin(p.x * 80.0));
        float mountain = feather(size.y - px.y, terrain);
        light += mix(base, palette(p.x * 0.5), 0.35) * fx4.x * mountain * activity;
    }
    if (fx4.y > 0.0) {
        for (int i = 0; i < 24; ++i) {
            float seed = float(i) + 1.0;
            float life = fract(time * (0.08 + hash(seed) * 0.1) + hash(seed + 30.0));
            vec2 center = vec2(fract(hash(seed + 7.0) + sin(time + seed) * bands.z * 0.08),
                               1.0 - life * (0.35 + bands.y * 0.65));
            float distance = length((p - center) * size);
            float glitter = 0.5 + 0.5 * sin(time * 12.0 + seed * 3.0) * bands.w;
            light += palette(hash(seed)) * fx4.y * exp(-distance * 0.35)
                * sin(life * PI) * glitter * activity;
        }
    }
    if (fx4.w > 0.0) {
        vec2 cornerDistance = min(px, size - px);
        float cornerGlow = exp(-length(cornerDistance) / max(1.0, reach * (1.1 - collapse * 0.8)));
        light += base * fx4.w * cornerGlow * collapse * 0.65;
        float launch = dropAge * minSize * 1.2;
        float wave = line(edge - launch, 3.0 + softness * 14.0);
        light += base * fx4.w * wave * dropStrength * exp(-dropAge * 2.5);
    }
    // Bound combined overlays, while preserving scene values above 1 for HDR desktops.
    float opacity = clamp(max(light.r, max(light.g, light.b)) * tint.a, 0.0, 0.8);
    vec3 overlay = light / max(0.0001, max(light.r, max(light.g, light.b)));
    color = mix(color, overlay, opacity);
    fragColor = vec4(max(color, vec3(0.0)), 1.0);
}
