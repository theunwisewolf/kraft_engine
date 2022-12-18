#version 450

// Inputs
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;

// Written to; using descriptor sets
layout (set = 0, binding = 0) uniform globalUniformObject
{
    mat4 Projection;
    mat4 View;
} globalState;

layout (push_constant) uniform pushConstants
{
    mat4 Model;
} variableState;

// Output from the vertex shader to the fragment shader
layout(location = 0) out struct DataTransferObject {
	vec2 UV;
} outDTO;

void main()
{
    outDTO.UV = inUV;
    gl_Position = globalState.Projection * globalState.View * variableState.Model * vec4(inPosition, 1.0);
}