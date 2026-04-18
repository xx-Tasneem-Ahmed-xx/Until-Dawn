#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;
uniform int hasTexture;
uniform sampler2D uBaseColorTex;

void main(){
    vec4 albedo = tint * fs_in.color;

    if(hasTexture == 1){
        albedo *= texture(uBaseColorTex, fs_in.tex_coord);
    }

    frag_color = albedo;
}