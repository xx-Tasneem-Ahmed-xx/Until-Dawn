#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 4) in uvec4 joints;
layout(location = 5) in vec4 weights;

out Varyings {
    vec4 color;
    vec2 tex_coord;
} vs_out;

uniform mat4 transform;
uniform int hasSkinning;
uniform int boneCount;
uniform mat4 uBones[128];

void main(){
    vec4 localPosition = vec4(position, 1.0);

    if(hasSkinning == 1 && boneCount > 0){
        mat4 skin = mat4(0.0);

        if(int(joints.x) < boneCount) skin += uBones[int(joints.x)] * weights.x;
        if(int(joints.y) < boneCount) skin += uBones[int(joints.y)] * weights.y;
        if(int(joints.z) < boneCount) skin += uBones[int(joints.z)] * weights.z;
        if(int(joints.w) < boneCount) skin += uBones[int(joints.w)] * weights.w;

        localPosition = skin * localPosition;
    }

    gl_Position = transform * localPosition;
    vs_out.color = color;
    vs_out.tex_coord = tex_coord;
}