#include "kraft_shaderfx.h"

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
#include <renderer/shaderfx/kraft_shaderfx_types.h>

#include <shaderc/shaderc.h>

#define MARK_ERROR(error)                                                                                                                                                                              \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        this->Error = true;                                                                                                                                                                            \
        this->ErrorLine = this->Lexer->Line;                                                                                                                                                           \
        StringCopy(this->ErrorString, error);                                                                                                                                                          \
    } while (0)

#define MARK_ERROR_WITH_FIELD(error, field)                                                                                                                                                            \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        this->Error = true;                                                                                                                                                                            \
        this->ErrorLine = this->Lexer->Line;                                                                                                                                                           \
        StringCopy(this->ErrorString, error);                                                                                                                                                          \
        StringConcat(this->ErrorString, " '");                                                                                                                                                         \
        StringConcat(this->ErrorString, field);                                                                                                                                                        \
        StringConcat(this->ErrorString, "'");                                                                                                                                                          \
    } while (0)

#define MARK_ERROR_RETURN(error)                                                                                                                                                                       \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        this->Error = true;                                                                                                                                                                            \
        this->ErrorLine = this->Lexer->Line;                                                                                                                                                           \
        StringCopy(this->ErrorString, error);                                                                                                                                                          \
        return;                                                                                                                                                                                        \
    } while (0)

#define MARK_ERROR_WITH_FIELD_RETURN(error, field)                                                                                                                                                     \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        this->Error = true;                                                                                                                                                                            \
        this->ErrorLine = this->Lexer->Line;                                                                                                                                                           \
        StringCopy(this->ErrorString, error);                                                                                                                                                          \
        StringConcat(this->ErrorString, " '");                                                                                                                                                         \
        StringConcat(this->ErrorString, field);                                                                                                                                                        \
        StringConcat(this->ErrorString, "'");                                                                                                                                                          \
        return;                                                                                                                                                                                        \
    } while (0)

namespace kraft::renderer {

int GetShaderStageFromString(StringView Value)
{
    if (Value == "Vertex")
    {
        return SHADER_STAGE_FLAGS_VERTEX;
    }
    else if (Value == "Fragment")
    {
        return SHADER_STAGE_FLAGS_FRAGMENT;
    }
    else if (Value == "Compute")
    {
        return SHADER_STAGE_FLAGS_COMPUTE;
    }
    else if (Value == "Geometry")
    {
        return SHADER_STAGE_FLAGS_GEOMETRY;
    }

    return 0;
}

ShaderEffect ShaderFXParser::Parse(const String& SourceFilePath, kraft::Lexer* Lexer)
{
    KASSERT(Lexer);

    ShaderEffect Effect = {};
    this->Lexer = Lexer;
    Effect.ResourcePath = SourceFilePath;

    this->GenerateAST(&Effect);

    return Effect;
}

void ShaderFXParser::GenerateAST(ShaderEffect* Effect)
{
    while (true)
    {
        Token Token = this->Lexer->NextToken();
        if (Token.Type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->ParseIdentifier(Effect, Token);
        }
        else if (Token.Type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }
}

void ShaderFXParser::ParseIdentifier(ShaderEffect* Effect, const Token& Token)
{
    char Char = Token.Text[0];
    switch (Char)
    {
        case 'S':
        {
            if (Token.MatchesKeyword("Shader"))
            {
                this->ParseShaderDeclaration(Effect);
            }
        }
        break;
        case 'L':
        {
            if (Token.MatchesKeyword("Layout"))
            {
                this->ParseLayoutBlock(Effect);
            }
        }
        break;
        case 'G':
        {
            if (Token.MatchesKeyword("GLSL"))
            {
                this->ParseGLSLBlock(Effect);
            }
        }
        break;
        case 'R':
        {
            if (Token.MatchesKeyword("RenderState"))
            {
                this->ParseRenderStateBlock(Effect);
            }
        }
        break;
        case 'P':
        {
            if (Token.MatchesKeyword("Pass"))
            {
                this->ParseRenderPassBlock(Effect);
            }
        }
        break;
    }
}

void ShaderFXParser::ParseShaderDeclaration(ShaderEffect* Effect)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        return;
    }

    Effect->Name = String(Token.Text, Token.Length);
}

