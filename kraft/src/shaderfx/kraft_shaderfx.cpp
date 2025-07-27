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

#include <core/kraft_lexer_types.h>
#include <platform/kraft_filesystem_types.h>
#include <renderer/kraft_renderer_types.h>
#include <shaderfx/kraft_shaderfx_types.h>

#define PARSER_ERROR_NEXT_TOKEN_READ_FAILED "Error while reading the next token %d"
#define PARSER_ERROR_TOKEN_MISMATCH         "Token mismatch\nExpected '%s' got '%s'"
#define PARSER_ERROR_TOKEN_KEYWORD_MISMATCH "Keyword mismatch\nExpected '%s' got '%s'"
#define PARSER_ERROR_INVALID_SHADER_STAGE   "Invalid shader stage '%s'"

namespace kraft::shaderfx {

int GetShaderStageFromString(StringView Value)
{
    if (Value == "Vertex")
    {
        return renderer::SHADER_STAGE_FLAGS_VERTEX;
    }
    else if (Value == "Fragment")
    {
        return renderer::SHADER_STAGE_FLAGS_FRAGMENT;
    }
    else if (Value == "Compute")
    {
        return renderer::SHADER_STAGE_FLAGS_COMPUTE;
    }
    else if (Value == "Geometry")
    {
        return renderer::SHADER_STAGE_FLAGS_GEOMETRY;
    }

    return 0;
}

void ShaderFXParser::SetError(const char* format, ...)
{
#ifdef KRAFT_COMPILER_MSVC
    va_list args;
#else
    __builtin_va_list args;
#endif
    va_start(args, format);
    StringFormatV(this->ErrorString, KRAFT_C_ARRAY_SIZE(this->ErrorString), format, args);
    va_end(args);

    this->ErrorLine = this->Lexer->Line;
    this->ErrorColumn = this->Lexer->Column;
    this->ErroredOut = true;
    this->ErrorCode = ErrorCode;
}

bool ShaderFXParser::Parse(const String& SourceFilePath, kraft::Lexer* Lexer, ShaderEffect* Out)
{
    KASSERT(Lexer);
    this->Lexer = Lexer;
    Out->ResourcePath = SourceFilePath;

    return this->GenerateAST(Out);
}

bool ShaderFXParser::GenerateAST(ShaderEffect* Effect)
{
    while (true)
    {
        LexerToken Token;
        if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
        {
            this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
            return false;
        }

        if (Token.Type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            if (!this->ParseIdentifier(Effect, &Token))
            {
                return false;
            }
        }
        else if (Token.Type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }

    return true;
}

bool ShaderFXParser::ParseIdentifier(ShaderEffect* Effect, const LexerToken* Token)
{
    char Char = Token->Text[0];
    switch (Char)
    {
        case 'S':
        {
            if (Token->MatchesKeyword("Shader"))
            {
                return this->ParseShaderDeclaration(Effect);
            }
        }
        break;
        case 'L':
        {
            if (Token->MatchesKeyword("Layout"))
            {
                return this->ParseLayoutBlock(Effect);
            }
        }
        break;
        case 'G':
        {
            if (Token->MatchesKeyword("GLSL"))
            {
                return this->ParseGLSLBlock(Effect);
            }
        }
        break;
        case 'R':
        {
            if (Token->MatchesKeyword("RenderState"))
            {
                return this->ParseRenderStateBlock(Effect);
            }
        }
        break;
        case 'P':
        {
            if (Token->MatchesKeyword("Pass"))
            {
                return this->ParseRenderPassBlock(Effect);
            }
        }
        break;
    }

    return true;
}

bool ShaderFXParser::ParseShaderDeclaration(ShaderEffect* Effect)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
        return false;
    }

    Effect->Name = String(Token.Text, Token.Length);
    return true;
}

