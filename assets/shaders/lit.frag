#version 330 core

in vec3 v_position;
in vec4 v_color;
in vec2 v_tex_coord;
in vec3 v_normal;

out vec4 frag_color;

// Textures
uniform sampler2D tex; // Albedo
uniform sampler2D tex_specular;
uniform sampler2D tex_roughness;
uniform sampler2D tex_ambient_occlusion;
uniform sampler2D tex_emission;

uniform vec4 tint;
uniform float alphaThreshold;

// Lighting definitions
#define MAX_LIGHTS 16
struct Light {
    int type; // 0=Directional, 1=Point, 2=Spot
    vec3 position;
    vec3 direction;
    vec3 color;
    vec3 attenuation;
    vec2 cone_angles;
};

uniform int light_count;
uniform Light lights[MAX_LIGHTS];
uniform vec3 eye_position;

void main() {
    vec4 albedo = texture(tex, v_tex_coord) * tint * v_color;
    if(albedo.a < alphaThreshold) discard;

    vec3 normal = normalize(v_normal);
    vec3 view_dir = normalize(eye_position - v_position);
    
    // Material parameters (with fallback to simple constants if maps aren't bound)
    // Since we didn't add "use_texture_x" uniforms, we will just use defaults for non-albedo if they are black
    // Often in these engines, missing textures bind to 1x1 white textures. We will just use simple Blinn-Phong.
    
    vec3 total_light = vec3(0.0);
    // Add simple ambient light
    vec3 ambient = albedo.rgb * 0.1;
    total_light += ambient;

    for (int i = 0; i < light_count; ++i) {
        Light light = lights[i];
        
        vec3 light_dir;
        float attenuation = 1.0;
        
        if (light.type == 0) { // Directional
            light_dir = normalize(-light.direction);
        } else { // Point or Spot
            vec3 diff = light.position - v_position;
            float dist = length(diff);
            light_dir = diff / dist;
            
            attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
            
            if (light.type == 2) { // Spot
                float angle = acos(dot(-light_dir, normalize(light.direction)));
                float inner = light.cone_angles.x;
                float outer = light.cone_angles.y;
                float spot_factor = smoothstep(outer, inner, angle);
                attenuation *= spot_factor;
            }
        }
        
        // Diffuse
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = light.color * diff * albedo.rgb;
        
        // Specular (Blinn-Phong)
        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(normal, halfway_dir), 0.0), 32.0); // hardcoded shininess 32 for now
        vec3 specular = light.color * spec; // Assuming white specular material
        
        total_light += (diffuse + specular) * attenuation;
    }
    
    frag_color = vec4(total_light, albedo.a);
}
