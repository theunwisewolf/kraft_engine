#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexureCoords;

out vec4 vertexColor;
out vec2 textureCoords;

// uniform float offset;

void main()
{
    gl_Position = vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);
    vertexColor = aColor;
    textureCoords = aTexureCoords;
}