bool ShaderFXParser::ParseLayoutBlock(ShaderEffect* Effect)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        if (Token.MatchesKeyword("Vertex"))
        {
            // Consume "Vertex"
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            VertexLayoutDefinition Layout;
            Layout.Name = String(Token.Text, Token.Length);

            if (!this->ParseVertexLayout(&Layout))
            {
                return false;
            }

            Effect->VertexLayouts.Push(Layout);
        }
        // Local/Global resources
        else if (Token.MatchesKeyword("local") || Token.MatchesKeyword("global"))
        {
            ResourceBindingType::Enum Type = Token.MatchesKeyword("global") ? ResourceBindingType::Global : ResourceBindingType::Local;

            // Consume 'local'/'global' and move to the next token
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            if (!Token.MatchesKeyword("Resource"))
            {
                this->SetError(PARSER_ERROR_TOKEN_KEYWORD_MISMATCH, "Resource", *Token.ToString());
                return false;
            }

            // Consume "Resource" and move to the next token
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            ResourceBindingsDefinition Bindings;
            Bindings.Name = String(Token.Text, Token.Length);
            if (!this->ParseResourceBindings(&Bindings))
            {
                return false;
            }

            if (Type == ResourceBindingType::Local)
            {
                Effect->LocalResources.Push(Bindings);
            }
            else
            {
                Effect->GlobalResources.Push(Bindings);
            }
        }
        else if (Token.MatchesKeyword("ConstantBuffer"))
        {
            // Consume "ConstantBuffer"
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            ConstantBufferDefinition CBufferDefinition;
            CBufferDefinition.Name = String(Token.Text, Token.Length);

            if (!this->ParseConstantBuffer(&CBufferDefinition))
            {
                return false;
            }

            Effect->ConstantBuffers.Push(CBufferDefinition);
        }
        else if (Token.MatchesKeyword("UniformBuffer"))
        {
            // Consume "UniformBuffer"
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            UniformBufferDefinition UBufferDefinition;
            UBufferDefinition.Name = String(Token.Text, Token.Length);

            if (!this->ParseUniformBuffer(&UBufferDefinition))
            {
                return false;
            }

            Effect->UniformBuffers.Push(UBufferDefinition);
        }
        else if (Token.MatchesKeyword("StorageBuffer"))
        {
            // Consume "StorageBuffer"
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            UniformBufferDefinition UBufferDefinition;
            UBufferDefinition.Name = String(Token.Text, Token.Length);

            if (!this->ParseUniformBuffer(&UBufferDefinition))
            {
                return false;
            }

            Effect->StorageBuffers.Push(UBufferDefinition);
        }
    }

    return true;
}

bool ShaderFXParser::ParseResourceBindings(ResourceBindingsDefinition* ResourceBindings)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        ResourceBinding Binding;
        if (Token.MatchesKeyword("UniformBuffer"))
        {
            Binding.Type = renderer::ResourceType::UniformBuffer;
        }
        else if (Token.MatchesKeyword("StorageBuffer"))
        {
            Binding.Type = renderer::ResourceType::StorageBuffer;
        }
        else if (Token.MatchesKeyword("Sampler"))
        {
            Binding.Type = renderer::ResourceType::Sampler;
        }
        else
        {
            this->SetError("Invalid binding type '%s'", *Token.ToString());
            return false;
        }

        // Parse the name of the binding
        if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
        {
            this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
            return false;
        }

        Binding.Name = String(Token.Text, Token.Length);

        while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            LexerNamedToken Pair;
            if (!this->ParseNamedToken(&Token, &Pair))
            {
                return false;
            }

            if (Pair.Key == "Stage")
            {
                Binding.Stage = (renderer::ShaderStageFlags)GetShaderStageFromString(Pair.Value.ToStringView());
                if (Binding.Stage == 0)
                {
                    this->SetError(PARSER_ERROR_INVALID_SHADER_STAGE, *Pair.Value.ToString());
                    return false;
                }
            }
            else if (Pair.Key == "Binding")
            {
                Binding.Binding = (uint16)Pair.Value.FloatValue;
            }
            else if (Pair.Key == "Size")
            {
                Binding.Size = (uint16)Pair.Value.FloatValue;
            }
            else if (Pair.Key == "Set")
            {
                Binding.Set = (uint16)Pair.Value.FloatValue;
            }
            else
            {
                KWARN("Found unknown named key: '%s'", *String(Pair.Key.Buffer, Pair.Key.Length));
            }
        }

        ResourceBindings->ResourceBindings.Push(Binding);
    }

    return true;
}

