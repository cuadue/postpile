#version 330 core

uniform sampler2D sampler;

uniform int num_lights;
uniform vec3 light_direction[16];
uniform vec3 light_color[16];

in vec3 normal_dir_world;
in vec2 uv;
in float elevation;

out vec3 color;

const float ambient = 0.15;

void main()
{
    float fadeout = 1 + elevation / 2;
    vec3 light = vec3(0, 0, 0);

    int i;
    for (i = 0; i < min(num_lights, 16); i++) {
        float dp = dot(normal_dir_world, light_direction[i]);
        light += dp * light_color[i] / num_lights;
    }

    color = texture(sampler, uv).rgb *
            clamp(fadeout, 0, 1) *
            clamp(light, ambient, 1);
}
