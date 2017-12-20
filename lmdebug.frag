#version 330 core

uniform sampler2D samp;

in vec2 uv;

out vec4 color;

void main()
{
    float x = texture(samp, uv).x;
    if (x > 0.99)
        color = vec4(1, 1, 1, 0.3);
    else
        color = vec4(x, x, x, 1);
}
