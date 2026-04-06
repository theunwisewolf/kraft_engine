#pragma once

namespace kraft::shaderfx {

struct VertexAttribute
{
    u16               location = 0;
    u16               binding = 0;
    u16               offset = 0;
    r::ShaderDataType format = r::ShaderDataType::Invalid();
};

struct VertexInputBinding
{
    u16                      binding = 0;
    u16                      stride = 0;
    r::VertexInputRate::Enum input_rate = r::VertexInputRate::Count;
};

struct VertexLayoutDefinition
{
    String8             name;
    VertexAttribute*    attributes;
    u32                 attribute_count;
    VertexInputBinding* input_bindings;
    u32                 input_binding_count;
};

struct ResourceBinding
{
    String8               name;
    u16                   set = 0;
    u16                   binding = 0;
    u16                   size = 0;
    i16                   parent_index = -1; // If this is a uniform buffer, index into the actual buffer
    r::ResourceType::Enum type;
    r::ShaderStageFlags   stage;
};

namespace ResourceBindingType {
enum Enum
{
    Local,
    Global
};
}

struct ResourceBindingsDefinition
{
    String8          name;
    u32              binding_count;
    ResourceBinding* bindings;
};

struct ShaderCodeFragment
{
    String8 name;
    String8 code;
};

struct RenderStateDefinition
{
    String8                name;
    r::CullModeFlags::Enum cull_mode;
    r::CompareOp::Enum     z_test_op;
    bool                   z_write_enable;
    bool                   blend_enable;
    r::BlendState          blend_mode;
    r::PolygonMode::Enum   polygon_mode;
    f32                    line_width;
};

struct ConstantBufferEntry
{
    String8             name;
    r::ShaderStageFlags stage;
    r::ShaderDataType   type;
};

struct ConstantBufferDefinition
{
    String8              name;
    u32                  field_count;
    ConstantBufferEntry* fields;
};

struct UniformBufferEntry
{
    String8           name;
    r::ShaderDataType type;
};

struct UniformBufferDefinition
{
    String8             name;
    u32                 field_count;
    UniformBufferEntry* fields;
};

struct VariantDefinition
{
    struct ShaderDefinition
    {
        r::ShaderStageFlags stage;
        ShaderCodeFragment  code_fragment;
    };

    String8                           name;
    const RenderStateDefinition*      render_state;
    const VertexLayoutDefinition*     vertex_layout;
    const ResourceBindingsDefinition* resources;
    const ConstantBufferDefinition*   contant_buffers;
    u32                               shader_stage_count;
    ShaderDefinition*                 shader_stages;
    bool                              has_color_output = true;
    bool                              has_depth_output = true;
    u8                                msaa_samples = 0; // 0 = inherit from swapchain, 1/2/4/8 = override
};

struct ShaderEffect
{
    String8 name;
    String8 resource_path;

    u32                     vertex_layout_count;
    VertexLayoutDefinition* vertex_layouts;

    u32                         local_resource_count;
    ResourceBindingsDefinition* local_resources;

    u32                         global_resource_count;
    ResourceBindingsDefinition* global_resources;

    u32                       constant_buffer_count;
    ConstantBufferDefinition* constant_buffers;

    u32                      uniform_buffer_count;
    UniformBufferDefinition* uniform_buffers;

    u32                      storage_buffer_count;
    UniformBufferDefinition* storage_buffers;

    u32                      render_state_count;
    RenderStateDefinition*   render_states;

    u32                      code_fragment_count;
    ShaderCodeFragment*      code_fragments;

    u32                      variant_count;
    VariantDefinition*       variants;
};

} // namespace kraft::shaderfx
