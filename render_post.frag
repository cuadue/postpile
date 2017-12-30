#version 330 core

uniform sampler2D diffuse_map;
uniform sampler2DShadow shadow_map;

uniform float visibility;
uniform int num_lights;
uniform vec3 light_vec[16];
uniform vec3 light_color[16];

in vec3 normal_frag;
in vec2 uv;
in float elevation;
in vec4 shadow_coord;

out vec3 color;

const float ambient = 0.15;

float shadow_intensity()
{
    vec4 biased = 0.5 * (1 + shadow_coord);
    return 0.2 + 0.8 * texture(shadow_map, vec3(biased.xy, biased.z - 0.001));
}

const float k_sat = 0.3;

void main()
{
    float fadeout = 1 + elevation / 2;
    vec3 normal = normalize(normal_frag);

    vec3 light = vec3(0, 0, 0);
    for (int i = 0; i < min(16, num_lights); i++) {
        light += light_color[i] * dot(normal, light_vec[i]);
    }

    float shadow = shadow_intensity();
    light *= shadow;

    color = texture(diffuse_map, uv).rgb *
            clamp(fadeout, 0, 1) *
            clamp(atan(k_sat * light)/k_sat, ambient, 1) *
            clamp(visibility, 0, 1);
}
