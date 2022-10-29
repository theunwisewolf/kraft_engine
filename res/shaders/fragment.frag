#version 450
// #extension GL_ARB_separate_shader_objects : enable

// Inputs
layout (location = 0) in vec4 inColor;

// Outputs
layout (location = 0) out vec4 outColor;

void main() 
{
    outColor = inColor;
}