bool ShaderFXParser::ParseConstantBuffer(ConstantBufferDefinition* CBufferDefinition)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        ConstantBufferEntry Entry;

        // Parse the data type
        if (!ParseDataType(&Token, &Entry.Type))
        {
            this->SetError("Invalid data type for constant buffer entry '%s'", *Token.ToString());
            return false;
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        Entry.Name = Token.ToString();

        // Parse properties
        while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            LexerNamedToken Pair;
            if (this->ParseNamedToken(&Token, &Pair))
            {
                if (Pair.Key == "Stage")
                {
                    Entry.Stage = (renderer::ShaderStageFlags)GetShaderStageFromString(Pair.Value.ToStringView());
                    if (Entry.Stage == 0)
                    {
                        this->SetError(PARSER_ERROR_INVALID_SHADER_STAGE, *Pair.Value.ToString());
                        return false;
                    }
                }
            }
            else
            {
                return false;
            }
        }

        CBufferDefinition->Fields.Push(Entry);
    }

    return true;
}

bool ShaderFXParser::ParseUniformBuffer(UniformBufferDefinition* UBufferDefinition)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        UniformBufferEntry Entry;

        // Parse the data type
        if (!ParseDataType(&Token, &Entry.Type))
        {
            this->SetError("Invalid data type for constant buffer entry '%s'", *Token.ToString());
            return false;
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        Entry.Name = Token.ToString();
        UBufferDefinition->Fields.Push(Entry);

        if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_SEMICOLON), TokenType::String(Token.Type));
            return false;
        }
    }

    return true;
}

#define MATCH_FORMAT(type, value)                                                                                                                                                                      \
    if (CurrentToken->MatchesKeyword(type))                                                                                                                                                            \
    {                                                                                                                                                                                                  \
        OutType->UnderlyingType = value;                                                                                                                                                               \
    }

bool ShaderFXParser::ParseDataType(const LexerToken* CurrentToken, renderer::ShaderDataType* OutType)
{
    char Char = CurrentToken->Text[0];
    OutType->UnderlyingType = renderer::ShaderDataType::Count;
    switch (Char)
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
    if (OutType->UnderlyingType == renderer::ShaderDataType::Count)
    {
        this->SetError("Invalid data type '%s'", *CurrentToken->ToString());
        return false;
    }

    // Is this an array type?
    uint64     CurrentCursorPosition = Lexer->Position;
    LexerToken NextToken;
    if (LexerError ErrorCode = Lexer->NextToken(&NextToken))
    {
        this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    // Array type?
    if (NextToken.Type == TokenType::TOKEN_TYPE_OPEN_BRACKET)
    {
        if (LexerError ErrorCode = Lexer->NextToken(&NextToken))
        {
            this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
            return false;
        }

        if (NextToken.Type == TokenType::TOKEN_TYPE_NUMBER)
        {
            if (NextToken.FloatValue < 0.0f)
            {
                this->SetError("Array size cannot be negative");
                return false;
            }

            OutType->ArraySize = (uint16)NextToken.FloatValue;

            // Move forward
            if (LexerError ErrorCode = Lexer->NextToken(&NextToken))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }
        }

        if (NextToken.Type != TokenType::TOKEN_TYPE_CLOSE_BRACKET)
        {
            this->SetError("Array not terminated");
            return false;
        }
    }
    else
    {
        // Go back to the original position
        Lexer->MoveCursorTo(CurrentCursorPosition);
    }

    return true;
}

#undef MATCH_FORMAT

bool ShaderFXParser::ParseNamedToken(const LexerToken* CurrentToken, LexerNamedToken* OutToken)
{
    if (CurrentToken->Type != TokenType::TOKEN_TYPE_IDENTIFIER)
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(CurrentToken->Type));
        return false;
    }

    OutToken->Key = StringView(CurrentToken->Text, CurrentToken->Length);
    LexerToken NextToken;
    if (!this->Lexer->ExpectToken(&NextToken, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_PARENTHESIS), TokenType::String(CurrentToken->Type));
        return false;
    }

    // Skip past the parenthesis
    if (LexerError ErrorCode = this->Lexer->NextToken(&NextToken))
    {
        this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    OutToken->Value = NextToken;
    if (!this->Lexer->ExpectToken(&NextToken, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_PARENTHESIS), TokenType::String(CurrentToken->Type));
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseVertexLayout(VertexLayoutDefinition* Layout)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        if (Token.MatchesKeyword("Attribute"))
        {
            if (!this->ParseVertexAttribute(Layout))
            {
                return false;
            }
        }
        else if (Token.MatchesKeyword("Binding"))
        {
            if (!this->ParseVertexInputBinding(Layout))
            {
                return false;
            }
        }
    }

    return true;
}

