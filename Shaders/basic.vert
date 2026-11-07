#version 330 core

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inTex;
out vec2 chTex;

uniform vec2 uPos;       // bottom left corner, in NDC
uniform vec2 uSize;      // width and height, in NDC
uniform vec2 uTexRepeat; // texture repetitions along x and y

void main()
{
    // Scale and translate the unit quad
    gl_Position = vec4(inPos * uSize + uPos, 0.0, 1.0);
    chTex = inTex * uTexRepeat;
}
