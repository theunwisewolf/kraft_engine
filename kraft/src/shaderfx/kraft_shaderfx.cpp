#include "kraft_shaderfx.h"

#include <stdarg.h>

#include <containers/kraft_buffer.h>
#include <core/kraft_allocators.h>
#include <core/kraft_asserts.h>
#include <core/kraft_lexer.h>
#include <core/kraft_log.h>
#include <core/kraft_memory.h>
#include <core/kraft_string_view.h>
#include <platform/kraft_filesystem.h>

#include <renderer/kraft_renderer_types.h>
#include <shaderfx/kraft_shaderfx_types.h>

#define PARSER_ERROR_NEXT_TOKEN_READ_FAILED "Error while reading the next token %d"
#define PARSER_ERROR_TOKEN_MISMATCH         "Token mismatch\nExpected '%S' got '%S'"
#define PARSER_ERROR_TOKEN_KEYWORD_MISMATCH "Keyword mismatch\nExpected '%S' got '%S'"
#define PARSER_ERROR_INVALID_SHADER_STAGE   "Invalid shader stage '%S'"

namespace kraft::shaderfx {

int GetShaderStageFromString(String8 value)
{
    if (StringEqual(value, String8Raw("Vertex")))
    {
        return renderer::SHADER_STAGE_FLAGS_VERTEX;
    }
    else if (StringEqual(value, String8Raw("Fragment")))
    {
        return renderer::SHADER_STAGE_FLAGS_FRAGMENT;
    }
    else if (StringEqual(value, String8Raw("Compute")))
    {
        return renderer::SHADER_STAGE_FLAGS_COMPUTE;
    }
    else if (StringEqual(value, String8Raw("Geometry")))
    {
        return renderer::SHADER_STAGE_FLAGS_GEOMETRY;
    }

    return 0;
}

void ShaderFXParser::SetError(ArenaAllocator* arena, const char* format, ...)
{
#ifdef KRAFT_COMPILER_MSVC
    va_list args;
#else
    __builtin_va_list args;
#endif
    va_start(args, format);
    this->error_str = StringFormatV(arena, format, args);
    va_end(args);

    this->ErrorLine = this->Lexer->line;
    this->ErrorColumn = this->Lexer->column;
    this->ErroredOut = true;
}

bool ShaderFXParser::Parse(ArenaAllocator* arena, String8 file_path, kraft::Lexer* lexer, ShaderEffect* out)
{
    KASSERT(lexer);
    this->Lexer = lexer;
    out->resource_path = ArenaPushString8Copy(arena, file_path);

    return this->GenerateAST(arena, out);
}

bool ShaderFXParser::GenerateAST(ArenaAllocator* arena, ShaderEffect* effect)
{
    while (true)
    {
        LexerToken token;
        if (LexerError err = this->Lexer->NextToken(&token))
        {
            if (err == LEXER_ERROR_UNEXPECTED_EOF)
            {
                return true;
            }

            this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
            return false;
        }

        if (token.type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            if (!this->ParseIdentifier(arena, effect, &token))
            {
                return false;
            }
        }
        else if (token.type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }

    return true;
}

bool ShaderFXParser::ParseIdentifier(ArenaAllocator* arena, ShaderEffect* effect, const LexerToken* token)
{
    u8 character = token->text.ptr[0];
    switch (character)
    {
        case 'S':
        {
            if (token->MatchesKeyword(String8Raw("Shader")))
            {
                return this->ParseShaderDeclaration(arena, effect);
            }
        }
        break;
        case 'L':
        {
            if (token->MatchesKeyword(String8Raw("Layout")))
            {
                return this->ParseLayoutBlock(arena, effect);
            }
        }
        break;
        case 'G':
        {
            if (token->MatchesKeyword(String8Raw("GLSL")))
            {
                return this->ParseGLSLBlock(arena, effect);
            }
        }
        break;
        case 'R':
        {
            if (token->MatchesKeyword(String8Raw("RenderState")))
            {
                return this->ParseRenderStateBlock(arena, effect);
            }
        }
        break;
        case 'P':
        {
            if (token->MatchesKeyword(String8Raw("Pass")))
            {
                return this->ParseRenderPassBlock(arena, effect);
            }
        }
        break;
    }

    return true;
}

bool ShaderFXParser::ParseShaderDeclaration(ArenaAllocator* arena, ShaderEffect* effect)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
        return false;
    }

    effect->name = ArenaPushString8Copy(arena, token.text);
    return true;
}

