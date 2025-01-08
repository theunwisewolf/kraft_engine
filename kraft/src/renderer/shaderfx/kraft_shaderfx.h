#pragma once

#include "core/kraft_core.h"

// https://jorenjoestar.github.io/post/writing_shader_effect_language_1/

// The language grammar is similar to hydrafx with some changes


namespace kraft {
struct Lexer;
struct Token;
struct NamedToken;

struct ArenaAllocator;
}

namespace kraft::renderer {

struct ShaderEffect;
struct VertexLayoutDefinition;
struct ResourceBindingsDefinition;
struct ResourceBindingsDefinition;
struct ConstantBufferDefinition;
struct UniformBufferDefinition;
struct RenderStateDefinition;
struct RenderPassDefinition;

namespace ShaderDataType {
enum Enum : int;
}

namespace BlendFactor {
enum Enum : int;
}

namespace BlendOp {
enum Enum : int;
}

enum ShaderStageFlags : int;

struct ShaderFXParser
{
    Lexer* Lexer = nullptr;
    uint64 ErrorLine;
    bool   Error;
    char   ErrorString[1024];
    bool   Verbose = false;

    ShaderEffect Parse(const String& SourceFilePath, kraft::Lexer* Lexer);

    void                 GenerateAST(ShaderEffect* Effect);
    void                 ParseIdentifier(ShaderEffect* Effect, const Token& Token);
    bool                 ParseNamedToken(const Token& Token, NamedToken* OutToken);
    ShaderDataType::Enum ParseDataType(const Token& Token);
    void                 ParseShaderDeclaration(ShaderEffect* Effect);
    void                 ParseLayoutBlock(ShaderEffect* Effect);
    void                 ParseGLSLBlock(ShaderEffect* Effect);
    void                 ParseVertexLayout(VertexLayoutDefinition* Layout);
    void                 ParseVertexAttribute(VertexLayoutDefinition* Layout);
    void                 ParseVertexInputBinding(VertexLayoutDefinition* Layout);
    void                 ParseResourceBindings(ResourceBindingsDefinition* ResourceBindings);
    void                 ParseConstantBuffer(ConstantBufferDefinition* CBufferDefinition);
    void                 ParseUniformBuffer(UniformBufferDefinition* UBufferDefinition);
    void                 ParseRenderStateBlock(ShaderEffect* Effect);
    void                 ParseRenderState(RenderStateDefinition* State);
    bool                 ParseBlendFactor(const Token& Token, BlendFactor::Enum& Factor);
    bool                 ParseBlendOp(const Token& Token, BlendOp::Enum& Factor);

    void ParseRenderPassBlock(ShaderEffect* Effect);
    bool ParseRenderPassShaderStage(ShaderEffect* Effect, RenderPassDefinition* Pass, renderer::ShaderStageFlags ShaderStage);
};

bool CompileShaderFX(const String& InputPath, const String& OutputPath, bool Verbose = false);
bool CompileShaderFX(ArenaAllocator* Arena, ShaderEffect& Shader, const String& OutputPath, bool Verbose = false);
bool LoadShaderFX(const String& Path, ShaderEffect* Shader);
bool ValidateShaderFX(const ShaderEffect& ShaderA, ShaderEffect& ShaderB);

}