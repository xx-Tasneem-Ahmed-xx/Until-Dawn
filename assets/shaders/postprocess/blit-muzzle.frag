#version 330

// Base scene texture (blit pass)
uniform sampler2D tex;
// Flash intensity controlled by gameplay code in range [0, 1]
uniform float flash;
// Screen-space location of the muzzle in UV coordinates
uniform vec2 flashCenter;

in vec2 tex_coord;
out vec4 frag_color;

void main(){
    // BLIT: start from the unmodified rendered scene
    vec4 scene = texture(tex, tex_coord);

    // MUZZLE FLASH: additive burst around flashCenter
    vec2 delta = tex_coord - flashCenter;
    float radius = dot(delta, delta);
    float core = exp(-radius * 420.0);
    float glow = exp(-radius * 120.0);
    float burst = core + 0.65 * glow;

    vec3 flashColor = vec3(1.0, 0.78, 0.45);
    float strength = clamp(flash, 0.0, 1.0) * burst;

    vec3 color = scene.rgb + flashColor * strength;
    frag_color = vec4(clamp(color, 0.0, 1.0), scene.a);
}
