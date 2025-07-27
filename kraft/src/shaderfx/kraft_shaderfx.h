#pragma once

#include "core/kraft_core.h"

// https://jorenjoestar.github.io/post/writing_shader_effect_language_1/

// The language grammar is similar to hydrafx with some changes

namespace kraft {
struct Lexer;
struct LexerToken;
struct LexerNamedToken;

struct ArenaAllocator;
} // namespace kraft

namespace kraft {
namespace renderer {

struct ShaderDataType;

namespace BlendFactor {
enum Enum : int;
}

namespace BlendOp {
enum Enum : int;
}

enum ShaderStageFlags : int;
} // namespace renderer
} // namespace kraft

namespace kraft::shaderfx {

struct ShaderEffect;
struct VertexLayoutDefinition;
struct ResourceBindingsDefinition;
struct ResourceBindingsDefinition;
struct ConstantBufferDefinition;
struct UniformBufferDefinition;
struct RenderStateDefinition;
struct RenderPassDefinition;

struct ShaderFXParser
{
    Lexer* Lexer = nullptr;
    uint32 ErrorLine = 0;
    uint32 ErrorColumn = 0;
    uint32 ErrorCode = 0;
    bool   ErroredOut = false;
    char   ErrorString[1024];
    bool   Verbose = false;

    void SetError(const char* Format, ...);
    bool Parse(const String& SourceFilePath, kraft::Lexer* Lexer, ShaderEffect* Out);
    bool ParseDataType(const LexerToken* CurrentToken, renderer::ShaderDataType* OutType);
    bool GenerateAST(ShaderEffect* Effect);
    bool ParseIdentifier(ShaderEffect* Effect, const LexerToken* Token);
    bool ParseNamedToken(const LexerToken* Token, LexerNamedToken* OutToken);
    bool ParseShaderDeclaration(ShaderEffect* Effect);
    bool ParseLayoutBlock(ShaderEffect* Effect);
    bool ParseGLSLBlock(ShaderEffect* Effect);
    bool ParseVertexLayout(VertexLayoutDefinition* Layout);
    bool ParseVertexAttribute(VertexLayoutDefinition* Layout);
    bool ParseVertexInputBinding(VertexLayoutDefinition* Layout);
    bool ParseResourceBindings(ResourceBindingsDefinition* ResourceBindings);
    bool ParseConstantBuffer(ConstantBufferDefinition* CBufferDefinition);
    bool ParseUniformBuffer(UniformBufferDefinition* UBufferDefinition);
    bool ParseRenderStateBlock(ShaderEffect* Effect);
    bool ParseRenderState(RenderStateDefinition* State);
    bool ParseBlendFactor(const LexerToken* Token, renderer::BlendFactor::Enum& Factor);
    bool ParseBlendOp(const LexerToken* Token, renderer::BlendOp::Enum& Factor);
    bool ParseRenderPassBlock(ShaderEffect* Effect);
    bool ParseRenderPassShaderStage(ShaderEffect* Effect, RenderPassDefinition* Pass, renderer::ShaderStageFlags ShaderStage);
};

bool LoadShaderFX(const String& Path, ShaderEffect* Shader);
bool ValidateShaderFX(const ShaderEffect& ShaderA, ShaderEffect& ShaderB);

} // namespace kraft::shaderfx