void ShaderFXParser::ParseLayoutBlock(ShaderEffect* Effect)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
            continue;

        if (Token.MatchesKeyword("Vertex"))
        {
            // Consume "Vertex"
            Token = this->Lexer->NextToken();

            VertexLayoutDefinition Layout;
            Layout.Name = String(Token.Text, Token.Length);

            this->ParseVertexLayout(&Layout);

            Effect->VertexLayouts.Push(Layout);
        }
        else if (Token.MatchesKeyword("Resource"))
        {
            // Consume "Resource"
            Token = this->Lexer->NextToken();

            ResourceBindingsDefinition ResourceBindings;
            ResourceBindings.Name = String(Token.Text, Token.Length);

            this->ParseResourceBindings(&ResourceBindings);

            Effect->Resources.Push(ResourceBindings);
        }
        else if (Token.MatchesKeyword("ConstantBuffer"))
        {
            // Consume "ConstantBuffer"
            Token = this->Lexer->NextToken();

            ConstantBufferDefinition CBufferDefinition;
            CBufferDefinition.Name = String(Token.Text, Token.Length);

            this->ParseConstantBuffer(&CBufferDefinition);

            Effect->ConstantBuffers.Push(CBufferDefinition);
        }
        else if (Token.MatchesKeyword("UniformBuffer"))
        {
            // Consume "UniformBuffer"
            Token = this->Lexer->NextToken();

            UniformBufferDefinition UBufferDefinition;
            UBufferDefinition.Name = String(Token.Text, Token.Length);

            this->ParseUniformBuffer(&UBufferDefinition);

            Effect->UniformBuffers.Push(UBufferDefinition);
        }
        else if (Token.MatchesKeyword("StorageBuffer"))
        {
            // Consume "StorageBuffer"
            Token = this->Lexer->NextToken();

            UniformBufferDefinition UBufferDefinition;
            UBufferDefinition.Name = String(Token.Text, Token.Length);

            this->ParseUniformBuffer(&UBufferDefinition);

            Effect->StorageBuffers.Push(UBufferDefinition);
        }
    }
}

void ShaderFXParser::ParseResourceBindings(ResourceBindingsDefinition* ResourceBindings)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
            continue;

        ResourceBinding Binding;
        if (Token.MatchesKeyword("UniformBuffer"))
        {
            Binding.Type = ResourceType::UniformBuffer;
        }
        else if (Token.MatchesKeyword("StorageBuffer"))
        {
            Binding.Type = ResourceType::StorageBuffer;
        }
        else if (Token.MatchesKeyword("Sampler"))
        {
            Binding.Type = ResourceType::Sampler;
        }
        else
        {
            MARK_ERROR_WITH_FIELD("Invalid binding type", Token.Text);
            return;
        }

        // Parse the name of the binding
        Token = this->Lexer->NextToken();
        Binding.Name = String(Token.Text, Token.Length);

        while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            NamedToken Pair;
            if (!this->ParseNamedToken(Token, &Pair))
            {
                MARK_ERROR_RETURN("Unexpected token");
            }

            if (Pair.Key == "Stage")
            {
                Binding.Stage = (ShaderStageFlags)GetShaderStageFromString(Pair.Value.ToStringView());
                if (Binding.Stage == 0)
                {
                    MARK_ERROR_WITH_FIELD("Invalid shader stage", Pair.Value.Text);
                    return;
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
        }

        ResourceBindings->ResourceBindings.Push(Binding);
    }
}

void ShaderFXParser::ParseConstantBuffer(ConstantBufferDefinition* CBufferDefinition)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
            continue;

        ConstantBufferEntry Entry;

        // Parse the data type
        Entry.Type = this->ParseDataType(Token);
        if (Entry.Type == ShaderDataType::Count)
        {
            MARK_ERROR_WITH_FIELD("Invalid data type for constant buffer entry", Token.Text);
            return;
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            return;
        }

        Entry.Name = Token.ToString();

        // Parse properties
        while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            NamedToken Pair;
            if (this->ParseNamedToken(Token, &Pair))
            {
                if (Pair.Key == "Stage")
                {
                    Entry.Stage = (ShaderStageFlags)GetShaderStageFromString(Pair.Value.ToStringView());
                    if (Entry.Stage == 0)
                    {
                        MARK_ERROR_WITH_FIELD("Invalid shader stage", Pair.Value.Text);
                        return;
                    }
                }
            }
            else
            {
                MARK_ERROR_RETURN("Unexpected token");
            }
        }

        CBufferDefinition->Fields.Push(Entry);
    }
}

