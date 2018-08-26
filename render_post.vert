#version 330 core

uniform mat4 view_matrix;
uniform mat4 projection_matrix;
//uniform mat4 shadow_view_projection_matrix;

in vec4 vertex;
in vec3 normal;
in vec2 vertex_uv;
in float visibility;
in vec3 position;
in vec2 uv_offset;

out vec2 uv;
out float elevation;
out vec3 normal_frag;
//out vec4 shadow_coord;
out float visibility_frag;
out vec2 uv_offset_frag;

void main()
{
    mat4 model_matrix = mat4(1);
    model_matrix[3].xyz = position;

    mat4 MVP = projection_matrix * view_matrix * model_matrix;
    mat3 normal_matrix = mat3(transpose(inverse(view_matrix * model_matrix)));
    //mat4 shadow_MVP = shadow_view_projection_matrix * model_matrix;

    gl_Position = MVP * vertex;
    elevation = vertex.z;
    uv = vertex_uv;
    uv_offset_frag = uv_offset;
    normal_frag = normal_matrix * normal;
    //shadow_coord = shadow_MVP * vertex;
    visibility_frag = visibility;
}
