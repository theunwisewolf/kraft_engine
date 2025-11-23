#pragma once

#include <containers/kraft_array.h>
#include <containers/kraft_buffer.h>
#include <core/kraft_string.h>
#include <renderer/kraft_renderer_types.h>

namespace kraft::shaderfx {

struct VertexAttribute
{
    u16                      location = 0;
    u16                      binding = 0;
    u16                      offset = 0;
    renderer::ShaderDataType format = renderer::ShaderDataType::Invalid();
};

struct VertexInputBinding
{
    u16                             binding = 0;
    u16                             stride = 0;
    renderer::VertexInputRate::Enum input_rate = renderer::VertexInputRate::Count;
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
    String8                      name;
    u16                          set = 0;
    u16                          binding = 0;
    u16                          size = 0;
    i16                          parent_index = -1; // If this is a uniform buffer, index into the actual buffer
    renderer::ResourceType::Enum type;
    renderer::ShaderStageFlags   stage;
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
    String8                       name;
    renderer::CullModeFlags::Enum cull_mode;
    renderer::CompareOp::Enum     z_test_op;
    bool                          z_write_enable;
    bool                          blend_enable;
    renderer::BlendState          blend_mode;
    renderer::PolygonMode::Enum   polygon_mode;
    f32                           line_width;
};

struct ConstantBufferEntry
{
    String8                    name;
    renderer::ShaderStageFlags stage;
    renderer::ShaderDataType   type;
};

struct ConstantBufferDefinition
{
    String8              name;
    u32                  field_count;
    ConstantBufferEntry* fields;
};

struct UniformBufferEntry
{
    String8                  name;
    renderer::ShaderDataType type;
};

struct UniformBufferDefinition
{
    String8             name;
    u32                 field_count;
    UniformBufferEntry* fields;
};

struct RenderPassDefinition
{
    struct ShaderDefinition
    {
        renderer::ShaderStageFlags stage;
        ShaderCodeFragment         code_fragment;
    };

    String8                           name;
    const RenderStateDefinition*      render_state;
    const VertexLayoutDefinition*     vertex_layout;
    const ResourceBindingsDefinition* resources;
    const ConstantBufferDefinition*   contant_buffers;
    u32                               shader_stage_count;
    ShaderDefinition*                 shader_stages;
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

    RenderStateDefinition render_state;
    ShaderCodeFragment    code_fragment;
    RenderPassDefinition  render_pass_def;

    // ShaderEffect() {};
    // ShaderEffect(ShaderEffect& Other)
    // {
    //     *this = Other;
    // }

    // ShaderEffect& operator=(const ShaderEffect& Other)
    // {
    //     Name = Other.Name;
    //     ResourcePath = Other.ResourcePath;

    //     vertex_layout_count = Other.vertex_layout_count;
    //     vertex_layouts = Other.vertex_layouts;

    //     local_resource_count = Other.local_resource_count;
    //     local_resources = Other.local_resources;

    //     global_resource_count = Other.global_resource_count;
    //     global_resources = Other.global_resources;

    //     constant_buffer_count = Other.constant_buffer_count;
    //     constant_buffers = Other.constant_buffers;

    //     uniform_buffer_count = Other.uniform_buffer_count;
    //     uniform_buffers = Other.uniform_buffers;

    //     storage_buffer_count = Other.storage_buffer_count;
    //     storage_buffers = Other.storage_buffers;

    //     code_fragment = Other.code_fragment;
    //     render_state = Other.render_state;
    //     Resources = Other.Resources;
    //     render_pass_def = Other.render_pass_def;

    //     // for (int i = 0; i < Other.RenderPasses.Length; i++)
    //     // {
    //         render_pass_def[i].Name = Other.RenderPasses[i].Name;
    //         RenderPasses[i].ShaderStages = Other.RenderPasses[i].ShaderStages;

    //         // Correctly assign the offsets
    //         RenderPasses[i].VertexLayout = &vertex_layouts[(Other.RenderPasses[i].VertexLayout - &Other.vertex_layouts[0])];
    //         RenderPasses[i].Resources = &LocalResources[(Other.RenderPasses[i].Resources - &Other.LocalResources[0])];
    //         RenderPasses[i].ConstantBuffers = &ConstantBuffers[(Other.RenderPasses[i].ConstantBuffers - &Other.ConstantBuffers[0])];
    //         RenderPasses[i].RenderState = &RenderStates[(Other.RenderPasses[i].RenderState - &Other.RenderStates[0])];
    //     // }

    //     return *this;
    // }
};

} // namespace kraft::shaderfx