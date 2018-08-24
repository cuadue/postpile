#version 330 core

uniform mat4 view_projection;
layout(location=2) in vec3 vertex;
layout(location=3) in mat4 model_matrix;

void main()
{
    mat4 MVP = view_projection * model_matrix;
    gl_Position = MVP * vec4(vertex, 1);
}
