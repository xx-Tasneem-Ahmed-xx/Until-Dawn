#version 330

in vec2 tex_coord;
out vec4 frag_color;

uniform vec2 center;
uniform float halfLength;
uniform float halfThickness;
uniform vec4 color;

void main(){
    vec2 delta = abs(tex_coord - center);

    float vertical = step(delta.x, halfThickness) * step(delta.y, halfLength);
    float horizontal = step(delta.y, halfThickness) * step(delta.x, halfLength);
    float mask = max(vertical, horizontal);

    if(mask <= 0.0) discard;

    frag_color = vec4(color.rgb, color.a * mask);
}
