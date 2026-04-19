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
uniform int has_albedo_map;
uniform int has_specular_map;
uniform int has_roughness_map;
uniform int has_ambient_occlusion_map;
uniform int has_emission_map;

uniform vec4 tint;
uniform float alphaThreshold;
uniform vec2 uv_scale;

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
    vec2 uv = v_tex_coord * uv_scale;
    vec4 texel = (has_albedo_map == 1) ? texture(tex, uv) : vec4(1.0);
    vec4 albedo = texel * tint * v_color;
    if(albedo.a < alphaThreshold) discard;

    vec3 normal = normalize(v_normal);
    vec3 view_dir = normalize(eye_position - v_position);

    vec3 specular_color = (has_specular_map == 1) ? texture(tex_specular, uv).rgb : vec3(1.0);
    float roughness = (has_roughness_map == 1) ? texture(tex_roughness, uv).r : 0.5;
    roughness = clamp(roughness, 0.04, 1.0);
    float ao = (has_ambient_occlusion_map == 1) ? texture(tex_ambient_occlusion, uv).r : 1.0;
    ao = clamp(ao, 0.0, 1.0);
    vec3 emission = (has_emission_map == 1) ? texture(tex_emission, uv).rgb : vec3(0.0);

    // Convert roughness to a Blinn-Phong shininess value.
    // Lower roughness => tighter highlights, higher roughness => broader highlights.
    float shininess = mix(128.0, 4.0, roughness);
    
    vec3 total_light = vec3(0.0);
    // Add simple ambient term, modulated by ambient occlusion.
    vec3 ambient = albedo.rgb * 0.20 * ao;
    total_light += ambient;

    for (int i = 0; i < light_count; ++i) {
        Light light = lights[i];
        
        vec3 light_dir;
        float attenuation = 1.0;
        
        if (light.type == 0) { // Directional
            light_dir = normalize(-light.direction);
        } else { // Point or Spot
            vec3 diff = light.position - v_position;
            float dist = max(length(diff), 0.0001);
            light_dir = diff / dist;
            
            attenuation = 1.0 / (light.attenuation.x + light.attenuation.y * dist + light.attenuation.z * dist * dist);
            attenuation = max(attenuation, 0.0);
            
            if (light.type == 2) { // Spot
                float inner_cos = cos(light.cone_angles.x);
                float outer_cos = cos(light.cone_angles.y);
                float cos_angle = dot(normalize(light.direction), -light_dir);
                float spot_factor = smoothstep(outer_cos, inner_cos, cos_angle);
                attenuation *= spot_factor;
            }
        }
        
        // Diffuse
        float diff = max(dot(normal, light_dir), 0.0);
        vec3 diffuse = light.color * diff * albedo.rgb;
        
        // Specular (Blinn-Phong)
        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(normal, halfway_dir), 0.0), shininess);
        vec3 specular = light.color * spec * specular_color;
        
        total_light += (diffuse + specular) * attenuation;
    }

    total_light += emission;
    
    frag_color = vec4(total_light, albedo.a);
}
