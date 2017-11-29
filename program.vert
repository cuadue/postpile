#version 330 core

uniform mat4 mvp;
uniform mat4 model;

in vec4 vertex_modelspace;
in vec3 normal_modelspace;
in vec2 vertex_uv;

out vec2 uv;
out float elevation;
out vec3 normal_dir_world;

void main()
{
    normal_dir_world = normal_modelspace;

    gl_Position = mvp * vertex_modelspace;
    elevation = vertex_modelspace.z;
    uv = vertex_uv;
}
