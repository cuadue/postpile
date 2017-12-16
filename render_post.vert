#version 330 core

uniform mat4 MVP;
uniform mat3 N;

in vec4 vertex;
in vec3 normal;
in vec2 vertex_uv;

out vec2 uv;
out float elevation;
out vec3 normal_frag;

void main()
{
    gl_Position = MVP * vertex;
    elevation = vertex.z;
    uv = vertex_uv;
    normal_frag = normal;
}