bool ShaderFXParser::ParseLayoutBlock(ArenaAllocator* arena, ShaderEffect* effect)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    TempArena scratch = ScratchBegin(&arena, 1);

    u32                     vertex_layout_count = 0;
    VertexLayoutDefinition* vertex_layouts = ArenaPushArray(scratch.arena, VertexLayoutDefinition, 16);

    u32                         local_resource_count = 0;
    ResourceBindingsDefinition* local_resources = ArenaPushArray(scratch.arena, ResourceBindingsDefinition, 16);

    u32                         global_resource_count = 0;
    ResourceBindingsDefinition* global_resources = ArenaPushArray(scratch.arena, ResourceBindingsDefinition, 16);

    u32                       constant_buffer_count = 0;
    ConstantBufferDefinition* constant_buffers = ArenaPushArray(scratch.arena, ConstantBufferDefinition, 16);

    u32                      uniform_buffer_count = 0;
    UniformBufferDefinition* uniform_buffers = ArenaPushArray(scratch.arena, UniformBufferDefinition, 16);

    u32                      storage_buffer_count = 0;
    UniformBufferDefinition* storage_buffers = ArenaPushArray(scratch.arena, UniformBufferDefinition, 16);

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        if (token.MatchesKeyword(String8Raw("Vertex")))
        {
            // Consume "Vertex"
            if (LexerError err = this->Lexer->NextToken(&token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
                return false;
            }

            vertex_layouts[vertex_layout_count].name = ArenaPushString8Copy(arena, token.text);
            if (!this->ParseVertexLayout(arena, &vertex_layouts[vertex_layout_count]))
            {
                return false;
            }

            vertex_layout_count++;
        }
        // Local/Global resources
        else if (token.MatchesKeyword(String8Raw("local")) || token.MatchesKeyword(String8Raw("global")))
        {
            ResourceBindingType::Enum type = token.MatchesKeyword(String8Raw("global")) ? ResourceBindingType::Global : ResourceBindingType::Local;

            // Consume 'local'/'global' and move to the next token
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            if (!token.MatchesKeyword(String8Raw("Resource")))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_KEYWORD_MISMATCH, String8Raw("Resource"), token.text);
                return false;
            }

            // Consume "Resource" and move to the next token
            if (LexerError err = this->Lexer->NextToken(&token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
                return false;
            }

            ResourceBindingsDefinition* bindings_def = (type == ResourceBindingType::Local) ? &local_resources[local_resource_count] : &global_resources[global_resource_count];
            bindings_def->name = ArenaPushString8Copy(arena, token.text);
            if (!this->ParseResourceBindings(arena, bindings_def))
            {
                return false;
            }

            if (type == ResourceBindingType::Local)
            {
                local_resource_count++;
            }
            else
            {
                global_resource_count++;
            }
        }
        else if (token.MatchesKeyword(String8Raw("ConstantBuffer")))
        {
            // Consume "ConstantBuffer"
            if (LexerError err = this->Lexer->NextToken(&token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
                return false;
            }

            ConstantBufferDefinition* buffer_def = &constant_buffers[constant_buffer_count];
            buffer_def->name = ArenaPushString8Copy(arena, token.text);

            if (!this->ParseConstantBuffer(arena, buffer_def))
            {
                return false;
            }

            constant_buffer_count++;
        }
        else if (token.MatchesKeyword(String8Raw("UniformBuffer")))
        {
            // Consume "UniformBuffer"
            if (LexerError ErrorCode = this->Lexer->NextToken(&token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            UniformBufferDefinition* buffer_def = &uniform_buffers[uniform_buffer_count];
            buffer_def->name = ArenaPushString8Copy(arena, token.text);

            if (!this->ParseUniformBuffer(arena, buffer_def))
            {
                return false;
            }

            uniform_buffer_count++;
        }
        else if (token.MatchesKeyword(String8Raw("StorageBuffer")))
        {
            // Consume "StorageBuffer"
            if (LexerError ErrorCode = this->Lexer->NextToken(&token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            UniformBufferDefinition* buffer_def = &storage_buffers[storage_buffer_count];
            buffer_def->name = ArenaPushString8Copy(arena, token.text);

            if (!this->ParseUniformBuffer(arena, buffer_def))
            {
                return false;
            }

            storage_buffer_count++;
        }
    }

    effect->vertex_layout_count = vertex_layout_count;
    effect->vertex_layouts = ArenaPushArray(arena, VertexLayoutDefinition, vertex_layout_count);
    MemCpy(effect->vertex_layouts, vertex_layouts, sizeof(VertexLayoutDefinition) * vertex_layout_count);

    effect->local_resource_count = local_resource_count;
    effect->local_resources = ArenaPushArray(arena, ResourceBindingsDefinition, local_resource_count);
    MemCpy(effect->local_resources, local_resources, sizeof(ResourceBindingsDefinition) * local_resource_count);

    effect->global_resource_count = global_resource_count;
    effect->global_resources = ArenaPushArray(arena, ResourceBindingsDefinition, global_resource_count);
    MemCpy(effect->global_resources, global_resources, sizeof(ResourceBindingsDefinition) * global_resource_count);

    effect->constant_buffer_count = constant_buffer_count;
    effect->constant_buffers = ArenaPushArray(arena, ConstantBufferDefinition, constant_buffer_count);
    MemCpy(effect->constant_buffers, constant_buffers, sizeof(ConstantBufferDefinition) * constant_buffer_count);

    effect->uniform_buffer_count = uniform_buffer_count;
    effect->uniform_buffers = ArenaPushArray(arena, UniformBufferDefinition, uniform_buffer_count);
    MemCpy(effect->uniform_buffers, uniform_buffers, sizeof(UniformBufferDefinition) * uniform_buffer_count);

    effect->storage_buffer_count = storage_buffer_count;
    effect->storage_buffers = ArenaPushArray(arena, UniformBufferDefinition, storage_buffer_count);
    MemCpy(effect->storage_buffers, storage_buffers, sizeof(UniformBufferDefinition) * storage_buffer_count);

    ScratchEnd(scratch);

    return true;
}

bool ShaderFXParser::ParseResourceBindings(ArenaAllocator* arena, ResourceBindingsDefinition* bindings_def)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    TempArena        scratch = ScratchBegin(&arena, 1);
    u32              binding_count = 0;
    ResourceBinding* bindings = ArenaPushArray(scratch.arena, ResourceBinding, 16);

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        ResourceBinding* binding = &bindings[binding_count];
        if (token.MatchesKeyword(String8Raw("UniformBuffer")))
        {
            binding->type = renderer::ResourceType::UniformBuffer;
        }
        else if (token.MatchesKeyword(String8Raw("StorageBuffer")))
        {
            binding->type = renderer::ResourceType::StorageBuffer;
        }
        else if (token.MatchesKeyword(String8Raw("Sampler")))
        {
            binding->type = renderer::ResourceType::Sampler;
        }
        else
        {
            this->SetError(arena, "Invalid binding type '%S'", token.text);
            return false;
        }

        // Parse the name of the binding
        if (LexerError ErrorCode = this->Lexer->NextToken(&token))
        {
            this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
            return false;
        }

        binding->name = ArenaPushString8Copy(arena, token.text);
        while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            LexerNamedToken pair;
            if (!this->ParseNamedToken(arena, &token, &pair))
            {
                return false;
            }

            if (StringEqual(pair.key, String8Raw("Stage")))
            {
                binding->stage = (renderer::ShaderStageFlags)GetShaderStageFromString(pair.value.text);
                if (binding->stage == 0)
                {
                    this->SetError(arena, PARSER_ERROR_INVALID_SHADER_STAGE, pair.value.text);
                    return false;
                }
            }
            else if (StringEqual(pair.key, String8Raw("Binding")))
            {
                binding->binding = (u16)pair.value.float_value;
            }
            else if (StringEqual(pair.key, String8Raw("Size")))
            {
                binding->size = (u16)pair.value.float_value;
            }
            else if (StringEqual(pair.key, String8Raw("Set")))
            {
                binding->set = (u16)pair.value.float_value;
            }
            else
            {
                KWARN("Found unknown named key: '%S'", pair.key);
            }
        }

        binding_count++;
    }

    bindings_def->binding_count = binding_count;
    bindings_def->bindings = ArenaPushArray(arena, ResourceBinding, binding_count);
    MemCpy(bindings_def->bindings, bindings, sizeof(ResourceBinding) * binding_count);

    ScratchEnd(scratch);

    return true;
}

