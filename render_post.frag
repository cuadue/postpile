#version 330 core

uniform sampler2D diffuse_map;
//uniform sampler2DShadow shadow_map;
uniform float uv_scale;

uniform int num_lights;
uniform vec3 light_vec[16];
uniform vec3 light_color[16];

in float visibility_frag;
in vec3 normal_frag;
in vec2 uv;
in float elevation;
//in vec4 shadow_coord;
in vec2 uv_offset_frag;

out vec4 color;

const float ambient_min = 0.2;
const float ambient_max = 0.4;
const float ambient_contrast = 10;
//const float shadow_map_bias = 0.002;

/*
float shadow_intensity()
{
    vec4 biased = 0.5 * (1 + shadow_coord);
    return 2 * texture(shadow_map,
        vec3(biased.xy, biased.z - shadow_map_bias));
}
*/

float ambient()
{
    // as the angle of incidence of the shadowing light gets
    // further from the zenith, increase the ambient lighting so
    // that during the darkest hours, you can still see.
    float x = 1.1 - dot(normalize(light_vec[0]), vec3(0, 0, 1));
    float clamped = clamp(x, 0, 1);
    float spread = ambient_max - ambient_min;
    return ambient_min + spread * clamped;
}

uniform mat4 view_matrix;
void main()
{
    vec2 scaled_uv = uv / uv_scale;
    vec2 tiled_uv = fract(uv) / uv_scale + uv_offset_frag;
    // Blender convention: origin is the bottom of the image
    tiled_uv.y = 1 - tiled_uv.y;

    vec4 tex_value = textureGrad(diffuse_map, tiled_uv, dFdx(scaled_uv), dFdy(scaled_uv));

    if (tex_value.a < 1e-3) {
        discard;
    }

    float fadeout = 1 + elevation / 2;
    vec3 normal = normalize(normal_frag);

    vec3 light = vec3(0, 0, 0);
    for (int i = 0; i < min(16, num_lights); i++) {
        float n_dot_l = dot(normal, light_vec[i]);
        light += light_color[i] * n_dot_l;
    }

    float shadow = 1; //shadow_intensity();
    light = mix(clamp(shadow * light, 0, 1), vec3(1, 1, 1),  ambient());

    vec3 rgb = tex_value.rgb *
               clamp(fadeout, 0, 1) *
               light *
               clamp(visibility_frag, 0, 1);
    color = vec4(rgb, tex_value.a);
}