void ShaderFXParser::ParseUniformBuffer(UniformBufferDefinition* UBufferDefinition)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
            continue;

        UniformBufferEntry Entry;

        // Parse the data type
        Entry.Type = this->ParseDataType(Token);
        if (Entry.Type == ShaderDataType::Count)
        {
            MARK_ERROR_WITH_FIELD("Invalid data type for uniform buffer entry", Token.Text);
            return;
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            return;
        }

        Entry.Name = Token.ToString();
        UBufferDefinition->Fields.Push(Entry);
    }
}

#define MATCH_FORMAT(type, value)                                                                                                                                                                      \
    if (Token.MatchesKeyword(type))                                                                                                                                                                    \
    {                                                                                                                                                                                                  \
        return value;                                                                                                                                                                                  \
    }

ShaderDataType::Enum ShaderFXParser::ParseDataType(const Token& Token)
{
    char Char = Token.Text[0];
    switch (Char)
    {
        case 'f': // "Float", "Float2", "Float3", "Float4"
        case 'F':
        {
            MATCH_FORMAT("float", ShaderDataType::Float);
            MATCH_FORMAT("Float", ShaderDataType::Float);
            MATCH_FORMAT("float2", ShaderDataType::Float2);
            MATCH_FORMAT("Float2", ShaderDataType::Float2);
            MATCH_FORMAT("float3", ShaderDataType::Float3);
            MATCH_FORMAT("Float3", ShaderDataType::Float3);
            MATCH_FORMAT("float4", ShaderDataType::Float4);
            MATCH_FORMAT("Float4", ShaderDataType::Float4);
        }
        break;
        case 'm': // "Mat4"
        case 'M': // "Mat4"
        {
            MATCH_FORMAT("mat4", ShaderDataType::Mat4);
            MATCH_FORMAT("Mat4", ShaderDataType::Mat4);
        }
        case 'b': // "Byte", "Byte4N"
        case 'B': // "Byte", "Byte4N"
        {
            MATCH_FORMAT("byte", ShaderDataType::Byte);
            MATCH_FORMAT("Byte", ShaderDataType::Byte);
            MATCH_FORMAT("byte4n", ShaderDataType::Byte4N);
            MATCH_FORMAT("Byte4N", ShaderDataType::Byte4N);
        }
        break;
        case 'u': // "UByte", "UByte4N", "UInt", "UInt2", "UInt4"
        case 'U': // "UByte", "UByte4N", "UInt", "UInt2", "UInt4"
        {
            MATCH_FORMAT("ubyte", ShaderDataType::UByte);
            MATCH_FORMAT("UByte", ShaderDataType::UByte);
            MATCH_FORMAT("ubyte4n", ShaderDataType::UByte4N);
            MATCH_FORMAT("UByte4N", ShaderDataType::UByte4N);
            MATCH_FORMAT("uint", ShaderDataType::UInt);
            MATCH_FORMAT("UInt", ShaderDataType::UInt);
            MATCH_FORMAT("uint2", ShaderDataType::UInt2);
            MATCH_FORMAT("UInt2", ShaderDataType::UInt2);
            MATCH_FORMAT("uint4", ShaderDataType::UInt4);
            MATCH_FORMAT("UInt4", ShaderDataType::UInt4);
        }
        break;
        case 's': // "Short2", "Short2N", "Short4", "Short4N"
        case 'S': // "Short2", "Short2N", "Short4", "Short4N"
        {
            MATCH_FORMAT("short2", ShaderDataType::Short2);
            MATCH_FORMAT("Short2", ShaderDataType::Short2);
            MATCH_FORMAT("short2n", ShaderDataType::Short2N);
            MATCH_FORMAT("Short2N", ShaderDataType::Short2N);
            MATCH_FORMAT("short4", ShaderDataType::Short4);
            MATCH_FORMAT("Short4", ShaderDataType::Short4);
            MATCH_FORMAT("short4n", ShaderDataType::Short4N);
            MATCH_FORMAT("Short4N", ShaderDataType::Short4N);
        }
        break;
    }

    return ShaderDataType::Count;
}

#undef MATCH_FORMAT

