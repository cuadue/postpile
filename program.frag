#version 330 core

uniform sampler2D diffuse_map;
uniform mat3 N;

uniform int num_lights;
uniform vec3 light_vec[16];
uniform vec3 light_color[16];

in vec3 normal_frag;
in vec2 uv;
in float elevation;

out vec3 color;

const float ambient = 0.15;

void main()
{
    float fadeout = 1 + elevation / 2;
    vec3 normal = normalize(N * normal_frag);

    vec3 light = vec3(0, 0, 0);
    for (int i = 0; i < min(16, num_lights); i++) {
        light += light_color[i] * dot(normal, light_vec[i]);
    }

    color = texture(diffuse_map, uv).rgb *
            clamp(fadeout, 0, 1) *
            clamp(atan(ambient + light), 0, 1);
}