bool ShaderFXParser::ParseVertexAttribute(VertexLayoutDefinition* Layout)
{
    LexerToken      CurrentToken;
    VertexAttribute Attribute;
    Attribute.Format = renderer::ShaderDataType::Invalid();

    if (!this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(CurrentToken.Type));
        return false;
    }

    // Format identifier
    if (!ParseDataType(&CurrentToken, &Attribute.Format))
    {
        return false;
    }

    if (!this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(CurrentToken.Type));
        return false;
    }

    // Attribute name
    // Skip
    // Attribute.Name = String(Token, Token.Length);

    // Binding
    if (this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Binding = (uint16)CurrentToken.FloatValue;
    }
    else
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(CurrentToken.Type));
        return false;
    }

    // Location
    if (this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Location = (uint16)CurrentToken.FloatValue;
    }
    else
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(CurrentToken.Type));
        return false;
    }

    // Offset
    if (this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Offset = (uint16)CurrentToken.FloatValue;
    }
    else
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(CurrentToken.Type));
        return false;
    }

    Layout->Attributes.Push(Attribute);
    return true;
}

bool ShaderFXParser::ParseVertexInputBinding(VertexLayoutDefinition* Layout)
{
    LexerToken         Token;
    VertexInputBinding InputBinding;

    // Binding
    if (this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        InputBinding.Binding = (uint16)Token.FloatValue;
    }
    else
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(Token.Type));
        return false;
    }

    // Stride
    if (this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        InputBinding.Stride = (uint16)Token.FloatValue;
    }
    else
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(Token.Type));
        return false;
    }

    // Input Rate
    if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
    {
        this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    if (Token.MatchesKeyword("vertex"))
    {
        InputBinding.InputRate = renderer::VertexInputRate::PerVertex;
    }
    else if (Token.MatchesKeyword("instance"))
    {
        InputBinding.InputRate = renderer::VertexInputRate::PerInstance;
    }
    else
    {
        this->SetError("Invalid value for VertexInputRate: '%s'", *Token.ToString());
        return false;
    }

    Layout->InputBindings.Push(InputBinding);
    return true;
}

bool ShaderFXParser::ParseGLSLBlock(ShaderEffect* Effect)
{
    LexerToken Token;
    if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
    {
        this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    ShaderCodeFragment CodeFragment;
    CodeFragment.Name = String(Token.Text, Token.Length);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
    {
        this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    char* ShaderCodeStart = Token.Text;

    int OpenBraceCount = 1;
    while (OpenBraceCount)
    {
        if (Token.Type == TokenType::TOKEN_TYPE_OPEN_BRACE)
        {
            OpenBraceCount++;
        }
        else if (Token.Type == TokenType::TOKEN_TYPE_CLOSE_BRACE)
        {
            OpenBraceCount--;
        }

        if (OpenBraceCount)
        {
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                if (ErrorCode == LEXER_ERROR_UNEXPECTED_CHARACTER && Token.Type == TokenType::TOKEN_TYPE_UNKNOWN)
                {
                    continue;
                }

                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }
        }
    }

    uint64 ShaderCodeLength = Token.Text - ShaderCodeStart;
    CodeFragment.Code = String(ShaderCodeStart, ShaderCodeLength);
    Effect->CodeFragments.Push(CodeFragment);
    if (this->Verbose)
    {
        KDEBUG("%s", *CodeFragment.Code);
    }

    return true;
}

bool ShaderFXParser::ParseRenderStateBlock(ShaderEffect* Effect)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    // Parse all the render states
    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        // Expect "State"
        if (Token.MatchesKeyword("State"))
        {
            // Move past "State"
            if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            RenderStateDefinition State;
            State.Name = String(Token.Text, Token.Length);

            this->ParseRenderState(&State);
            Effect->RenderStates.Push(State);
        }
    }

    return true;
}