bool ShaderFXParser::ParseNamedToken(const Token& CurrentToken, NamedToken* OutToken)
{
    if (CurrentToken.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
    {
        MARK_ERROR_WITH_FIELD("Error parsing named token; Expected TOKEN_TYPE_IDENTIFIER got", TokenType::String(CurrentToken.Type));
        return false;
    }

    OutToken->Key = StringView(CurrentToken.Text, CurrentToken.Length);
    Token NextToken;
    if (!this->Lexer->ExpectToken(&NextToken, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
    {
        MARK_ERROR_WITH_FIELD("Error parsing named token; Expected TOKEN_TYPE_OPEN_PARENTHESIS got", TokenType::String(NextToken.Type));
        return false;
    }

    // Skip past the parenthesis
    NextToken = this->Lexer->NextToken();
    OutToken->Value = NextToken;
    if (!this->Lexer->ExpectToken(&NextToken, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
    {
        MARK_ERROR_WITH_FIELD("Error parsing named token; Expected TOKEN_TYPE_CLOSE_PARENTHESIS got", TokenType::String(NextToken.Type));
        return false;
    }

    return true;
}

void ShaderFXParser::ParseVertexLayout(VertexLayoutDefinition* Layout)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
            continue;

        if (Token.MatchesKeyword("Attribute"))
        {
            this->ParseVertexAttribute(Layout);
        }
        else if (Token.MatchesKeyword("Binding"))
        {
            this->ParseVertexInputBinding(Layout);
        }
    }
}

void ShaderFXParser::ParseVertexAttribute(VertexLayoutDefinition* Layout)
{
    Token           CurrentToken;
    VertexAttribute Attribute;
    Attribute.Format = ShaderDataType::Count;

    // Format identifier
    if (this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        Attribute.Format = this->ParseDataType(CurrentToken);
    }

    if (Attribute.Format == ShaderDataType::Count)
    {
        MARK_ERROR_WITH_FIELD("Invalid data type", CurrentToken.Text);
        return;
    }

    if (!this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        MARK_ERROR("Expected identifier");
        return;
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
        MARK_ERROR("Invalid value for vertex binding index");
        return;
    }

    // Location
    if (this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Location = (uint16)CurrentToken.FloatValue;
    }
    else
    {
        MARK_ERROR("Invalid value for vertex location");
        return;
    }

    // Offset
    if (this->Lexer->ExpectToken(&CurrentToken, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Offset = (uint16)CurrentToken.FloatValue;
    }
    else
    {
        MARK_ERROR("Invalid value for vertex offset");
        return;
    }

    Layout->Attributes.Push(Attribute);
}

void ShaderFXParser::ParseVertexInputBinding(VertexLayoutDefinition* Layout)
{
    Token              Token;
    VertexInputBinding InputBinding;

    // Binding
    if (this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        InputBinding.Binding = (uint16)Token.FloatValue;
    }
    else
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    // Stride
    if (this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        InputBinding.Stride = (uint16)Token.FloatValue;
    }
    else
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    // Input Rate
    Token = this->Lexer->NextToken();
    if (Token.MatchesKeyword("vertex"))
    {
        InputBinding.InputRate = VertexInputRate::PerVertex;
    }
    else if (Token.MatchesKeyword("instance"))
    {
        InputBinding.InputRate = VertexInputRate::PerInstance;
    }
    else
    {
        MARK_ERROR("Invalid value for VertexInputRate");
        return;
    }

    Layout->InputBindings.Push(InputBinding);
}

void ShaderFXParser::ParseGLSLBlock(ShaderEffect* Effect)
{
    Token Token = this->Lexer->NextToken();

    ShaderCodeFragment CodeFragment;
    CodeFragment.Name = String(Token.Text, Token.Length);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    Token = this->Lexer->NextToken();
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
            Token = this->Lexer->NextToken();
        }
    }

    uint64 ShaderCodeLength = Token.Text - ShaderCodeStart;
    CodeFragment.Code = String(ShaderCodeStart, ShaderCodeLength);
    Effect->CodeFragments.Push(CodeFragment);
    if (this->Verbose)
    {
        KDEBUG("%s", *CodeFragment.Code);
    }
}

void ShaderFXParser::ParseRenderStateBlock(ShaderEffect* Effect)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    // Parse all the render states
    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->Error = true;
            this->ErrorLine = this->Lexer->Line;

            return;
        }

        // Expect "State"
        if (Token.MatchesKeyword("State"))
        {
            // Move past "State"
            Token = this->Lexer->NextToken();

            RenderStateDefinition State;
            State.Name = String(Token.Text, Token.Length);

            this->ParseRenderState(&State);

            Effect->RenderStates.Push(State);
        }
    }
}

