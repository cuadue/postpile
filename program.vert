#version 330 core

in vec4 vertex_modelspace;
in vec2 vertex_uv;

out vec2 uv;

uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vertex_modelspace;
    uv = vertex_uv;
}
