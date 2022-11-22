#version 450

// Inputs
layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec2 inUV;

layout (set = 0, binding = 0) uniform globalUniformObject
{
    mat4 Projection;
    mat4 View;
} globalState;

layout (push_constant) uniform pushConstants
{
    mat4 Model;
} variableState;

layout(location = 0) out struct DataTransferObject {
	vec2 UV;
} outDTO;

void main()
{
    // gl_Position = vec4(inPosition, 1.0);
    outDTO.UV = inUV;
    gl_Position = globalState.Projection * globalState.View * variableState.Model * vec4(inPosition, 1.0);
}