void ShaderFXParser::ParseRenderState(RenderStateDefinition* State)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            MARK_ERROR("Expected Identifier");
            return;
        }

        if (Token.MatchesKeyword("Cull"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (Token.MatchesKeyword("Back"))
            {
                State->CullMode = CullModeFlags::Back;
            }
            else if (Token.MatchesKeyword("Front"))
            {
                State->CullMode = CullModeFlags::Front;
            }
            else if (Token.MatchesKeyword("FrontAndBack"))
            {
                State->CullMode = CullModeFlags::FrontAndBack;
            }
            else if (Token.MatchesKeyword("Off") || Token.MatchesKeyword("None"))
            {
                State->CullMode = CullModeFlags::None;
            }
            else
            {
                MARK_ERROR("Invalid value for CullMode");
                return;
            }
        }
        else if (Token.MatchesKeyword("ZTest"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            bool Valid = false;
            for (int i = 0; i < CompareOp::Count; i++)
            {
                if (Token.MatchesKeyword(CompareOp::Strings[i]))
                {
                    State->ZTestOperation = (CompareOp::Enum)i;
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                MARK_ERROR("Invalid value for ZTest");
                return;
            }
        }
        else if (Token.MatchesKeyword("ZWrite"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
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
                MARK_ERROR("Invalid value for ZWrite");
                return;
            }
        }
        else if (Token.MatchesKeyword("Blend"))
        {
            // Lot of tokens to parse here

            // SrcColor
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            // Blending turned off
            if (Token.MatchesKeyword("Off") || Token.MatchesKeyword("None"))
            {
                State->BlendEnable = false;
                continue;
            }

            State->BlendEnable = true;
            if (!this->ParseBlendFactor(Token, State->BlendMode.SrcColorBlendFactor))
            {
                return;
            }

            // DstColor
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State->BlendMode.DstColorBlendFactor))
            {
                return;
            }

            // Comma
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_COMMA))
            {
                return;
            }

            // SrcAlpha
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State->BlendMode.SrcAlphaBlendFactor))
            {
                return;
            }

            // DstAlpha
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State->BlendMode.DstAlphaBlendFactor))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("BlendOp"))
        {
            // ColorBlendOp
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendOp(Token, State->BlendMode.ColorBlendOperation))
            {
                return;
            }

            // Comma
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_COMMA))
            {
                return;
            }

            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            // AlphaBlendOp
            if (!this->ParseBlendOp(Token, State->BlendMode.AlphaBlendOperation))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("PolygonMode"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            for (int i = 0; i < PolygonMode::Count; i++)
            {
                if (Token.ToString() == PolygonMode::Strings[i])
                {
                    State->PolygonMode = (PolygonMode::Enum)i;
                    break;
                }
            }
        }
        else if (Token.MatchesKeyword("LineWidth"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_NUMBER))
            {
                return;
            }

            State->LineWidth = (float32)Token.FloatValue;
        }
    }
}

