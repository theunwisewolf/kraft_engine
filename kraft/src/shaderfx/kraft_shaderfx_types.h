#pragma once

#include <containers/kraft_array.h>
#include <containers/kraft_buffer.h>
#include <core/kraft_string.h>
#include <renderer/kraft_renderer_types.h>

namespace kraft::shaderfx {

struct VertexAttribute
{
    uint16                         Location = 0;
    uint16                         Binding = 0;
    uint16                         Offset = 0;
    renderer::ShaderDataType::Enum Format = renderer::ShaderDataType::Count;
};

struct VertexInputBinding
{
    uint16                          Binding = 0;
    uint16                          Stride = 0;
    renderer::VertexInputRate::Enum InputRate = renderer::VertexInputRate::Count;
};

struct VertexLayoutDefinition
{
    String                    Name;
    Array<VertexAttribute>    Attributes;
    Array<VertexInputBinding> InputBindings;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Attributes);
        Out->Write(InputBindings);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Attributes);
        In->Read(&InputBindings);
    }
};

struct ResourceBinding
{
    String                       Name;
    uint16                       Binding = 0;
    uint16                       Size = 0;
    int16                        ParentIndex = -1; // If this is a uniform buffer, index into the actual buffer
    renderer::ResourceType::Enum Type;
    renderer::ShaderStageFlags   Stage;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Binding);
        Out->Write(Size);
        Out->Write(ParentIndex);
        Out->Write(Type);
        Out->Write(Stage);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Binding);
        In->Read(&Size);
        In->Read(&ParentIndex);
        In->Read(&Type);
        In->Read(&Stage);
    }
};

struct ResourceBindingsDefinition
{
    String                 Name;
    Array<ResourceBinding> ResourceBindings;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(ResourceBindings);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&ResourceBindings);
    }
};

struct ShaderCodeFragment
{
    String Name;
    String Code;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Code);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Code);
    }
};

struct RenderStateDefinition
{
    String                        Name;
    renderer::CullModeFlags::Enum CullMode;
    renderer::CompareOp::Enum     ZTestOperation;
    bool                          ZWriteEnable;
    bool                          BlendEnable;
    renderer::BlendState          BlendMode;
    renderer::PolygonMode::Enum   PolygonMode;
    float32                       LineWidth;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(CullMode);
        Out->Write(ZTestOperation);
        Out->Write(ZWriteEnable);
        Out->Write(BlendEnable);
        Out->Write(BlendMode);
        Out->Write(PolygonMode);
        Out->Write(LineWidth);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&CullMode);
        In->Read(&ZTestOperation);
        In->Read(&ZWriteEnable);
        In->Read(&BlendEnable);
        In->Read(&BlendMode);
        In->Read(&PolygonMode);
        In->Read(&LineWidth);
    }
};

struct ConstantBufferEntry
{
    String                         Name;
    renderer::ShaderStageFlags     Stage;
    renderer::ShaderDataType::Enum Type;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Stage);
        Out->Write(Type);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Stage);
        In->Read(&Type);
    }
};

struct ConstantBufferDefinition
{
    String                     Name;
    Array<ConstantBufferEntry> Fields;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Fields);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Fields);
    }
};

struct UniformBufferEntry
{
    renderer::ShaderDataType::Enum Type;
    String                         Name;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Type);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Type);
    }
};

struct UniformBufferDefinition
{
    String                    Name;
    Array<UniformBufferEntry> Fields;

    void WriteTo(kraft::Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Fields);
    }

    void ReadFrom(kraft::Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Fields);
    }
};

struct RenderPassDefinition
{
    struct ShaderDefinition
    {
        renderer::ShaderStageFlags Stage;
        ShaderCodeFragment         CodeFragment;

        void WriteTo(kraft::Buffer* Out)
        {
            Out->Write(Stage);
            Out->Write(CodeFragment);
        }

        void ReadFrom(kraft::Buffer* In)
        {
            In->Read(&Stage);
            In->Read(&CodeFragment);
        }
    };