bool ShaderFXParser::ParseRenderState(RenderStateDefinition* State)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        if (Token.MatchesKeyword("Cull"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
                return false;
            }

            if (Token.MatchesKeyword("Back"))
            {
                State->CullMode = renderer::CullModeFlags::Back;
            }
            else if (Token.MatchesKeyword("Front"))
            {
                State->CullMode = renderer::CullModeFlags::Front;
            }
            else if (Token.MatchesKeyword("FrontAndBack"))
            {
                State->CullMode = renderer::CullModeFlags::FrontAndBack;
            }
            else if (Token.MatchesKeyword("Off") || Token.MatchesKeyword("None"))
            {
                State->CullMode = renderer::CullModeFlags::None;
            }
            else
            {
                this->SetError("Invalid value for CullMode: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("ZTest"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            bool Valid = false;
            for (int i = 0; i < renderer::CompareOp::Count; i++)
            {
                if (Token.MatchesKeyword(renderer::CompareOp::Strings[i]))
                {
                    State->ZTestOperation = (renderer::CompareOp::Enum)i;
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->SetError("Invalid value for ZTest: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("ZWrite"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            if (Token.MatchesKeyword("On"))
            {
                State->ZWriteEnable = true;
            }
            else if (Token.MatchesKeyword("Off"))
            {
                State->ZWriteEnable = false;
            }
            else
            {
                this->SetError("Invalid value for ZWrite: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("Blend"))
        {
            // Lot of tokens to parse here

            // SrcColor
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            // Blending turned off
            if (Token.MatchesKeyword("Off") || Token.MatchesKeyword("None"))
            {
                State->BlendEnable = false;
                continue;
            }

            State->BlendEnable = true;
            if (!this->ParseBlendFactor(&Token, State->BlendMode.SrcColorBlendFactor))
            {
                return false;
            }

            // DstColor
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            if (!this->ParseBlendFactor(&Token, State->BlendMode.DstColorBlendFactor))
            {
                return false;
            }

            // Comma
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_COMMA))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_COMMA), TokenType::String(Token.Type));
                return false;
            }

            // SrcAlpha
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            if (!this->ParseBlendFactor(&Token, State->BlendMode.SrcAlphaBlendFactor))
            {
                return false;
            }

            // DstAlpha
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            if (!this->ParseBlendFactor(&Token, State->BlendMode.DstAlphaBlendFactor))
            {
                return false;
            }
        }
        else if (Token.MatchesKeyword("BlendOp"))
        {
            // ColorBlendOp
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            if (!this->ParseBlendOp(&Token, State->BlendMode.ColorBlendOperation))
            {
                return false;
            }

            // Comma
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_COMMA))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_COMMA), TokenType::String(Token.Type));
                return false;
            }

            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            // AlphaBlendOp
            if (!this->ParseBlendOp(&Token, State->BlendMode.AlphaBlendOperation))
            {
                return false;
            }
        }
        else if (Token.MatchesKeyword("PolygonMode"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            for (int i = 0; i < renderer::PolygonMode::Count; i++)
            {
                if (Token.ToString() == renderer::PolygonMode::Strings[i])
                {
                    State->PolygonMode = (renderer::PolygonMode::Enum)i;
                    break;
                }
            }
        }
        else if (Token.MatchesKeyword("LineWidth"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_NUMBER), TokenType::String(Token.Type));
                return false;
            }

            State->LineWidth = (float32)Token.FloatValue;
        }
    }

    return true;
}

