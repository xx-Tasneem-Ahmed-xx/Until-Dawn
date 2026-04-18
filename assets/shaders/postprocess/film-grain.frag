#version 330 core

// The texture holding the scene pixels
uniform sampler2D tex;

// Elapsed time in seconds — drives the animated grain
uniform float time;

// Texture coordinate passed from the fullscreen vertex shader
in vec2 tex_coord;

out vec4 frag_color;

// =====================================================================
// Film Grain + Vignette — Phase 2 Postprocessing Effect
// Combines two horror-atmosphere effects in a single pass:
//   1. Vignette  — darkens the screen edges, focusing attention center
//   2. Film Grain — animated noise overlay, mimics old horror film stock
// =====================================================================

// Simple pseudo-random hash based on position + time
float rand(vec2 co) {
    return fract(sin(dot(co.xy + time * 0.01, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    // --- Sample scene colour ---
    vec4 color = texture(tex, tex_coord);

    // --- Vignette ---
    // Convert UV (0..1) to NDC (-1..1), compute squared distance from centre
    vec2 ndc = tex_coord * 2.0 - 1.0;
    float dist_sq = dot(ndc, ndc);

    // Smooth, aggressive vignette — stronger than the previous pass
    float vignette = 1.0 / (1.0 + dist_sq * 2.5);

    // --- Film Grain ---
    // Amount of grain (0.0 = none, 1.0 = pure noise)
    float grain_strength = 0.055;
    float noise = rand(tex_coord) * 2.0 - 1.0;   // in [-1, 1]

    // --- Combine ---
    vec3 result = color.rgb * vignette + noise * grain_strength;

    // Slight cold blue tint to reinforce the "night horror" mood
    result *= vec3(0.90, 0.93, 1.0);

    frag_color = vec4(result, color.a);
}