bool ShaderFXParser::ParseConstantBuffer(ArenaAllocator* arena, ConstantBufferDefinition* buffer_def)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    TempArena scratch = ScratchBegin(&arena, 1);

    u32                  entry_count = 0;
    ConstantBufferEntry* entries = ArenaPushArray(scratch.arena, ConstantBufferEntry, 16);

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        ConstantBufferEntry* entry = &entries[entry_count];

        // Parse the data type
        if (!ParseDataType(arena, &token, &entry->type))
        {
            this->SetError(arena, "Invalid data type for constant buffer entry '%S'", token.text);
            return false;
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        entry->name = ArenaPushString8Copy(arena, token.text);

        // Parse properties
        while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            LexerNamedToken pair;
            if (this->ParseNamedToken(arena, &token, &pair))
            {
                if (StringEqual(pair.key, String8Raw("Stage")))
                {
                    entry->stage = (renderer::ShaderStageFlags)GetShaderStageFromString(pair.value.text);
                    if (entry->stage == 0)
                    {
                        this->SetError(arena, PARSER_ERROR_INVALID_SHADER_STAGE, pair.value.text);
                        return false;
                    }
                }
            }
            else
            {
                return false;
            }
        }

        entry_count++;
    }

    buffer_def->field_count = entry_count;
    buffer_def->fields = ArenaPushArray(arena, ConstantBufferEntry, entry_count);
    MemCpy(buffer_def->fields, entries, sizeof(ConstantBufferEntry) * entry_count);

    ScratchEnd(scratch);

    return true;
}

bool ShaderFXParser::ParseUniformBuffer(ArenaAllocator* arena, UniformBufferDefinition* buffer_def)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    TempArena scratch = ScratchBegin(&arena, 1);

    u32                 entry_count = 0;
    UniformBufferEntry* entries = ArenaPushArray(scratch.arena, UniformBufferEntry, 16);

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        UniformBufferEntry* entry = &entries[entry_count];

        // Parse the data type
        if (!ParseDataType(arena, &token, &entry->type))
        {
            this->SetError(arena, "Invalid data type for constant buffer entry '%S'", token.text);
            return false;
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        entry->name = ArenaPushString8Copy(arena, token.text);

        if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_SEMICOLON), TokenType::String(token.type));
            return false;
        }

        entry_count++;
    }

    buffer_def->field_count = entry_count;
    buffer_def->fields = ArenaPushArray(arena, UniformBufferEntry, entry_count);
    MemCpy(buffer_def->fields, entries, sizeof(UniformBufferEntry) * entry_count);

    ScratchEnd(scratch);

    return true;
}

#define MATCH_FORMAT(type, value)                                                                                                                                                                      \
    if (current_token->MatchesKeyword(String8Raw(type)))                                                                                                                                               \
    {                                                                                                                                                                                                  \
        out_type->UnderlyingType = value;                                                                                                                                                              \
    }

