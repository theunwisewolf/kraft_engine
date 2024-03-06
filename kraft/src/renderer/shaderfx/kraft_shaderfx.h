#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "core/kraft_lexer.h"
#include "containers/kraft_buffer.h"
#include "containers/kraft_array.h"
#include "renderer/kraft_renderer_types.h"

// https://jorenjoestar.github.io/post/writing_shader_effect_language_1/

// The language grammar is similar to hydrafx with some changes

namespace kraft::renderer
{

struct VertexAttribute
{
    uint16                          Location;
    uint16                          Binding;
    uint16                          Offset;
    VertexAttributeFormat::Enum     Format = VertexAttributeFormat::Count;
};

struct VertexInputBinding
{
    uint16                          Binding;
    uint16                          Stride;
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
    String                          Name;
    renderer::CullMode              CullMode;
    renderer::ZTestOp               ZTestOperation;
    bool                            ZWriteEnable;
    renderer::BlendState            BlendMode;

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

struct RenderPassDefinition
{
    struct ShaderDefinition
    {
        renderer::ShaderStage::Enum   Stage;
        ShaderCodeFragment            CodeFragment;

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

    String                          Name;
    const RenderStateDefinition*    RenderState;
    const VertexLayoutDefinition*   VertexLayout;
    Array<ShaderDefinition>         ShaderStages;
};

struct ShaderEffect
{
    String                          Name;
    String                          ResourcePath;
    Array<VertexLayoutDefinition>   VertexLayouts;
    Array<RenderStateDefinition>    RenderStates;
    Array<ShaderCodeFragment>       CodeFragments;
    Array<RenderPassDefinition>     RenderPasses;

    ShaderEffect() {}
    ShaderEffect(ShaderEffect& Other)
    {
        Name = Other.Name;
        ResourcePath = Other.ResourcePath;
        VertexLayouts = Other.VertexLayouts;
        RenderStates = Other.RenderStates;
        CodeFragments = Other.CodeFragments;
        RenderPasses = Array<RenderPassDefinition>(Other.RenderPasses.Length);
        
        for (int i = 0; i < Other.RenderPasses.Length; i++)
        {
            RenderPasses[i].Name = Other.RenderPasses[i].Name;
            RenderPasses[i].ShaderStages = Other.RenderPasses[i].ShaderStages;

            // Correctly assign the offsets
            RenderPasses[i].RenderState = &RenderStates[(Other.RenderPasses[i].RenderState - &Other.RenderStates[0])];
            RenderPasses[i].VertexLayout = &VertexLayouts[(Other.RenderPasses[i].VertexLayout - &Other.VertexLayouts[0])];
        }
    }
};

struct ShaderFXParser
{
    Lexer*          Lexer            = nullptr;
    String          SourceFilePath;
    ShaderEffect    Shader;
    uint64          ErrorLine;
    bool            Error;
    String          ErrorString;

    void Create(const String& SourceFilePath, kraft::Lexer* Lexer);
    void Destroy();

    void GenerateAST();
    void ParseIdentifier(Token Token);
    void ParseShaderDeclaration();
    void ParseLayoutBlock();
    void ParseVertexLayout(VertexLayoutDefinition& Layout);
    void ParseVertexAttribute(VertexLayoutDefinition& Layout);
    void ParseVertexInputBinding(VertexLayoutDefinition& Layout);
    void ParseGLSLBlock();
    void ParseRenderStateBlock();
    void ParseRenderState(RenderStateDefinition& State);
    bool ParseBlendFactor(Token Token, BlendFactor& Factor);
    bool ParseBlendOp(Token Token, BlendOp& Factor);

    void ParseRenderPassBlock();
    bool ParseRenderPassShaderStage(RenderPassDefinition& Pass, renderer::ShaderStage::Enum ShaderStage);
};

bool CompileShaderFX(const String& InputPath, const String& OutputPath);
bool CompileShaderFX(const ShaderEffect& Shader, const String& OutputPath);
bool LoadShaderFX(const String& Path, ShaderEffect& Shader);
bool ValidateShaderFX(const ShaderEffect& ShaderA, ShaderEffect& ShaderB);

}