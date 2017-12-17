#version 330 core

uniform mat4 MVP;
in vec3 vertex;

void main()
{
    gl_Position = mvp * vec4(vertex, 1);
}
