#pragma once

#include "core/kraft_core.h"
#include "core/kraft_string.h"
#include "core/kraft_lexer.h"
#include "containers/kraft_buffer.h"
#include "containers/kraft_array.h"
#include "resources/kraft_resource_types.h"
#include "renderer/shaderfx/kraft_shaderfx_types.h"

// https://jorenjoestar.github.io/post/writing_shader_effect_language_1/

// The language grammar is similar to hydrafx with some changes

namespace kraft::renderer
{

struct ShaderFXParser
{
    Lexer*          Lexer            = nullptr;
    String          SourceFilePath;
    ShaderEffect    Shader;
    uint64          ErrorLine;
    bool            Error;
    String          ErrorString;

    struct NamedToken
    {
        StringView Key;
        Token      Value;
    };

    void Create(const String& SourceFilePath, kraft::Lexer* Lexer);
    void Destroy();

    void GenerateAST();
    void ParseIdentifier(Token Token);
    bool ParseNamedToken(Token Token, NamedToken& OutToken);
    ShaderDataType::Enum ParseDataType(Token Token);
    void ParseShaderDeclaration();
    void ParseLayoutBlock();
    void ParseVertexLayout(VertexLayoutDefinition& Layout);
    void ParseVertexAttribute(VertexLayoutDefinition& Layout);
    void ParseVertexInputBinding(VertexLayoutDefinition& Layout);
    void ParseResourceBindings(ResourceBindingsDefinition& ResourceBindings);
    void ParseConstantBuffer(ConstantBufferDefinition& CBufferDefinition);
    void ParseGLSLBlock();
    void ParseRenderStateBlock();
    void ParseRenderState(RenderStateDefinition& State);
    bool ParseBlendFactor(Token Token, BlendFactor::Enum& Factor);
    bool ParseBlendOp(Token Token, BlendOp::Enum& Factor);

    void ParseRenderPassBlock();
    bool ParseRenderPassShaderStage(RenderPassDefinition& Pass, renderer::ShaderStage::Enum ShaderStage);
};

bool CompileShaderFX(const String& InputPath, const String& OutputPath);
bool CompileShaderFX(const ShaderEffect& Shader, const String& OutputPath);
bool LoadShaderFX(const String& Path, ShaderEffect& Shader);
bool ValidateShaderFX(const ShaderEffect& ShaderA, ShaderEffect& ShaderB);

}