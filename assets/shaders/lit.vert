#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;

uniform mat4 transform;       // MVP matrix
uniform mat4 M;               // Model to world
uniform mat4 M_IT;            // Inverse transpose of M

out vec3 v_position;          // World space position
out vec4 v_color;
out vec2 v_tex_coord;
out vec3 v_normal;            // World space normal

void main() {
    v_color = color;
    v_tex_coord = tex_coord;
    
    // Transform vertex position to world space
    v_position = vec3(M * vec4(position, 1.0));
    
    // Transform normal to world space
    v_normal = normalize(mat3(M_IT) * normal);
    
    // Compute NDC position
    gl_Position = transform * vec4(position, 1.0);
}
