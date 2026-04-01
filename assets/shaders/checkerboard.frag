#version 330 core

out vec4 frag_color;

// In this shader, we want to draw a checkboard where the size of each tile is (size x size).
// The color of the top-left most tile should be "colors[0]" and the 2 tiles adjacent to it
// should have the color "colors[1]".

//TODO: (Req 1) Finish this shader.

// each 32 pixel will map to a tile
uniform int size = 32;
uniform vec3 colors[2];

void main(){
    vec2 coord = gl_FragCoord.xy;

    vec2 tile = floor(coord / size);

    int checker = int(mod(tile.x + tile.y, 2.0));

    vec3 color = colors[checker];

    frag_color = vec4(color, 1.0);
}   