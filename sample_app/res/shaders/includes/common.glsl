#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require

#ifndef KRAFT_VERTEX_REF_TYPE
#error KRAFT_VERTEX_REF_TYPE must be defined before including this file
#endif

// Written to; using descriptor sets
layout (set = 0, binding = 0) uniform GlobalUniformBuffer
{
    mat4 ViewProjection; // 64
    mat4 LightViewProjection; // 64
    vec3 LightPosition; // 12
    float Time; // 4
    vec4 LightColor; // 16
    vec3 CameraPosition; // 12
    float DeltaTime; // 4
} globalState;

struct Vertex3D
{
    vec3 position;
    vec2 uv;
    vec3 normal;
    vec4 tangent;
};

layout(buffer_reference, scalar, buffer_reference_align = 4) readonly buffer Vertex3DRef
{
    Vertex3D vertices[];
};

layout (push_constant) uniform pushConstants
{
    mat4 Model;
    vec2 MousePosition;
    uint EntityId;
    uint MaterialIdx;
    KRAFT_VERTEX_REF_TYPE VertexBuffer;
} variableState;

#define vertices variableState.VertexBuffer.vertices

#if defined FRAGMENT

layout(set = 0, binding = 1) uniform sampler TextureSamplers[];
layout (set = 2, binding = 0) uniform texture2D Textures[];

#define SAMPLER_INDEX_MASK 0xF0000000u
#define TEXTURE_INDEX_MASK 0x0FFFFFFFu
#define UnpackSamplerIndex(packed_id) ((packed_id & SAMPLER_INDEX_MASK) >> 28)
#define UnpackTextureIndex(packed_id) (packed_id & TEXTURE_INDEX_MASK)
#define SampleTexture(packed_id, uv) texture(sampler2D(Textures[UnpackTextureIndex(packed_id)], TextureSamplers[UnpackSamplerIndex(packed_id)]), uv)

#endif