    String                            Name;
    const RenderStateDefinition*      RenderState;
    const VertexLayoutDefinition*     VertexLayout;
    const ResourceBindingsDefinition* Resources;
    const ConstantBufferDefinition*   ConstantBuffers;
    Array<ShaderDefinition>           ShaderStages;
};

struct ShaderEffect
{
    String                            Name;
    String                            ResourcePath;
    Array<VertexLayoutDefinition>     VertexLayouts;
    Array<ResourceBindingsDefinition> Resources;
    Array<ConstantBufferDefinition>   ConstantBuffers;
    Array<UniformBufferDefinition>    UniformBuffers;
    Array<UniformBufferDefinition>    StorageBuffers;
    Array<RenderStateDefinition>      RenderStates;
    Array<ShaderCodeFragment>         CodeFragments;
    Array<RenderPassDefinition>       RenderPasses;

    ShaderEffect() {};
    ShaderEffect(ShaderEffect& Other)
    {
        Name = Other.Name;
        ResourcePath = Other.ResourcePath;
        VertexLayouts = Other.VertexLayouts;
        Resources = Other.Resources;
        ConstantBuffers = Other.ConstantBuffers;
        UniformBuffers = Other.UniformBuffers;
        StorageBuffers = Other.StorageBuffers;
        RenderStates = Other.RenderStates;
        CodeFragments = Other.CodeFragments;
        RenderPasses = Array<RenderPassDefinition>(Other.RenderPasses.Length);

        for (int i = 0; i < Other.RenderPasses.Length; i++)
        {
            RenderPasses[i].Name = Other.RenderPasses[i].Name;
            RenderPasses[i].ShaderStages = Other.RenderPasses[i].ShaderStages;

            // Correctly assign the offsets
            RenderPasses[i].VertexLayout = &VertexLayouts[(Other.RenderPasses[i].VertexLayout - &Other.VertexLayouts[0])];
            RenderPasses[i].Resources = &Resources[(Other.RenderPasses[i].Resources - &Other.Resources[0])];
            RenderPasses[i].ConstantBuffers = &ConstantBuffers[(Other.RenderPasses[i].ConstantBuffers - &Other.ConstantBuffers[0])];
            RenderPasses[i].RenderState = &RenderStates[(Other.RenderPasses[i].RenderState - &Other.RenderStates[0])];
        }
    }

    ShaderEffect& operator=(const ShaderEffect& Other)
    {
        Name = Other.Name;
        ResourcePath = Other.ResourcePath;
        VertexLayouts = Other.VertexLayouts;
        Resources = Other.Resources;
        ConstantBuffers = Other.ConstantBuffers;
        UniformBuffers = Other.UniformBuffers;
        StorageBuffers = Other.StorageBuffers;
        RenderStates = Other.RenderStates;
        CodeFragments = Other.CodeFragments;
        RenderPasses = Array<RenderPassDefinition>(Other.RenderPasses.Length);

        for (int i = 0; i < Other.RenderPasses.Length; i++)
        {
            RenderPasses[i].Name = Other.RenderPasses[i].Name;
            RenderPasses[i].ShaderStages = Other.RenderPasses[i].ShaderStages;

            // Correctly assign the offsets
            RenderPasses[i].VertexLayout = &VertexLayouts[(Other.RenderPasses[i].VertexLayout - &Other.VertexLayouts[0])];
            RenderPasses[i].Resources = &Resources[(Other.RenderPasses[i].Resources - &Other.Resources[0])];
            RenderPasses[i].ConstantBuffers = &ConstantBuffers[(Other.RenderPasses[i].ConstantBuffers - &Other.ConstantBuffers[0])];
            RenderPasses[i].RenderState = &RenderStates[(Other.RenderPasses[i].RenderState - &Other.RenderStates[0])];
        }

        return *this;
    }
};

} // namespace kraft::shaderfx