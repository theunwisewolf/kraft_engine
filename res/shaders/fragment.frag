#version 450
// #extension GL_ARB_separate_shader_objects : enable

// Inputs from the vertex shader
layout (location = 0) in struct DataTransferObject 
{
	vec2 UV;
} inDTO;

// Outputs
layout (location = 0) out vec4 outColor;

// Written to; using descriptor sets
layout(set = 1, binding = 0) uniform LocalUniformObject 
{
    vec4 DiffuseColor;
} localUniformObject;

layout(set = 1, binding = 1) uniform sampler2D diffuseSampler;

void main() 
{
    outColor = localUniformObject.DiffuseColor * texture(diffuseSampler, inDTO.UV);
}