bool ShaderFXParser::ParseDataType(ArenaAllocator* arena, const LexerToken* current_token, renderer::ShaderDataType* out_type)
{
    char character = current_token->text.ptr[0];
    out_type->UnderlyingType = renderer::ShaderDataType::Count;
    switch (character)
    {
        case 'f': // "Float", "Float2", "Float3", "Float4"
        case 'F':
        {
            MATCH_FORMAT("float", renderer::ShaderDataType::Float);
            MATCH_FORMAT("Float", renderer::ShaderDataType::Float);
            MATCH_FORMAT("float2", renderer::ShaderDataType::Float2);
            MATCH_FORMAT("Float2", renderer::ShaderDataType::Float2);
            MATCH_FORMAT("float3", renderer::ShaderDataType::Float3);
            MATCH_FORMAT("Float3", renderer::ShaderDataType::Float3);
            MATCH_FORMAT("float4", renderer::ShaderDataType::Float4);
            MATCH_FORMAT("Float4", renderer::ShaderDataType::Float4);
        }
        break;
        case 'm': // "Mat4"
        case 'M': // "Mat4"
        {
            MATCH_FORMAT("mat4", renderer::ShaderDataType::Mat4);
            MATCH_FORMAT("Mat4", renderer::ShaderDataType::Mat4);
        }
        break;
        case 'b': // "Byte", "Byte4N"
        case 'B': // "Byte", "Byte4N"
        {
            MATCH_FORMAT("byte", renderer::ShaderDataType::Byte);
            MATCH_FORMAT("Byte", renderer::ShaderDataType::Byte);
            MATCH_FORMAT("byte4n", renderer::ShaderDataType::Byte4N);
            MATCH_FORMAT("Byte4N", renderer::ShaderDataType::Byte4N);
        }
        break;
        case 'u': // "UByte", "UByte4N", "UInt", "UInt2", "UInt4"
        case 'U': // "UByte", "UByte4N", "UInt", "UInt2", "UInt4"
        {
            MATCH_FORMAT("ubyte", renderer::ShaderDataType::UByte);
            MATCH_FORMAT("UByte", renderer::ShaderDataType::UByte);
            MATCH_FORMAT("ubyte4n", renderer::ShaderDataType::UByte4N);
            MATCH_FORMAT("UByte4N", renderer::ShaderDataType::UByte4N);
            MATCH_FORMAT("uint", renderer::ShaderDataType::UInt);
            MATCH_FORMAT("UInt", renderer::ShaderDataType::UInt);
            MATCH_FORMAT("uint2", renderer::ShaderDataType::UInt2);
            MATCH_FORMAT("UInt2", renderer::ShaderDataType::UInt2);
            MATCH_FORMAT("uint4", renderer::ShaderDataType::UInt4);
            MATCH_FORMAT("UInt4", renderer::ShaderDataType::UInt4);
        }
        break;
        case 's': // "Short2", "Short2N", "Short4", "Short4N"
        case 'S': // "Short2", "Short2N", "Short4", "Short4N"
        {
            MATCH_FORMAT("short2", renderer::ShaderDataType::Short2);
            MATCH_FORMAT("Short2", renderer::ShaderDataType::Short2);
            MATCH_FORMAT("short2n", renderer::ShaderDataType::Short2N);
            MATCH_FORMAT("Short2N", renderer::ShaderDataType::Short2N);
            MATCH_FORMAT("short4", renderer::ShaderDataType::Short4);
            MATCH_FORMAT("Short4", renderer::ShaderDataType::Short4);
            MATCH_FORMAT("short4n", renderer::ShaderDataType::Short4N);
            MATCH_FORMAT("Short4N", renderer::ShaderDataType::Short4N);
        }
        break;
        case 't':
        {
            MATCH_FORMAT("texID", renderer::ShaderDataType::TextureID);
        }
        break;
        default: return false;
    }

    // Invalid type
    if (out_type->UnderlyingType == renderer::ShaderDataType::Count)
    {
        this->SetError(arena, "Invalid data type '%S'", current_token->text);
        return false;
    }

    // Is this an array type?
    u64        current_cursor_position = Lexer->position;
    LexerToken next_token;
    if (LexerError ErrorCode = Lexer->NextToken(&next_token))
    {
        this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    // Array type?
    if (next_token.type == TokenType::TOKEN_TYPE_OPEN_BRACKET)
    {
        if (LexerError ErrorCode = Lexer->NextToken(&next_token))
        {
            this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
            return false;
        }

        if (next_token.type == TokenType::TOKEN_TYPE_NUMBER)
        {
            if (next_token.float_value < 0.0f)
            {
                this->SetError(arena, "Array size cannot be negative");
                return false;
            }

            out_type->ArraySize = (uint16)next_token.float_value;

            // Move forward
            if (LexerError ErrorCode = Lexer->NextToken(&next_token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }
        }

        if (next_token.type != TokenType::TOKEN_TYPE_CLOSE_BRACKET)
        {
            this->SetError(arena, "Array not terminated");
            return false;
        }
    }
    else
    {
        // Non-Array type values have an array size of 1
        out_type->ArraySize = 1;

        // Go back to the original position
        Lexer->MoveCursorTo(current_cursor_position);
    }

    return true;
}

#undef MATCH_FORMAT

bool ShaderFXParser::ParseNamedToken(ArenaAllocator* arena, const LexerToken* current_token, LexerNamedToken* out_token)
{
    if (current_token->type != TokenType::TOKEN_TYPE_IDENTIFIER)
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(current_token->type));
        return false;
    }

    out_token->key = current_token->text;
    LexerToken next_token;
    if (!this->Lexer->ExpectToken(&next_token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_PARENTHESIS), TokenType::String(current_token->type));
        return false;
    }

    // Skip past the parenthesis
    if (LexerError error_code = this->Lexer->NextToken(&next_token))
    {
        this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, error_code);
        return false;
    }

    out_token->value = next_token;
    if (!this->Lexer->ExpectToken(&next_token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_PARENTHESIS), TokenType::String(current_token->type));
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseVertexLayout(ArenaAllocator* arena, VertexLayoutDefinition* layout)
{
    const int  MAX_ATTRIBUTES = 16;
    const int  MAX_BINDINGS = 16;
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    TempArena           scratch = ScratchBegin(&arena, 1);
    VertexAttribute*    attributes = ArenaPushArray(scratch.arena, VertexAttribute, MAX_ATTRIBUTES);
    u32                 attribute_count = 0;
    VertexInputBinding* bindings = ArenaPushArray(scratch.arena, VertexInputBinding, MAX_BINDINGS);
    u32                 binding_count = 0;

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        if (token.MatchesKeyword(String8Raw("Attribute")))
        {
            if (!this->ParseVertexAttribute(arena, &attributes[attribute_count]))
            {
                return false;
            }

            attribute_count++;
        }
        else if (token.MatchesKeyword(String8Raw("Binding")))
        {
            if (!this->ParseVertexInputBinding(arena, &bindings[binding_count]))
            {
                return false;
            }

            binding_count++;
        }
    }

    layout->attribute_count = attribute_count;
    layout->input_binding_count = binding_count;
    layout->attributes = ArenaPushArray(arena, VertexAttribute, attribute_count);
    layout->input_bindings = ArenaPushArray(arena, VertexInputBinding, binding_count);
    MemCpy(layout->attributes, attributes, sizeof(VertexAttribute) * attribute_count);
    MemCpy(layout->input_bindings, bindings, sizeof(VertexInputBinding) * binding_count);

    ScratchEnd(scratch);

    return true;
}

bool ShaderFXParser::ParseVertexAttribute(ArenaAllocator* arena, VertexAttribute* attribute)
{
    LexerToken current_token;
    attribute->format = renderer::ShaderDataType::Invalid();

    if (!this->Lexer->ExpectToken(&current_token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(current_token.type));
        return false;
    }

    // Format identifier
    if (!ParseDataType(arena, &current_token, &attribute->format))
    {
        return false;
    }

    if (!this->Lexer->ExpectToken(&current_token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(current_token.type));
        return false;
    }

    // Attribute name
    // Skip
    // Attribute.Name = String(Token, Token.Length);

    // Binding
    if (this->Lexer->ExpectToken(&current_token, TokenType::TOKEN_TYPE_NUMBER))
    {
        attribute->binding = (u16)current_token.float_value;
    }
    else
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(current_token.type));
        return false;
    }

    // Location
    if (this->Lexer->ExpectToken(&current_token, TokenType::TOKEN_TYPE_NUMBER))
    {
        attribute->location = (u16)current_token.float_value;
    }
    else
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(current_token.type));
        return false;
    }

    // Offset
    if (this->Lexer->ExpectToken(&current_token, TokenType::TOKEN_TYPE_NUMBER))
    {
        attribute->offset = (u16)current_token.float_value;
    }
    else
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(current_token.type));
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseVertexInputBinding(ArenaAllocator* arena, VertexInputBinding* input_binding)
{
    LexerToken token;

    // Binding
    if (this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER))
    {
        input_binding->binding = (u16)token.float_value;
    }
    else
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(token.type));
        return false;
    }

    // Stride
    if (this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER))
    {
        input_binding->stride = (u16)token.float_value;
    }
    else
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(token.type));
        return false;
    }

    // Input Rate
    if (LexerError err = this->Lexer->NextToken(&token))
    {
        this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
        return false;
    }

    if (token.MatchesKeyword(String8Raw("vertex")))
    {
        input_binding->input_rate = renderer::VertexInputRate::PerVertex;
    }
    else if (token.MatchesKeyword(String8Raw("instance")))
    {
        input_binding->input_rate = renderer::VertexInputRate::PerInstance;
    }
    else
    {
        this->SetError(arena, "Invalid value for VertexInputRate: '%S'", token.text);
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseGLSLBlock(ArenaAllocator* arena, ShaderEffect* effect)
{
    LexerToken token;
    if (LexerError err = this->Lexer->NextToken(&token))
    {
        this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
        return false;
    }

    effect->code_fragment.name = ArenaPushString8Copy(arena, token.text);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    if (LexerError err = this->Lexer->NextToken(&token))
    {
        this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
        return false;
    }

    u8* shader_code_start = token.text.ptr;
    int open_brace_count = 1;
    while (open_brace_count)
    {
        if (token.type == TokenType::TOKEN_TYPE_OPEN_BRACE)
        {
            open_brace_count++;
        }
        else if (token.type == TokenType::TOKEN_TYPE_CLOSE_BRACE)
        {
            open_brace_count--;
        }

        if (open_brace_count)
        {
            if (LexerError ErrorCode = this->Lexer->NextToken(&token))
            {
                if (ErrorCode == LEXER_ERROR_UNEXPECTED_CHARACTER && token.type == TokenType::TOKEN_TYPE_UNKNOWN)
                {
                    continue;
                }

                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }
        }
    }

    u64 shader_code_length = token.text.ptr - shader_code_start;
    effect->code_fragment.code = ArenaPushString8Copy(arena, String8FromPtrAndLength(shader_code_start, shader_code_length));

    if (this->Verbose)
    {
        KDEBUG("%S", effect->code_fragment.code);
    }

    return true;
}

bool ShaderFXParser::ParseRenderStateBlock(ArenaAllocator* arena, ShaderEffect* effect)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    // Parse all the render states
    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        // Expect "State"
        if (token.MatchesKeyword(String8Raw("State")))
        {
            // Move past "State"
            if (LexerError err = this->Lexer->NextToken(&token))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
                return false;
            }

            effect->render_state.name = ArenaPushString8Copy(arena, token.text);
            this->ParseRenderState(arena, &effect->render_state);
        }
    }

    return true;
}

