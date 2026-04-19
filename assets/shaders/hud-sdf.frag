#version 330 core

in vec2 local_uv;
out vec4 frag_color;

uniform vec4 color;
uniform int shapeMode; // 0 = plain rect, 1 = bullet silhouette

float sdBox(vec2 p, vec2 b) {
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

void main() {
    if(shapeMode == 0) {
        frag_color = color;
        return;
    }

    vec2 p = local_uv;

    // Bullet silhouette: rectangle body + rounded tip (top semicircle)
    float halfWidth = 0.28;
    float bodyTop = 0.72;
    float tipRadius = halfWidth;

    // Body rectangle SDF in centered coordinates
    vec2 rectCenter = vec2(0.5, bodyTop * 0.5);
    vec2 rectHalf = vec2(halfWidth, bodyTop * 0.5);
    float dRect = sdBox(p - rectCenter, rectHalf);

    // Tip circle SDF
    vec2 tipCenter = vec2(0.5, bodyTop);
    float dCircle = length(p - tipCenter) - tipRadius;

    // Union of body and tip, clipped to top half for the round cap
    float dTip = max(dCircle, bodyTop - p.y);
    float d = min(dRect, dTip);

    // Smooth edge in screen-space for crisp scaling
    float aa = max(fwidth(d), 1e-4);
    float alpha = 1.0 - smoothstep(0.0, aa, d);

    if(alpha <= 0.001) discard;
    frag_color = vec4(color.rgb, color.a * alpha);
}