bool ShaderFXParser::ParseBlendFactor(const Token& Token, BlendFactor::Enum& Factor)
{
    bool Valid = false;
    for (int i = 0; i < BlendFactor::Count; i++)
    {
        if (Token.MatchesKeyword(BlendFactor::Strings[i]))
        {
            Factor = (BlendFactor::Enum)i;
            Valid = true;
            break;
        }
    }

    if (!Valid)
    {
        StringCopy(this->ErrorString, "Invalid value ");
        StringNConcat(this->ErrorString, Token.Text, Token.Length);
        StringConcat(this->ErrorString, " for BlendFactor");
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseBlendOp(const Token& Token, BlendOp::Enum& Op)
{
    bool Valid = false;
    for (int i = 0; i < BlendOp::Count; i++)
    {
        if (Token.MatchesKeyword(BlendOp::Strings[i]))
        {
            Op = (BlendOp::Enum)i;
            Valid = true;
            break;
        }
    }

    if (!Valid)
    {
        StringCopy(this->ErrorString, "Invalid value ");
        StringNConcat(this->ErrorString, Token.Text, Token.Length);
        StringConcat(this->ErrorString, " for BlendOp");

        return false;
    }

    return true;
}

void ShaderFXParser::ParseRenderPassBlock(ShaderEffect* Effect)
{
    // Consume the name of the pass
    Token Token = this->Lexer->NextToken();

    RenderPassDefinition Pass;
    Pass.Name = String(Token.Text, Token.Length);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(&Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            MARK_ERROR_RETURN("Expected identifier");
        }

        if (Token.MatchesKeyword("RenderState"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                return;

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
                MARK_ERROR_WITH_FIELD("Unknown render state", Token.Text);
                return;
            }
        }
        else if (Token.MatchesKeyword("VertexLayout"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                return;

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
                MARK_ERROR_WITH_FIELD("Unknown vertex layout", Token.Text);
                return;
            }
        }
        else if (Token.MatchesKeyword("Resources"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                return;

            // It is possible for shaders to have no local resources
            if (Effect->Resources.Length == 0)
                continue;

            StringView Name = Token.ToStringView();
            bool       Valid = false;
            for (int i = 0; i < Effect->Resources.Length; i++)
            {
                if (Effect->Resources[i].Name == Name)
                {
                    Pass.Resources = &Effect->Resources[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                MARK_ERROR_WITH_FIELD("Unknown resource buffer", Token.Text);
                return;
            }
        }
        else if (Token.MatchesKeyword("ConstantBuffer"))
        {
            if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
                return;

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
                MARK_ERROR_WITH_FIELD("Unknown constant buffer", Token.Text);
                return;
            }
        }
        else if (Token.MatchesKeyword("VertexShader"))
        {
            if (!this->ParseRenderPassShaderStage(Effect, &Pass, ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("FragmentShader"))
        {
            if (!this->ParseRenderPassShaderStage(Effect, &Pass, ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT))
            {
                return;
            }
        }
    }

    Effect->RenderPasses.Push(Pass);
}

bool ShaderFXParser::ParseRenderPassShaderStage(ShaderEffect* Effect, RenderPassDefinition* Pass, renderer::ShaderStageFlags ShaderStage)
{
    Token Token;
    if (!this->Lexer->ExpectToken(&Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
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

    MARK_ERROR("Unknown shader");

    return false;
}

#define KRAFT_VERTEX_DEFINE       "VERTEX"
#define KRAFT_GEOMETRY_DEFINE     "GEOMETRY"
#define KRAFT_FRAGMENT_DEFINE     "FRAGMENT"
#define KRAFT_COMPUTE_DEFINE      "COMPUTE"
#define KRAFT_SHADERFX_ENABLE_VAL "1"

bool CompileShaderFX(const String& InputPath, const String& OutputPath, bool Verbose)
{
    KINFO("Compiling shaderfx %s", *InputPath);

    filesystem::FileHandle File;
    bool                   Result = filesystem::OpenFile(InputPath, filesystem::FILE_OPEN_MODE_READ, true, &File);
    if (!Result)
    {
        return false;
    }

    ArenaAllocator* TempArena = CreateArena({ .ChunkSize = KRAFT_SIZE_MB(16), .Alignment = 64 });

    uint64 BufferSize = kraft::filesystem::GetFileSize(&File) + 1;
    uint8* FileDataBuffer = ArenaPush(TempArena, BufferSize);
    filesystem::ReadAllBytes(&File, &FileDataBuffer);
    filesystem::CloseFile(&File);

    Lexer Lexer;
    Lexer.Create((char*)FileDataBuffer);
    renderer::ShaderFXParser Parser;
    Parser.Verbose = Verbose;

    ShaderEffect Effect = Parser.Parse(InputPath, &Lexer);
    Result = CompileShaderFX(TempArena, Effect, OutputPath);

    DestroyArena(TempArena);
    return Result;
}

uint64 GetUniformBufferSize(const UniformBufferDefinition& UniformBuffer)
{
    uint64 SizeInBytes = 0;
    for (int FieldIdx = 0; FieldIdx < UniformBuffer.Fields.Length; FieldIdx++)
    {
        const UniformBufferEntry& Field = UniformBuffer.Fields[FieldIdx];
        SizeInBytes += ShaderDataType::SizeOf(Field.Type);
    }

    return SizeInBytes;
}

static bool CheckBufferDefinition(ShaderEffect& Shader, const Array<UniformBufferDefinition>* Buffers, ResourceBinding* BindingDescription)
{
    for (int UBIdx = 0; UBIdx < Buffers->Length; UBIdx++)
    {
        if ((*Buffers)[UBIdx].Name == BindingDescription->Name)
        {
            // Check if the size is incorrect
            uint64 ExpectedSize = GetUniformBufferSize((*Buffers)[UBIdx]);
            // TODO: Maybe auto fix?
            if (BindingDescription->Size != ExpectedSize)
            {
                KERROR(
                    "'%s' shader compilation failed with error: Incorrect Buffer size for '%s': '%d bytes'. Expected '%d bytes'.",
                    *Shader.ResourcePath,
                    *BindingDescription->Name,
                    BindingDescription->Size,
                    ExpectedSize
                );

                return false;
            }

            // Update the parent index
            BindingDescription->ParentIndex = UBIdx;

            return true;
        }
    }

    KERROR("'%s' shader compilation failed with error: Missing BufferDefinition for '%s'.", *Shader.ResourcePath, *BindingDescription->Name);

    return false;
}

struct ShaderIncludeUserData
{
    ArenaAllocator*       Arena;
    ShaderEffect* Shader;
};

static shaderc_include_result* ShaderIncludeResolverFunction(void* UserData, const char* RequestedSource, int Type, const char* RequestingSource, size_t IncludeDepth)
{
    ShaderIncludeUserData* IncludeData = (ShaderIncludeUserData*)UserData;
    ArenaAllocator*                Arena = IncludeData->Arena;
    ShaderEffect*          Shader = IncludeData->Shader;

    char* ShaderDirectory = filesystem::Dirname(Arena, Shader->ResourcePath);
    char* IncludedFilePath = filesystem::PathJoin(Arena, ShaderDirectory, RequestedSource);

    filesystem::FileHandle File;
    KASSERT(filesystem::OpenFile(IncludedFilePath, kraft::filesystem::FILE_OPEN_MODE_READ, true, &File));
    uint64 FileSize = filesystem::GetFileSize(&File);
    uint8* FileBuffer = kraft::ArenaPush(Arena, FileSize);
    filesystem::ReadAllBytes(&File, &FileBuffer);

    shaderc_include_result* Result = (shaderc_include_result*)ArenaPush(Arena, sizeof(shaderc_include_result));
    Result->content = (const char*)FileBuffer;
    Result->content_length = FileSize;

    uint64 SourceNameLength = StringLength(IncludedFilePath);
    char*  SourceName = ArenaPushString(Arena, IncludedFilePath, SourceNameLength);
    Result->source_name = SourceName;
    Result->source_name_length = SourceNameLength;

    return Result;
}

static void ShaderIncludeResultReleaseFunction(void* user_data, shaderc_include_result* include_result)
{}

bool CompileShaderFX(ArenaAllocator* Arena, ShaderEffect& Shader, const String& OutputPath, bool Verbose)
{
    // Basic verification
    auto& Resources = Shader.Resources;
    for (int i = 0; i < Resources.Length; i++)
    {
        auto& Resource = Resources[i];
        for (int j = 0; j < Resource.ResourceBindings.Length; j++)
        {
            if (Resource.ResourceBindings[j].Type == ResourceType::UniformBuffer)
            {
                if (false == CheckBufferDefinition(Shader, &Shader.UniformBuffers, &Resource.ResourceBindings[j]))
                {
                    return false;
                }
            }
            else if (Resource.ResourceBindings[j].Type == ResourceType::StorageBuffer)
            {
                if (false == CheckBufferDefinition(Shader, &Shader.StorageBuffers, &Resource.ResourceBindings[j]))
                {
                    return false;
                }
            }
        }
    }

    shaderc_compiler_t           Compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t Result;
    shaderc_compile_options_t    CompileOptions;

    kraft::Buffer BinaryOutput;
    BinaryOutput.Write(Shader.Name);
    BinaryOutput.Write(Shader.ResourcePath);
    BinaryOutput.Write(Shader.VertexLayouts);
    BinaryOutput.Write(Shader.Resources);
    BinaryOutput.Write(Shader.ConstantBuffers);
    BinaryOutput.Write(Shader.UniformBuffers);
    BinaryOutput.Write(Shader.StorageBuffers);
    BinaryOutput.Write(Shader.RenderStates);
    BinaryOutput.Write(Shader.RenderPasses.Length);

    // We don't have to write the vertex layouts or render states
    // They are written as part of the renderpass definition
    for (int i = 0; i < Shader.RenderPasses.Length; i++)
    {
        const RenderPassDefinition& Pass = Shader.RenderPasses[i];
        BinaryOutput.Write(Pass.Name);

        // Write the vertex layout & render state offsets
        BinaryOutput.Write((int64)(Pass.VertexLayout - &Shader.VertexLayouts[0]));
        BinaryOutput.Write((int64)(Shader.Resources.Length > 0 ? Pass.Resources - &Shader.Resources[0] : -1));
        BinaryOutput.Write((int64)(Pass.ConstantBuffers - &Shader.ConstantBuffers[0]));
        BinaryOutput.Write((int64)(Pass.RenderState - &Shader.RenderStates[0]));

        // Manually write the shader code fragments to the buffer
        BinaryOutput.Write(Pass.ShaderStages.Length);

        for (int j = 0; j < Pass.ShaderStages.Length; j++)
        {
            const RenderPassDefinition::ShaderDefinition& Stage = Pass.ShaderStages[j];
            if (Verbose)
            {
                KDEBUG("Compiling shaderstage %d for %s", Stage.Stage, *Shader.ResourcePath);
            }

            CompileOptions = shaderc_compile_options_initialize();
            ShaderIncludeUserData UserData = {
                .Arena = Arena,
                .Shader = &Shader,
            };
            shaderc_compile_options_set_include_callbacks(CompileOptions, ShaderIncludeResolverFunction, ShaderIncludeResultReleaseFunction, &UserData);

            shaderc_shader_kind ShaderKind;
            switch (Stage.Stage)
            {
                case ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX:
                {
                    ShaderKind = shaderc_vertex_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_VERTEX_DEFINE, sizeof(KRAFT_VERTEX_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;

                case ShaderStageFlags::SHADER_STAGE_FLAGS_GEOMETRY:
                {
                    ShaderKind = shaderc_geometry_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_GEOMETRY_DEFINE, sizeof(KRAFT_GEOMETRY_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;

                case ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT:
                {
                    ShaderKind = shaderc_fragment_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_FRAGMENT_DEFINE, sizeof(KRAFT_FRAGMENT_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;

                case ShaderStageFlags::SHADER_STAGE_FLAGS_COMPUTE:
                {
                    ShaderKind = shaderc_compute_shader;
                    shaderc_compile_options_add_macro_definition(
                        CompileOptions, KRAFT_COMPUTE_DEFINE, sizeof(KRAFT_COMPUTE_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1
                    );
                }
                break;
            }

            Result = shaderc_compile_into_spv(Compiler, *Stage.CodeFragment.Code, Stage.CodeFragment.Code.Length, ShaderKind, "shader", "main", CompileOptions);
            if (shaderc_result_get_compilation_status(Result) == shaderc_compilation_status_success)
            {
                ShaderCodeFragment SpirvBinary;
                SpirvBinary.Name = Stage.CodeFragment.Name;
                SpirvBinary.Code = String(shaderc_result_get_bytes(Result), shaderc_result_get_length(Result));
                BinaryOutput.Write(Stage.Stage);
                BinaryOutput.Write(SpirvBinary);

                shaderc_result_release(Result);
            }
            else
            {
                KERROR("%d shader compilation failed with error:\n%s", Stage.Stage, shaderc_result_get_error_message(Result));

                shaderc_result_release(Result);
                shaderc_compiler_release(Compiler);

                return false;
            }
        }
    }

    shaderc_compiler_release(Compiler);

    filesystem::FileHandle File;
    if (filesystem::OpenFile(OutputPath, filesystem::FILE_OPEN_MODE_WRITE, true, &File))
    {
        filesystem::WriteFile(&File, BinaryOutput);
        filesystem::CloseFile(&File);
    }
    else
    {
        KERROR("[CompileKFX]: Failed to write binary output to file %s", *OutputPath);
        return false;
    }

    return true;
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
    Reader.Read(&Shader->Resources);
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
        Pass.Resources = ResourcesOffset > -1 ? &Shader->Resources[ResourcesOffset] : nullptr;

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

}