bool ShaderFXParser::ParseRenderState(ArenaAllocator* arena, RenderStateDefinition* state)
{
    LexerToken token;
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        if (token.MatchesKeyword(String8Raw("Cull")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            if (token.MatchesKeyword(String8Raw("Back")))
            {
                state->cull_mode = renderer::CullModeFlags::Back;
            }
            else if (token.MatchesKeyword(String8Raw("Front")))
            {
                state->cull_mode = renderer::CullModeFlags::Front;
            }
            else if (token.MatchesKeyword(String8Raw("FrontAndBack")))
            {
                state->cull_mode = renderer::CullModeFlags::FrontAndBack;
            }
            else if (token.MatchesKeyword(String8Raw("Off")) || token.MatchesKeyword(String8Raw("None")))
            {
                state->cull_mode = renderer::CullModeFlags::None;
            }
            else
            {
                this->SetError(arena, "Invalid value for CullMode: '%S'", token.text);
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("ZTest")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            bool Valid = false;
            for (int i = 0; i < renderer::CompareOp::Count; i++)
            {
                if (token.MatchesKeyword(renderer::CompareOp::String((renderer::CompareOp::Enum)i)))
                {
                    state->z_test_op = (renderer::CompareOp::Enum)i;
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->SetError(arena, "Invalid value for ZTest: '%S'", token.text);
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("ZWrite")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            if (token.MatchesKeyword(String8Raw("On")))
            {
                state->z_write_enable = true;
            }
            else if (token.MatchesKeyword(String8Raw("Off")))
            {
                state->z_write_enable = false;
            }
            else
            {
                this->SetError(arena, "Invalid value for ZWrite: '%S'", token.text);
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("Blend")))
        {
            // Lot of tokens to parse here

            // SrcColor
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            // Blending turned off
            if (token.MatchesKeyword(String8Raw("Off")) || token.MatchesKeyword(String8Raw("None")))
            {
                state->blend_enable = false;
                continue;
            }

            state->blend_enable = true;
            if (!this->ParseBlendFactor(arena, &token, &state->blend_mode.src_color_blend_factor))
            {
                return false;
            }

            // DstColor
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            if (!this->ParseBlendFactor(arena, &token, &state->blend_mode.dst_color_blend_factor))
            {
                return false;
            }

            // Comma
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_COMMA))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_COMMA), TokenType::String(token.type));
                return false;
            }

            // SrcAlpha
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            if (!this->ParseBlendFactor(arena, &token, &state->blend_mode.src_alpha_blend_factor))
            {
                return false;
            }

            // DstAlpha
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            if (!this->ParseBlendFactor(arena, &token, &state->blend_mode.dst_alpha_blend_factor))
            {
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("BlendOp")))
        {
            // ColorBlendOp
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            if (!this->ParseBlendOp(arena, &token, &state->blend_mode.color_blend_op))
            {
                return false;
            }

            // Comma
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_COMMA))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_COMMA), TokenType::String(token.type));
                return false;
            }

            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            // AlphaBlendOp
            if (!this->ParseBlendOp(arena, &token, &state->blend_mode.alpha_blend_op))
            {
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("PolygonMode")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            for (int i = 0; i < renderer::PolygonMode::Count; i++)
            {
                if (StringEqual(token.text, renderer::PolygonMode::strings[i]))
                {
                    state->polygon_mode = (renderer::PolygonMode::Enum)i;
                    break;
                }
            }
        }
        else if (token.MatchesKeyword(String8Raw("LineWidth")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_NUMBER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(token.type));
                return false;
            }

            state->line_width = (f32)token.float_value;
        }
    }

    return true;
}