bool ShaderFXParser::ParseBlendFactor(const LexerToken* Token, renderer::BlendFactor::Enum& Factor)
{
    bool Valid = false;
    for (int i = 0; i < renderer::BlendFactor::Count; i++)
    {
        if (Token->MatchesKeyword(renderer::BlendFactor::Strings[i]))
        {
            Factor = (renderer::BlendFactor::Enum)i;
            Valid = true;
            break;
        }
    }

    if (!Valid)
    {
        this->SetError("Invalid value for BlendFactor: '%s'", *Token->ToString());
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseBlendOp(const LexerToken* Token, renderer::BlendOp::Enum& Op)
{
    bool Valid = false;
    for (int i = 0; i < renderer::BlendOp::Count; i++)
    {
        if (Token->MatchesKeyword(renderer::BlendOp::Strings[i]))
        {
            Op = (renderer::BlendOp::Enum)i;
            Valid = true;
            break;
        }
    }

    if (!Valid)
    {
        this->SetError("Invalid value for BlendOp: '%s'", *Token->ToString());
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseRenderPassBlock(ShaderEffect* Effect)
{
    // Consume the name of the pass
    LexerToken Token;
    if (LexerError ErrorCode = this->Lexer->NextToken(&Token))
    {
        this->SetError(PARSER_ERROR_NEXT_TOKEN_READ_FAILED, ErrorCode);
        return false;
    }

    RenderPassDefinition Pass;
    Pass.Name = String(Token.Text, Token.Length);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_OPEN_BRACE), TokenType::String(Token.Type));
        return false;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
            return false;
        }

        if (Token.MatchesKeyword("RenderState"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            StringView Name = Token.ToStringView();
            bool       Valid = false;
            for (int i = 0; i < Effect->RenderStates.Length; i++)
            {
                if (Effect->RenderStates[i].Name == Name)
                {
                    Pass.RenderState = &Effect->RenderStates[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->SetError("Unknown RenderState: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("VertexLayout"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            StringView Name = Token.ToStringView();
            bool       Valid = false;
            for (int i = 0; i < Effect->VertexLayouts.Length; i++)
            {
                if (Effect->VertexLayouts[i].Name == Name)
                {
                    Pass.VertexLayout = &Effect->VertexLayouts[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->SetError("Unknown VertexLayout: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("Resources"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            // It is possible for shaders to have no local resources
            if (Effect->LocalResources.Length == 0)
                continue;

            StringView Name = Token.ToStringView();
            bool       Valid = false;
            for (int i = 0; i < Effect->LocalResources.Length; i++)
            {
                if (Effect->LocalResources[i].Name == Name)
                {
                    Pass.Resources = &Effect->LocalResources[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->SetError("Unknown Resource: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("ConstantBuffer"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
                return false;
            }

            StringView Name = Token.ToStringView();
            bool       Valid = false;
            for (int i = 0; i < Effect->ConstantBuffers.Length; i++)
            {
                if (Effect->ConstantBuffers[i].Name == Name)
                {
                    Pass.ConstantBuffers = &Effect->ConstantBuffers[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->SetError("Unknown ConstantBuffer: '%s'", *Token.ToString());
                return false;
            }
        }
        else if (Token.MatchesKeyword("VertexShader"))
        {
            if (!this->ParseRenderPassShaderStage(Effect, &Pass, renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX))
            {
                return false;
            }
        }
        else if (Token.MatchesKeyword("FragmentShader"))
        {
            if (!this->ParseRenderPassShaderStage(Effect, &Pass, renderer::ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT))
            {
                return false;
            }
        }
    }

    Effect->RenderPasses.Push(Pass);

    return true;
}

bool ShaderFXParser::ParseRenderPassShaderStage(ShaderEffect* Effect, RenderPassDefinition* Pass, renderer::ShaderStageFlags ShaderStage)
{
    LexerToken Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        this->SetError(PARSER_ERROR_TOKEN_MISMATCH, TokenType::String(TokenType::TOKEN_TYPE_IDENTIFIER), TokenType::String(Token.Type));
        return false;
    }

    for (int i = 0; i < Effect->CodeFragments.Length; i++)
    {
        if (Token.MatchesKeyword(Effect->CodeFragments[i].Name))
        {
            RenderPassDefinition::ShaderDefinition Stage;
            Stage.Stage = ShaderStage;
            Stage.CodeFragment = Effect->CodeFragments[i];
            Pass->ShaderStages.Push(Stage);

            return true;
        }
    }

    this->SetError("Unknown Shader: '%s'", *Token.ToString());
    return false;
}

bool LoadShaderFX(const String& Path, ShaderEffect* Shader)
{
    filesystem::FileHandle File;

    // Read test
    if (!filesystem::OpenFile(Path, filesystem::FILE_OPEN_MODE_READ, true, &File))
    {
        KERROR("Failed to read file %s", *Path);
        return false;
    }

    uint64 BinaryBufferSize = kraft::filesystem::GetFileSize(&File) + 1;
    uint8* BinaryBuffer = (uint8*)kraft::Malloc(BinaryBufferSize, MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&File, &BinaryBuffer);
    filesystem::CloseFile(&File);

    kraft::Buffer Reader((char*)BinaryBuffer, BinaryBufferSize);
    Reader.Read(&Shader->Name);
    Reader.Read(&Shader->ResourcePath);
    Reader.Read(&Shader->VertexLayouts);
    Reader.Read(&Shader->LocalResources);
    Reader.Read(&Shader->GlobalResources);
    Reader.Read(&Shader->ConstantBuffers);
    Reader.Read(&Shader->UniformBuffers);
    Reader.Read(&Shader->StorageBuffers);
    Reader.Read(&Shader->RenderStates);

    uint64 RenderPassesCount;
    Reader.Read(&RenderPassesCount);

    Shader->RenderPasses = Array<RenderPassDefinition>(RenderPassesCount);
    for (int i = 0; i < RenderPassesCount; i++)
    {
        RenderPassDefinition& Pass = Shader->RenderPasses[i];
        Reader.Read(&Pass.Name);

        int64 VertexLayoutOffset;
        Reader.Read(&VertexLayoutOffset);
        Pass.VertexLayout = &Shader->VertexLayouts[VertexLayoutOffset];

        int64 ResourcesOffset;
        Reader.Read(&ResourcesOffset);
        Pass.Resources = ResourcesOffset > -1 ? &Shader->LocalResources[ResourcesOffset] : nullptr;

        int64 ConstantBuffersOffset;
        Reader.Read(&ConstantBuffersOffset);
        Pass.ConstantBuffers = &Shader->ConstantBuffers[ConstantBuffersOffset];

        int64 RenderStateOffset;
        Reader.Read(&RenderStateOffset);
        Pass.RenderState = &Shader->RenderStates[RenderStateOffset];

        uint64 ShaderStagesCount;
        Reader.Read(&ShaderStagesCount);
        Pass.ShaderStages = Array<RenderPassDefinition::ShaderDefinition>(ShaderStagesCount);
        for (int j = 0; j < ShaderStagesCount; j++)
        {
            RenderPassDefinition::ShaderDefinition& Stage = Pass.ShaderStages[j];
            Reader.Read(&Stage.Stage);
            Reader.Read(&Stage.CodeFragment);
        }
    }

    kraft::Free(BinaryBuffer, BinaryBufferSize, MEMORY_TAG_FILE_BUF);

    return true;
}

bool ValidateShaderFX(const ShaderEffect& ShaderA, ShaderEffect& ShaderB)
{
    KASSERT(ShaderA.Name == ShaderB.Name);
    KASSERT(ShaderA.ResourcePath == ShaderB.ResourcePath);
    KASSERT(ShaderA.VertexLayouts.Length == ShaderB.VertexLayouts.Length);

    // Buffers
    KASSERT(ShaderA.UniformBuffers.Length == ShaderB.UniformBuffers.Length);
    for (int i = 0; i < ShaderA.UniformBuffers.Length; i++)
    {
        KASSERT(ShaderA.UniformBuffers[i].Name == ShaderB.UniformBuffers[i].Name);
        KASSERT(ShaderA.UniformBuffers[i].Fields.Length == ShaderB.UniformBuffers[i].Fields.Length);
        for (int j = 0; j < ShaderA.UniformBuffers[i].Fields.Length; j++)
        {
            KASSERT(ShaderA.UniformBuffers[i].Fields[j].Name == ShaderB.UniformBuffers[i].Fields[j].Name);
            KASSERT(ShaderA.UniformBuffers[i].Fields[j].Type == ShaderB.UniformBuffers[i].Fields[j].Type);
        }
    }

    KASSERT(ShaderA.StorageBuffers.Length == ShaderB.StorageBuffers.Length);
    for (int i = 0; i < ShaderA.StorageBuffers.Length; i++)
    {
        KASSERT(ShaderA.StorageBuffers[i].Name == ShaderB.StorageBuffers[i].Name);
        KASSERT(ShaderA.StorageBuffers[i].Fields.Length == ShaderB.StorageBuffers[i].Fields.Length);
        for (int j = 0; j < ShaderA.StorageBuffers[i].Fields.Length; j++)
        {
            KASSERT(ShaderA.StorageBuffers[i].Fields[j].Name == ShaderB.StorageBuffers[i].Fields[j].Name);
            KASSERT(ShaderA.StorageBuffers[i].Fields[j].Type == ShaderB.StorageBuffers[i].Fields[j].Type);
        }
    }

    for (int i = 0; i < ShaderA.VertexLayouts.Length; i++)
    {
        KASSERT(ShaderA.VertexLayouts[i].Name == ShaderB.VertexLayouts[i].Name);
        KASSERT(ShaderA.VertexLayouts[i].Attributes.Length == ShaderB.VertexLayouts[i].Attributes.Length);
        KASSERT(ShaderA.VertexLayouts[i].InputBindings.Length == ShaderB.VertexLayouts[i].InputBindings.Length);

        for (int j = 0; j < ShaderA.VertexLayouts[i].Attributes.Length; j++)
        {
            KASSERT(ShaderA.VertexLayouts[i].Attributes[j].Location == ShaderB.VertexLayouts[i].Attributes[j].Location);
            KASSERT(ShaderA.VertexLayouts[i].Attributes[j].Binding == ShaderB.VertexLayouts[i].Attributes[j].Binding);
            KASSERT(ShaderA.VertexLayouts[i].Attributes[j].Offset == ShaderB.VertexLayouts[i].Attributes[j].Offset);
            KASSERT(ShaderA.VertexLayouts[i].Attributes[j].Format == ShaderB.VertexLayouts[i].Attributes[j].Format);
        }

        for (int j = 0; j < ShaderA.VertexLayouts[i].InputBindings.Length; j++)
        {
            KASSERT(ShaderA.VertexLayouts[i].InputBindings[j].Binding == ShaderB.VertexLayouts[i].InputBindings[j].Binding);
            KASSERT(ShaderA.VertexLayouts[i].InputBindings[j].Stride == ShaderB.VertexLayouts[i].InputBindings[j].Stride);
            KASSERT(ShaderA.VertexLayouts[i].InputBindings[j].InputRate == ShaderB.VertexLayouts[i].InputBindings[j].InputRate);
        }
    }

    KASSERT(ShaderA.RenderStates.Length == ShaderB.RenderStates.Length);
    for (int i = 0; i < ShaderA.RenderStates.Length; i++)
    {
        KASSERT(ShaderA.RenderStates[i].Name == ShaderB.RenderStates[i].Name);
        KASSERT(ShaderA.RenderStates[i].CullMode == ShaderB.RenderStates[i].CullMode);
        KASSERT(ShaderA.RenderStates[i].ZTestOperation == ShaderB.RenderStates[i].ZTestOperation);
        KASSERT(ShaderA.RenderStates[i].ZWriteEnable == ShaderB.RenderStates[i].ZWriteEnable);
        KASSERT(ShaderA.RenderStates[i].BlendEnable == ShaderB.RenderStates[i].BlendEnable);
        KASSERT(ShaderA.RenderStates[i].BlendMode.SrcColorBlendFactor == ShaderB.RenderStates[i].BlendMode.SrcColorBlendFactor);
        KASSERT(ShaderA.RenderStates[i].BlendMode.DstColorBlendFactor == ShaderB.RenderStates[i].BlendMode.DstColorBlendFactor);
        KASSERT(ShaderA.RenderStates[i].BlendMode.ColorBlendOperation == ShaderB.RenderStates[i].BlendMode.ColorBlendOperation);
        KASSERT(ShaderA.RenderStates[i].BlendMode.SrcAlphaBlendFactor == ShaderB.RenderStates[i].BlendMode.SrcAlphaBlendFactor);
        KASSERT(ShaderA.RenderStates[i].BlendMode.DstAlphaBlendFactor == ShaderB.RenderStates[i].BlendMode.DstAlphaBlendFactor);
        KASSERT(ShaderA.RenderStates[i].BlendMode.AlphaBlendOperation == ShaderB.RenderStates[i].BlendMode.AlphaBlendOperation);
    }

    KASSERT(ShaderA.RenderPasses.Length == ShaderB.RenderPasses.Length);
    for (int i = 0; i < ShaderA.RenderPasses.Length; i++)
    {
        KASSERT(ShaderA.RenderPasses[i].ShaderStages.Length == ShaderB.RenderPasses[i].ShaderStages.Length);
        for (int j = 0; j < ShaderA.RenderPasses[i].ShaderStages.Length; j++)
        {
            KASSERT(ShaderA.RenderPasses[i].ShaderStages[j].Stage == ShaderB.RenderPasses[i].ShaderStages[j].Stage);
            KASSERT(ShaderA.RenderPasses[i].ShaderStages[j].CodeFragment.Name == ShaderB.RenderPasses[i].ShaderStages[j].CodeFragment.Name);
        }
    }

    return true;
}

} // namespace kraft::shaderfx
