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

#if defined FRAGMENT

layout(set = 0, binding = 1) uniform sampler TextureSamplers[];
layout (set = 2, binding = 0) uniform texture2D Textures[];

#define SAMPLER_INDEX_MASK 0xF0000000u
#define TEXTURE_INDEX_MASK 0x0FFFFFFFu
#define UnpackSamplerIndex(packed_id) ((packed_id & SAMPLER_INDEX_MASK) >> 28)
#define UnpackTextureIndex(packed_id) (packed_id & TEXTURE_INDEX_MASK)
#define SampleTexture(packed_id, uv) texture(sampler2D(Textures[UnpackTextureIndex(packed_id)], TextureSamplers[UnpackSamplerIndex(packed_id)]), uv)

#endif