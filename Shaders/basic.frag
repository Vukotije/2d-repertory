#version 330 core

in vec2 chTex;
out vec4 outCol;

uniform sampler2D uTex0;
uniform bool useTexture;
uniform vec4 uColor;

void main()
{
    // Flat colour, or the texture tinted by the colour (white leaves it unchanged)
    if (!useTexture) outCol = uColor;
    else outCol = texture(uTex0, chTex) * uColor;
}
