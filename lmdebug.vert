#version 330 core

in vec4 vertex;
in vec2 vertex_uv;

out vec2 uv;

void main()
{
    uv = vertex_uv;
    gl_Position = vertex;
}
