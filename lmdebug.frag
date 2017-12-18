#version 330 core

uniform sampler2D samp;

in vec2 uv;

out vec3 color;

void main()
{
    float x = texture(samp, uv).x;
    color = vec3(x, x, x);
}
