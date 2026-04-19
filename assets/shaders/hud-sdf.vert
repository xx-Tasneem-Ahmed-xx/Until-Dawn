#version 330 core

layout(location = 0) in vec2 position;

uniform mat4 projection;
uniform mat4 model;

out vec2 local_uv;

void main() {
    local_uv = position;
    gl_Position = projection * model * vec4(position, 0.0, 1.0);
}
