#version 330 core

uniform sampler2D diffuse_map;
uniform sampler2D shadow_map;

uniform mat3 N;

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

bool in_shadow()
{
    vec4 biased = 0.5 * (1 + shadow_coord);
    return texture(shadow_map, biased.xy).x < biased.z - 0.001;
}

void main()
{
    float fadeout = 1 + elevation / 2;
    vec3 normal = normalize(N * normal_frag);

    vec3 light = vec3(0, 0, 0);
    for (int i = 0; i < min(16, num_lights); i++) {
        light += light_color[i] * dot(normal, light_vec[i]);
    }

    vec3 shadow_intensity = vec3(1, 1, 1);
    if (in_shadow()) shadow_intensity *= 0.5;

    color = texture(diffuse_map, uv).rgb *
            clamp(fadeout, 0, 1) *
            clamp(atan(ambient + light), 0, 1) *
            clamp(visibility, 0, 1) *
            shadow_intensity;
}
