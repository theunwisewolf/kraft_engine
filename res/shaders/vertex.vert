#version 450

// Inputs
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec4 inColor;

layout (set = 0, binding = 0) uniform globalUniformObject
{
    mat4 Projection;
    mat4 View;
} globalState;

// Outputs
layout (location = 0) out vec4 outColor;

void main()
{
    // gl_Position = vec4(inPosition, 1.0);
    gl_Position = globalState.Projection * globalState.View * vec4(inPosition, 1.0);
    outColor = inColor;
}