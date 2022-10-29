#version 450

// layout (location = 0) in vec3 aPosition;
// layout (location = 1) in vec4 aColor;
// layout (location = 2) in vec2 aTexureCoords;

layout (location = 0) in vec3 aPosition;
// out vec2 textureCoords;
const vec3 pos[3] = {
    vec3(0.0, 0.5, 0.0),
    vec3(-0.5, -0.5, 0.0),
    vec3(0.5, -0.5, 0.0),
};

// uniform mat4 transform;

void main()
{
    // gl_Position = transform * vec4(aPosition.x, aPosition.y, aPosition.z, 1.0);
    // gl_Position = vec4(pos[gl_VertexIndex], 1.0);
    gl_Position = vec4(aPosition, 1.0);
    // textureCoords = aTexureCoords;
}