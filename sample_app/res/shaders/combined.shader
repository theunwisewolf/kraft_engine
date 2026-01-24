#shader_vertex
#version 330 core

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec4 aColor;

out vec4 vertexColor;

void main()
{
    gl_Position = vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);
    vertexColor = aColor;
}

#shader_fragment
#version 330 core

in vec4 vertexColor;

out vec4 FragColor;

void main()
{
    FragColor = vertexColor;
} 