bool ShaderFXParser::ParseBlendFactor(ArenaAllocator* arena, const LexerToken* token, renderer::BlendFactor::Enum* factor)
{
    bool valid = false;
    for (int i = 0; i < renderer::BlendFactor::Count; i++)
    {
        if (token->MatchesKeyword(renderer::BlendFactor::strings[i]))
        {
            *factor = (renderer::BlendFactor::Enum)i;
            valid = true;
            break;
        }
    }

    if (!valid)
    {
        this->SetError(arena, "Invalid value for BlendFactor: '%S'", token->text);
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseBlendOp(ArenaAllocator* arena, const LexerToken* token, renderer::BlendOp::Enum* op)
{
    bool valid = false;
    for (int i = 0; i < renderer::BlendOp::Count; i++)
    {
        if (token->MatchesKeyword(renderer::BlendOp::strings[i]))
        {
            *op = (renderer::BlendOp::Enum)i;
            valid = true;
            break;
        }
    }

    if (!valid)
    {
        this->SetError(arena, "Invalid value for BlendOp: '%S'", token->text);
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseRenderPassBlock(ArenaAllocator* arena, ShaderEffect* effect)
{
    // Consume the name of the pass
    LexerToken token;
    if (LexerError err = this->Lexer->NextToken(&token))
    {
        this->SetError(arena, PARSER_ERROR_NEXT_TOKEN_READ_FAILED, err);
        return false;
    }

    effect->render_pass_def.name = ArenaPushString8Copy(arena, token.text);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(token.type));
        return false;
    }

    TempArena scratch = ScratchBegin(&arena, 1);

    u32                                     shader_def_count = 0;
    RenderPassDefinition::ShaderDefinition* shader_defs = ArenaPushArray(scratch.arena, RenderPassDefinition::ShaderDefinition, 6);

    while (!this->Lexer->EqualsToken(&token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (token.type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
            return false;
        }

        if (token.MatchesKeyword(String8Raw("RenderState")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            StringView name = StringView((char*)token.text.ptr, token.text.count);
            effect->render_pass_def.render_state = &effect->render_state;
        }
        else if (token.MatchesKeyword(String8Raw("VertexLayout")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            bool valid = false;
            for (i32 i = 0; i < effect->vertex_layout_count; i++)
            {
                if (StringEqual(effect->vertex_layouts[i].name, token.text))
                {
                    effect->render_pass_def.vertex_layout = &effect->vertex_layouts[i];
                    valid = true;
                    break;
                }
            }

            if (!valid)
            {
                this->SetError(arena, "Unknown VertexLayout: '%S'", token.text);
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("Resources")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            // It is possible for shaders to have no local resources
            if (effect->local_resource_count == 0)
                continue;

            bool valid = false;
            for (int i = 0; i < effect->local_resource_count; i++)
            {
                if (StringEqual(effect->local_resources[i].name, token.text))
                {
                    effect->render_pass_def.resources = &effect->local_resources[i];
                    valid = true;
                    break;
                }
            }

            if (!valid)
            {
                this->SetError(arena, "Unknown Resource: '%S'", token.text);
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("ConstantBuffer")))
        {
            if (!this->Lexer->ExpectToken(&token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(arena, PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(token.type));
                return false;
            }

            bool valid = false;
            for (int i = 0; i < effect->constant_buffer_count; i++)
            {
                if (StringEqual(effect->constant_buffers[i].name, token.text))
                {
                    effect->render_pass_def.contant_buffers = &effect->constant_buffers[i];
                    valid = true;
                    break;
                }
            }

            if (!valid)
            {
                this->SetError(arena, "Unknown ConstantBuffer: '%S'", token.text);
                return false;
            }
        }
        else if (token.MatchesKeyword(String8Raw("VertexShader")))
        {
            shader_defs[shader_def_count].stage = renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX;
            shader_defs[shader_def_count].code_fragment = effect->code_fragment;

            shader_def_count++;
        }
        else if (token.MatchesKeyword(String8Raw("FragmentShader")))
        {
            shader_defs[shader_def_count].stage = renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT;
            shader_defs[shader_def_count].code_fragment = effect->code_fragment;

            shader_def_count++;
        }
    }

    effect->render_pass_def.shader_stage_count = shader_def_count;
    effect->render_pass_def.shader_stages = ArenaPushArray(arena, RenderPassDefinition::ShaderDefinition, shader_def_count);
    MemCpy(effect->render_pass_def.shader_stages, shader_defs, sizeof(RenderPassDefinition::ShaderDefinition) * shader_def_count);

    ScratchEnd(scratch);

    return true;
}

bool LoadShaderFX(ArenaAllocator* arena, String8 path, ShaderEffect* effect)
{
    KASSERT(arena);
    filesystem::FileHandle File;

    // Read test
    if (!filesystem::OpenFile(path, filesystem::FILE_OPEN_MODE_READ, true, &File))
    {
        KERROR("Failed to read file %S", path);
        return false;
    }

    u64 BinaryBufferSize = kraft::filesystem::GetFileSize(&File) + 1;
    u8* BinaryBuffer = (uint8*)kraft::Malloc(BinaryBufferSize, MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&File, &BinaryBuffer);
    filesystem::CloseFile(&File);

    kraft::Buffer Reader((char*)BinaryBuffer, BinaryBufferSize);
    effect->name = Reader.ReadString(arena);
    effect->resource_path = Reader.ReadString(arena);

    effect->vertex_layout_count = Reader.Readu32();
    effect->vertex_layouts = ArenaPushArray(arena, VertexLayoutDefinition, effect->vertex_layout_count);
    for (u32 i = 0; i < effect->vertex_layout_count; i++)
    {
        effect->vertex_layouts[i].name = Reader.ReadString(arena);

        effect->vertex_layouts[i].attribute_count = Reader.Readu32();
        effect->vertex_layouts[i].attributes = ArenaPushArray(arena, VertexAttribute, effect->vertex_layouts[i].attribute_count);
        for (u32 j = 0; j < effect->vertex_layouts[i].attribute_count; j++)
        {
            effect->vertex_layouts[i].attributes[j].location = Reader.Readu16();
            effect->vertex_layouts[i].attributes[j].binding = Reader.Readu16();
            effect->vertex_layouts[i].attributes[j].offset = Reader.Readu16();
            // Reader.ReadRaw(&effect->vertex_layouts[i].attributes[j].format, sizeof(renderer::ShaderDataType));
            effect->vertex_layouts[i].attributes[j].format.UnderlyingType = (renderer::ShaderDataType::Enum)Reader.Readu8();
            effect->vertex_layouts[i].attributes[j].format.ArraySize = Reader.Readu16();
        }

        effect->vertex_layouts[i].input_binding_count = Reader.Readu32();
        effect->vertex_layouts[i].input_bindings = ArenaPushArray(arena, VertexInputBinding, effect->vertex_layouts[i].input_binding_count);
        for (u32 j = 0; j < effect->vertex_layouts[i].input_binding_count; j++)
        {
            effect->vertex_layouts[i].input_bindings[j].binding = Reader.Readu16();
            effect->vertex_layouts[i].input_bindings[j].stride = Reader.Readu16();
            effect->vertex_layouts[i].input_bindings[j].input_rate = (renderer::VertexInputRate::Enum)Reader.Readi32();
        }
    }

    effect->local_resource_count = Reader.Readu32();
    effect->local_resources = ArenaPushArray(arena, ResourceBindingsDefinition, effect->local_resource_count);
    for (u32 i = 0; i < effect->local_resource_count; i++)
    {
        effect->local_resources[i].name = Reader.ReadString(arena);
        effect->local_resources[i].binding_count = Reader.Readu32();
        effect->local_resources[i].bindings = ArenaPushArray(arena, ResourceBinding, effect->local_resources[i].binding_count);
        for (u32 j = 0; j < effect->local_resources[i].binding_count; j++)
        {
            effect->local_resources[i].bindings[j].name = Reader.ReadString(arena);
            effect->local_resources[i].bindings[j].set = Reader.Readu16();
            effect->local_resources[i].bindings[j].binding = Reader.Readu16();
            effect->local_resources[i].bindings[j].size = Reader.Readu16();
            effect->local_resources[i].bindings[j].parent_index = Reader.Readi16();
            effect->local_resources[i].bindings[j].type = (renderer::ResourceType::Enum)Reader.Readi32();
            effect->local_resources[i].bindings[j].stage = (renderer::ShaderStageFlags)Reader.Readi32();
        }
    }

    effect->global_resource_count = Reader.Readu32();
    effect->global_resources = ArenaPushArray(arena, ResourceBindingsDefinition, effect->global_resource_count);
    for (u32 i = 0; i < effect->global_resource_count; i++)
    {
        effect->global_resources[i].name = Reader.ReadString(arena);
        effect->global_resources[i].binding_count = Reader.Readu32();
        effect->global_resources[i].bindings = ArenaPushArray(arena, ResourceBinding, effect->global_resources[i].binding_count);
        for (u32 j = 0; j < effect->global_resources[i].binding_count; j++)
        {
            effect->global_resources[i].bindings[j].name = Reader.ReadString(arena);
            effect->global_resources[i].bindings[j].set = Reader.Readu16();
            effect->global_resources[i].bindings[j].binding = Reader.Readu16();
            effect->global_resources[i].bindings[j].size = Reader.Readu16();
            effect->global_resources[i].bindings[j].parent_index = Reader.Readi16();
            effect->global_resources[i].bindings[j].type = (renderer::ResourceType::Enum)Reader.Readi32();
            effect->global_resources[i].bindings[j].stage = (renderer::ShaderStageFlags)Reader.Readi32();
        }
    }

    effect->constant_buffer_count = Reader.Readu32();
    effect->constant_buffers = ArenaPushArray(arena, ConstantBufferDefinition, effect->constant_buffer_count);
    for (u32 i = 0; i < effect->constant_buffer_count; i++)
    {
        effect->constant_buffers[i].name = Reader.ReadString(arena);
        effect->constant_buffers[i].field_count = Reader.Readu32();
        effect->constant_buffers[i].fields = ArenaPushArray(arena, ConstantBufferEntry, effect->constant_buffers[i].field_count);
        for (u32 j = 0; j < effect->constant_buffers[i].field_count; j++)
        {
            effect->constant_buffers[i].fields[j].name = Reader.ReadString(arena);
            effect->constant_buffers[i].fields[j].stage = (renderer::ShaderStageFlags)Reader.Readi32();
            Reader.ReadRaw(&effect->constant_buffers[i].fields[j].type, sizeof(renderer::ShaderDataType));
        }
    }

    effect->uniform_buffer_count = Reader.Readu32();
    effect->uniform_buffers = ArenaPushArray(arena, UniformBufferDefinition, effect->uniform_buffer_count);
    for (u32 i = 0; i < effect->uniform_buffer_count; i++)
    {
        effect->uniform_buffers[i].name = Reader.ReadString(arena);
        effect->uniform_buffers[i].field_count = Reader.Readu32();
        effect->uniform_buffers[i].fields = ArenaPushArray(arena, UniformBufferEntry, effect->uniform_buffers[i].field_count);
        for (u32 j = 0; j < effect->uniform_buffers[i].field_count; j++)
        {
            effect->uniform_buffers[i].fields[j].name = Reader.ReadString(arena);
            Reader.ReadRaw(&effect->uniform_buffers[i].fields[j].type, sizeof(renderer::ShaderDataType));
        }
    }

    effect->storage_buffer_count = Reader.Readu32();
    effect->storage_buffers = ArenaPushArray(arena, UniformBufferDefinition, effect->storage_buffer_count);
    for (u32 i = 0; i < effect->storage_buffer_count; i++)
    {
        effect->storage_buffers[i].name = Reader.ReadString(arena);
        effect->storage_buffers[i].field_count = Reader.Readu32();
        effect->storage_buffers[i].fields = ArenaPushArray(arena, UniformBufferEntry, effect->storage_buffers[i].field_count);
        for (u32 j = 0; j < effect->storage_buffers[i].field_count; j++)
        {
            effect->storage_buffers[i].fields[j].name = Reader.ReadString(arena);
            Reader.ReadRaw(&effect->storage_buffers[i].fields[j].type, sizeof(renderer::ShaderDataType));
        }
    }

    // RenderState
    effect->render_state.name = Reader.ReadString(arena);
    effect->render_state.cull_mode = (renderer::CullModeFlags::Enum)Reader.Readi32();
    effect->render_state.z_test_op = (renderer::CompareOp::Enum)Reader.Readi32();
    effect->render_state.z_write_enable = Reader.Readbool();
    effect->render_state.blend_enable = Reader.Readbool();
    Reader.ReadRaw(&effect->render_state.blend_mode, sizeof(renderer::BlendState));
    effect->render_state.polygon_mode = (renderer::PolygonMode::Enum)Reader.Readi32();
    effect->render_state.line_width = Reader.Readf32();

    effect->render_pass_def.name = Reader.ReadString(arena);

    i64 vertex_layout_offset = Reader.Readi64();
    effect->render_pass_def.vertex_layout = &effect->vertex_layouts[vertex_layout_offset];

    i64 resources_offset = Reader.Readi64();
    effect->render_pass_def.resources = resources_offset > -1 ? &effect->local_resources[resources_offset] : 0;

    i64 constant_buffers_offset = Reader.Readi64();
    effect->render_pass_def.contant_buffers = &effect->constant_buffers[constant_buffers_offset];

    // TODO: is this required?
    i64 _render_state_offset = Reader.Readi64();
    effect->render_pass_def.render_state = &effect->render_state;

    effect->render_pass_def.shader_stage_count = Reader.Readu32();
    effect->render_pass_def.shader_stages = ArenaPushArray(arena, RenderPassDefinition::ShaderDefinition, effect->render_pass_def.shader_stage_count);
    for (u32 i = 0; i < effect->render_pass_def.shader_stage_count; i++)
    {
        effect->render_pass_def.shader_stages[i].stage = (renderer::ShaderStageFlags)Reader.Readi32();
        effect->render_pass_def.shader_stages[i].code_fragment.name = Reader.ReadString(arena);
        effect->render_pass_def.shader_stages[i].code_fragment.code = Reader.ReadString(arena);
    }

    kraft::Free(BinaryBuffer, BinaryBufferSize, MEMORY_TAG_FILE_BUF);

    return true;
}

bool ValidateShaderFX(const ShaderEffect* shader1, const ShaderEffect* shader2)
{
    KASSERT(StringEqual(shader1->name, shader2->name));
    KASSERT(StringEqual(shader1->resource_path, shader2->resource_path));

    // Buffers
    KASSERT(shader1->uniform_buffer_count == shader2->uniform_buffer_count);
    for (int i = 0; i < shader1->uniform_buffer_count; i++)
    {
        KASSERT(StringEqual(shader1->uniform_buffers[i].name, shader2->uniform_buffers[i].name));
        KASSERT(shader1->uniform_buffers[i].field_count == shader2->uniform_buffers[i].field_count);
        for (int j = 0; j < shader1->uniform_buffers[i].field_count; j++)
        {
            KASSERT(StringEqual(shader1->uniform_buffers[i].fields[j].name, shader2->uniform_buffers[i].fields[j].name));
            KASSERT(shader1->uniform_buffers[i].fields[j].type == shader2->uniform_buffers[i].fields[j].type);
        }
    }

    KASSERT(shader1->storage_buffer_count == shader2->storage_buffer_count);
    for (int i = 0; i < shader1->storage_buffer_count; i++)
    {
        KASSERT(StringEqual(shader1->storage_buffers[i].name, shader2->storage_buffers[i].name));
        KASSERT(shader1->storage_buffers[i].field_count == shader2->storage_buffers[i].field_count);
        for (int j = 0; j < shader1->storage_buffers[i].field_count; j++)
        {
            KASSERT(StringEqual(shader1->storage_buffers[i].fields[j].name, shader2->storage_buffers[i].fields[j].name));
            KASSERT(shader1->storage_buffers[i].fields[j].type == shader2->storage_buffers[i].fields[j].type);
        }
    }

    KASSERT(shader1->vertex_layout_count == shader2->vertex_layout_count);
    for (int i = 0; i < shader1->vertex_layout_count; i++)
    {
        KASSERT(StringEqual(shader1->vertex_layouts[i].name, shader2->vertex_layouts[i].name));
        KASSERT(shader1->vertex_layouts[i].attribute_count == shader2->vertex_layouts[i].attribute_count);
        KASSERT(shader1->vertex_layouts[i].input_binding_count == shader2->vertex_layouts[i].input_binding_count);

        for (int j = 0; j < shader1->vertex_layouts[i].attribute_count; j++)
        {
            KASSERT(shader1->vertex_layouts[i].attributes[j].location == shader2->vertex_layouts[i].attributes[j].location);
            KASSERT(shader1->vertex_layouts[i].attributes[j].binding == shader2->vertex_layouts[i].attributes[j].binding);
            KASSERT(shader1->vertex_layouts[i].attributes[j].offset == shader2->vertex_layouts[i].attributes[j].offset);
            KASSERT(shader1->vertex_layouts[i].attributes[j].format == shader2->vertex_layouts[i].attributes[j].format);
        }

        for (int j = 0; j < shader1->vertex_layouts[i].input_binding_count; j++)
        {
            KASSERT(shader1->vertex_layouts[i].input_bindings[j].binding == shader2->vertex_layouts[i].input_bindings[j].binding);
            KASSERT(shader1->vertex_layouts[i].input_bindings[j].stride == shader2->vertex_layouts[i].input_bindings[j].stride);
            KASSERT(shader1->vertex_layouts[i].input_bindings[j].input_rate == shader2->vertex_layouts[i].input_bindings[j].input_rate);
        }
    }

    KASSERT(StringEqual(shader1->render_state.name, shader2->render_state.name));
    KASSERT(shader1->render_state.cull_mode == shader2->render_state.cull_mode);
    KASSERT(shader1->render_state.z_test_op == shader2->render_state.z_test_op);
    KASSERT(shader1->render_state.z_write_enable == shader2->render_state.z_write_enable);
    KASSERT(shader1->render_state.blend_enable == shader2->render_state.blend_enable);
    KASSERT(shader1->render_state.blend_mode.src_color_blend_factor == shader2->render_state.blend_mode.src_color_blend_factor);
    KASSERT(shader1->render_state.blend_mode.dst_color_blend_factor == shader2->render_state.blend_mode.dst_color_blend_factor);
    KASSERT(shader1->render_state.blend_mode.color_blend_op == shader2->render_state.blend_mode.color_blend_op);
    KASSERT(shader1->render_state.blend_mode.src_alpha_blend_factor == shader2->render_state.blend_mode.src_alpha_blend_factor);
    KASSERT(shader1->render_state.blend_mode.dst_alpha_blend_factor == shader2->render_state.blend_mode.dst_alpha_blend_factor);
    KASSERT(shader1->render_state.blend_mode.alpha_blend_op == shader2->render_state.blend_mode.alpha_blend_op);

    KASSERT(shader1->render_pass_def.shader_stage_count == shader2->render_pass_def.shader_stage_count);
    for (int j = 0; j < shader1->render_pass_def.shader_stage_count; j++)
    {
        KASSERT(shader1->render_pass_def.shader_stages[j].stage == shader2->render_pass_def.shader_stages[j].stage);
        KASSERT(StringEqual(shader1->render_pass_def.shader_stages[j].code_fragment.name, shader2->render_pass_def.shader_stages[j].code_fragment.name));
    }

    return true;
}

} // namespace kraft::shaderfx
