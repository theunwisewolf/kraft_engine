#extension GL_EXT_scalar_block_layout : require

layout (push_constant) uniform pushConstants
{
    mat4 Model;
    vec2 MousePosition;
    uint EntityId;
    uint MaterialIdx;
} variableState;

// Written to; using descriptor sets
layout (set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4 Projection;
    mat4 View;
    vec3 LightPosition;
    uint Pad0;
    vec4 LightColor;
    vec3  CameraPosition;
    float Time;
    float DeltaTime;
    uint  Pad1;
    uint  Pad2;
} globalState;

struct Vertex3D
{
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

layout(scalar, set = 0, binding = 3) readonly buffer VertexData
{
    Vertex3D vertices[];
};

#if defined FRAGMENT

layout(set = 0, binding = 1) uniform sampler TextureSampler;
layout (set = 2, binding = 0) uniform texture2D Textures[];

#endif