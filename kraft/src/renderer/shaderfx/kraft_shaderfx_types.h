#pragma once

#include "core/kraft_string.h"
#include "containers/kraft_buffer.h"
#include "containers/kraft_array.h"
#include "renderer/kraft_renderer_types.h"

namespace kraft::renderer
{

struct VertexAttribute
{
    uint16                          Location = 0;
    uint16                          Binding = 0;
    uint16                          Offset = 0;
    ShaderDataType::Enum     Format = ShaderDataType::Count;
};

struct VertexInputBinding
{
    uint16                          Binding = 0;
    uint16                          Stride = 0;
    VertexInputRate::Enum           InputRate = VertexInputRate::Count;
};

struct VertexLayoutDefinition
{
    String                          Name;
    Array<VertexAttribute>          Attributes;
    Array<VertexInputBinding>       InputBindings;

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Attributes);
        Out->Write(InputBindings);
    }

    void ReadFrom(Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Attributes);
        In->Read(&InputBindings);
    }
};

struct ResourceBinding
{
    String             Name;
    uint16             Binding = 0;
    uint16             Size = 0;
    ResourceType::Enum Type;
    ShaderStage::Enum  Stage;

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Binding);
        Out->Write(Size);
        Out->Write(Type);
        Out->Write(Stage);
    }

    void ReadFrom(Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Binding);
        In->Read(&Size);
        In->Read(&Type);
        In->Read(&Stage);
    }
};

struct ResourceBindingsDefinition
{
    String                 Name;
    Array<ResourceBinding> ResourceBindings;

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(ResourceBindings);
    }

    void ReadFrom(Buffer* In)
    {
        In->Read(&Name);
        In->Read(&ResourceBindings);
    }
};

struct ShaderCodeFragment
{
    String                          Name;
    StringView                      Code;

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Code);
    }

    void ReadFrom(Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Code);
    }
};

struct RenderStateDefinition
{
    String             Name;
    CullMode::Enum     CullMode;
    CompareOp::Enum    ZTestOperation;
    bool               ZWriteEnable;
    BlendState         BlendMode;
    PolygonMode::Enum  PolygonMode;
    float              LineWidth;

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(CullMode);
        Out->Write(ZTestOperation);
        Out->Write(ZWriteEnable);
        Out->Write(BlendMode);
    }

    void ReadFrom(Buffer* In)
    {
        In->Read(&Name);
        In->Read(&CullMode);
        In->Read(&ZTestOperation);
        In->Read(&ZWriteEnable);
        In->Read(&BlendMode);
    }
};

struct ConstantBufferEntry
{
    String               Name;
    ShaderStage::Enum    Stage;
    ShaderDataType::Enum Type;

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Stage);
        Out->Write(Type);
    }

    void ReadFrom(Buffer* In)
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

    void WriteTo(Buffer* Out)
    {
        Out->Write(Name);
        Out->Write(Fields);
    }

    void ReadFrom(Buffer* In)
    {
        In->Read(&Name);
        In->Read(&Fields);
    }
};

struct RenderPassDefinition
{
    struct ShaderDefinition
    {
        ShaderStage::Enum   Stage;
        ShaderCodeFragment  CodeFragment;

        void WriteTo(Buffer* Out)
        {
            Out->Write(Stage);
            Out->Write(CodeFragment);
        }

        void ReadFrom(Buffer* In)
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
    Array<RenderStateDefinition>      RenderStates;
    Array<ShaderCodeFragment>         CodeFragments;
    Array<RenderPassDefinition>       RenderPasses;

    ShaderEffect() {}
    ShaderEffect(ShaderEffect& Other)
    {
        Name = Other.Name;
        ResourcePath = Other.ResourcePath;
        VertexLayouts = Other.VertexLayouts;
        RenderStates = Other.RenderStates;
        CodeFragments = Other.CodeFragments;
        Resources = Other.Resources;
        ConstantBuffers = Other.ConstantBuffers;
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
        RenderStates = Other.RenderStates;
        CodeFragments = Other.CodeFragments;
        Resources = Other.Resources;
        ConstantBuffers = Other.ConstantBuffers;
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

}