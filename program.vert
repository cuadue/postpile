#version 330 core

in vec4 vertex_modelspace;

uniform mat4 mvp;

void main()
{
    gl_Position = mvp * vertex_modelspace;
}
