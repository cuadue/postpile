#version 330 core

in vec2 uv;
uniform sampler2D sampler;

out vec3 color;

void main()
{
    color = texture(sampler, uv).rgb + vec3(0, 0.1, 0);
}
