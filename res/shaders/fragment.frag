#version 330 core

in vec4 vertexColor;
in vec2 textureCoords;

out vec4 FragColor;

uniform sampler2D texture2Data;
uniform sampler2D textureData;

void main()
{
    // FragColor = vertexColor;
    FragColor = mix(texture(textureData, textureCoords), texture(texture2Data, textureCoords), 0.1);
} 