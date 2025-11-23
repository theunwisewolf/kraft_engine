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
    Lexer*  Lexer = nullptr;
    u32     ErrorLine = 0;
    u32     ErrorColumn = 0;
    u32     ErrorCode = 0;
    bool    ErroredOut = false;
    String8 error_str = {};
    bool    Verbose = false;

    void SetError(ArenaAllocator* arena, const char* format, ...);
    bool Parse(ArenaAllocator* arena, String8 file_path, kraft::Lexer* lexer, ShaderEffect* out);
    bool ParseDataType(ArenaAllocator* arena, const LexerToken* current_token, renderer::ShaderDataType* out_type);
    bool GenerateAST(ArenaAllocator* arena, ShaderEffect* effect);
    bool ParseIdentifier(ArenaAllocator* arena, ShaderEffect* effect, const LexerToken* token);
    bool ParseNamedToken(ArenaAllocator* arena, const LexerToken* Token, LexerNamedToken* OutToken);
    bool ParseShaderDeclaration(ArenaAllocator* arena, ShaderEffect* effect);
    bool ParseLayoutBlock(ArenaAllocator* arena, ShaderEffect* Effect);
    bool ParseGLSLBlock(ArenaAllocator* arena, ShaderEffect* Effect);
    bool ParseVertexLayout(ArenaAllocator* arena, VertexLayoutDefinition* Layout);
    bool ParseVertexAttribute(ArenaAllocator* arena, VertexAttribute* attribute);
    bool ParseVertexInputBinding(ArenaAllocator* arena, VertexInputBinding* Layout);
    bool ParseResourceBindings(ArenaAllocator* arena, ResourceBindingsDefinition* ResourceBindings);
    bool ParseConstantBuffer(ArenaAllocator* arena, ConstantBufferDefinition* CBufferDefinition);
    bool ParseUniformBuffer(ArenaAllocator* arena, UniformBufferDefinition* UBufferDefinition);
    bool ParseRenderStateBlock(ArenaAllocator* arena, ShaderEffect* Effect);
    bool ParseRenderState(ArenaAllocator* arena, RenderStateDefinition* State);
    bool ParseBlendFactor(ArenaAllocator* arena, const LexerToken* token, renderer::BlendFactor::Enum* factor);
    bool ParseBlendOp(ArenaAllocator* arena, const LexerToken* token, renderer::BlendOp::Enum* op);
    bool ParseRenderPassBlock(ArenaAllocator* arena, ShaderEffect* Effect);
};

bool LoadShaderFX(ArenaAllocator* arena, String8 path, ShaderEffect* shader);
bool ValidateShaderFX(const ShaderEffect* shader1, const ShaderEffect* shader2);

} // namespace kraft::shaderfx