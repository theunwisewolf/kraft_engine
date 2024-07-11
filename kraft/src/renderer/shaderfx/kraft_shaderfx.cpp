#include "kraft_shaderfx.h"

#include "core/kraft_memory.h"
#include "core/kraft_asserts.h"
#include "platform/kraft_filesystem.h"
#include "containers/kraft_buffer.h"

#include "shaderc/shaderc.h"

#define MARK_ERROR(error) do { this->Error = true; \
            this->ErrorLine = this->Lexer->Line; \
            this->ErrorString = error; } while(0)

#define MARK_ERROR_RETURN(error) do { this->Error = true; \
            this->ErrorLine = this->Lexer->Line; \
            this->ErrorString = error; \
            return; } while(0)

#define MARK_ERROR_WITH_FIELD_RETURN(error, field) do { this->Error = true; \
            this->ErrorLine = this->Lexer->Line; \
            this->ErrorString = error + String("'") + field + String("'"); \
            return; } while(0)

namespace kraft::renderer
{

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

void ShaderFXParser::Create(const String& SourceFilePath, kraft::Lexer* Lexer)
{
    this->Lexer = Lexer;
    Shader.ResourcePath = SourceFilePath;

    KASSERT(this->Lexer);
}

void ShaderFXParser::Destroy()
{
}

void ShaderFXParser::GenerateAST()
{
    while (true)
    {
        Token Token = this->Lexer->NextToken();
        if (Token.Type == TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            this->ParseIdentifier(Token);
        }
        else if (Token.Type == TokenType::TOKEN_TYPE_END_OF_STREAM)
        {
            break;
        }
    }
}

void ShaderFXParser::ParseIdentifier(Token Token)
{
    char Char = Token.Text[0];
    switch (Char)
    {
        case 'S':
        {
            if (Token.MatchesKeyword("Shader"))
            {
                this->ParseShaderDeclaration();
            }
        } break;
        case 'L':
        {
            if (Token.MatchesKeyword("Layout"))
            {
                this->ParseLayoutBlock();
            }
        } break;
        case 'G':
        {
            if (Token.MatchesKeyword("GLSL"))
            {
                this->ParseGLSLBlock();
            }
        } break;
        case 'R':
        {
            if (Token.MatchesKeyword("RenderState"))
            {
                this->ParseRenderStateBlock();
            }
        } break;
        case 'P':
        {
            if (Token.MatchesKeyword("Pass"))
            {
                this->ParseRenderPassBlock();
            }
        } break;
    }
}

void ShaderFXParser::ParseShaderDeclaration()
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        return;
    }

    this->Shader.Name = String(Token.Text, Token.Length);
}

void ShaderFXParser::ParseLayoutBlock()
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER) continue;

        if (Token.MatchesKeyword("Vertex"))
        {
            // Consume "Vertex"
            Token = this->Lexer->NextToken();

            VertexLayoutDefinition Layout;
            Layout.Name = String(Token.Text, Token.Length);
            
            this->ParseVertexLayout(Layout);

            this->Shader.VertexLayouts.Push(Layout);
        }
        else if (Token.MatchesKeyword("Resource"))
        {
            // Consume "Resource"
            Token = this->Lexer->NextToken();

            ResourceBindingsDefinition ResourceBindings;
            ResourceBindings.Name = String(Token.Text, Token.Length);

            this->ParseResourceBindings(ResourceBindings);

            this->Shader.Resources.Push(ResourceBindings);
        }
        else if (Token.MatchesKeyword("ConstantBuffer"))
        {
            // Consume "ConstantBuffer"
            Token = this->Lexer->NextToken();

            ConstantBufferDefinition CBufferDefinition;
            CBufferDefinition.Name = String(Token.Text, Token.Length);

            this->ParseConstantBuffer(CBufferDefinition);

            this->Shader.ConstantBuffers.Push(CBufferDefinition);
        }
    }
}

void ShaderFXParser::ParseResourceBindings(ResourceBindingsDefinition& ResourceBindings)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER) continue;

        ResourceBinding Binding;
        if (Token.MatchesKeyword("UniformBuffer"))
        {
            Binding.Type = ResourceType::UniformBuffer;
        }
        else if (Token.MatchesKeyword("Sampler"))
        {
            Binding.Type = ResourceType::Sampler;
        }
        else
        {
            MARK_ERROR_RETURN("Invalid binding type '" + String(Token.Text, Token.Length) + "'");
        }

        // Parse the name of the binding
        Token = this->Lexer->NextToken();
        Binding.Name = String(Token.Text, Token.Length);

        while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            NamedToken Pair;
            if (!this->ParseNamedToken(Token, Pair))
            {
                MARK_ERROR_RETURN("Unexpected token");
            }

            if (Pair.Key == "Stage")
            {
                Binding.Stage = (ShaderStageFlags)GetShaderStageFromString(Pair.Value.StringView());
                if (Binding.Stage == 0)
                {
                    MARK_ERROR_RETURN("Invalid shader stage '" + Pair.Value.String() + "'");
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

        ResourceBindings.ResourceBindings.Push(Binding);
    }
}

void ShaderFXParser::ParseConstantBuffer(ConstantBufferDefinition& CBufferDefinition)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER) continue;

        ConstantBufferEntry Entry;
        
        // Parse the data type
        Entry.Type = this->ParseDataType(Token);
        if (Entry.Type == ShaderDataType::Count)
        {
            MARK_ERROR_RETURN("Invalid data type for constant buffer entry '" + String(Token.Text, Token.Length) + "'");
        }

        // Parse the name
        if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
        {
            return;
        }
        
        Entry.Name = Token.String();

        // Parse properties
        while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_SEMICOLON))
        {
            NamedToken Pair;
            if (this->ParseNamedToken(Token, Pair))
            {
                if (Pair.Key == "Stage")
                {
                    Entry.Stage = (ShaderStageFlags)GetShaderStageFromString(Pair.Value.StringView());
                    if (Entry.Stage == 0)
                    {
                        MARK_ERROR_RETURN("Invalid shader stage '" + Pair.Value.String() + "'");
                    }
                }
            }
            else
            {
                MARK_ERROR_RETURN("Unexpected token");
            }
        }

        CBufferDefinition.Fields.Push(Entry);
    }
}

#define MATCH_FORMAT(type, value) if (Token.MatchesKeyword(type)) { return value; }

ShaderDataType::Enum ShaderFXParser::ParseDataType(Token Token)
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
        } break;
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
        } break;
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
        } break;
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
        } break;
    }

    return ShaderDataType::Count;
}

#undef MATCH_FORMAT

bool ShaderFXParser::ParseNamedToken(Token Token, NamedToken& OutToken)
{
    if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
    {
        MARK_ERROR("Error parsing named token; Expected TOKEN_TYPE_IDENTIFIER got " + String(TokenType::String(Token.Type)));
        return false;
    }

    OutToken.Key = StringView(Token.Text, Token.Length);
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_PARENTHESIS))
    {
        MARK_ERROR("Error parsing named token; Expected TOKEN_TYPE_OPEN_PARENTHESIS got " + String(TokenType::String(Token.Type)));
        return false;
    }

    // Skip past the parenthesis
    Token = this->Lexer->NextToken();
    OutToken.Value = Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_CLOSE_PARENTHESIS))
    {
        MARK_ERROR("Error parsing named token; Expected TOKEN_TYPE_CLOSE_PARENTHESIS got " + String(TokenType::String(Token.Type)));
        return false;
    }

    return true;
}

void ShaderFXParser::ParseVertexLayout(VertexLayoutDefinition& Layout)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER) continue;

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

void ShaderFXParser::ParseVertexAttribute(VertexLayoutDefinition &Layout)
{
    Token Token;
    VertexAttribute Attribute;
    Attribute.Format = ShaderDataType::Count;

    // Format identifier
    if (this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        Attribute.Format = this->ParseDataType(Token);
    }

    if (Attribute.Format == ShaderDataType::Count)
    {
        MARK_ERROR_RETURN("Invalid data type '" + Token.String() + "'");
    }

    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        MARK_ERROR("Expected identifier");
        return;
    }

    // Attribute name
    // Skip
    // Attribute.Name = String(Token, Token.Length);

    // Binding
    if (this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Binding = (uint16)Token.FloatValue;
    }
    else
    {
        MARK_ERROR("Invalid value for vertex binding index");
        return;
    }

    // Location
    if (this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Location = (uint16)Token.FloatValue;
    }
    else
    {
        MARK_ERROR("Invalid value for vertex location");
        return;
    }

    // Offset
    if (this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_NUMBER))
    {
        Attribute.Offset = (uint16)Token.FloatValue;
    }
    else
    {
        MARK_ERROR("Invalid value for vertex offset");
        return;
    }

    Layout.Attributes.Push(Attribute);
}

void ShaderFXParser::ParseVertexInputBinding(VertexLayoutDefinition& Layout)
{
    Token Token;
    VertexInputBinding InputBinding;
    
    // Binding
    if (this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_NUMBER))
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
    if (this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_NUMBER))
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

    Layout.InputBindings.Push(InputBinding);
}

void ShaderFXParser::ParseGLSLBlock()
{
    Token Token = this->Lexer->NextToken();

    ShaderCodeFragment CodeFragment;
    CodeFragment.Name = String(Token.Text, Token.Length);
    
    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }
    
    Token = this->Lexer->NextToken();
    CodeFragment.Code.Buffer = Token.Text;

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

    CodeFragment.Code.Length = Token.Text - CodeFragment.Code.Buffer;
    this->Shader.CodeFragments.Push(CodeFragment);

    String CodeStr(CodeFragment.Code);
    KDEBUG("%s", *CodeStr);
}

void ShaderFXParser::ParseRenderStateBlock()
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    // Parse all the render states
    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
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
            
            this->ParseRenderState(State);

            this->Shader.RenderStates.Push(State);
        }
    }
}

void ShaderFXParser::ParseRenderState(RenderStateDefinition& State)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            MARK_ERROR("Expected Identifier");
            return;
        }

        if (Token.MatchesKeyword("Cull"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }
            
            if (Token.MatchesKeyword("Back"))
            {
                State.CullMode = CullModeFlags::CULL_MODE_FLAGS_BACK;
            }
            else if (Token.MatchesKeyword("Front"))
            {
                State.CullMode = CullModeFlags::CULL_MODE_FLAGS_FRONT;
            }
            else if (Token.MatchesKeyword("FrontAndBack"))
            {
                State.CullMode = CullModeFlags::CULL_MODE_FLAGS_FRONT_AND_BACK;
            }
            else if (Token.MatchesKeyword("Off"))
            {
                State.CullMode = CullModeFlags::CULL_MODE_FLAGS_NONE;
            }
            else
            {
                MARK_ERROR("Invalid value for CullMode");
                return;
            }
        }
        else if (Token.MatchesKeyword("ZTest"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            bool Valid = false;
            for (int i = 0; i < CompareOp::Count; i++)
            {
                if (Token.MatchesKeyword(CompareOp::Strings[i]))
                {
                    State.ZTestOperation = (CompareOp::Enum)i;
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
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (Token.MatchesKeyword("On"))
            {
                State.ZWriteEnable = true;
            }
            else if (Token.MatchesKeyword("Off"))
            {
                State.ZWriteEnable = false;
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
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.SrcColorBlendFactor))
            {
                return;
            }

            // DstColor
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.DstColorBlendFactor))
            {
                return;
            }

            // Comma
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_COMMA))
            {
                return;
            }

            // SrcAlpha
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.SrcAlphaBlendFactor))
            {
                return;
            }

            // DstAlpha
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.DstAlphaBlendFactor))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("BlendOp"))
        {
            // ColorBlendOp
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendOp(Token, State.BlendMode.ColorBlendOperation))
            {
                return;
            }

            // Comma
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_COMMA))
            {
                return;
            }

            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            // AlphaBlendOp
            if (!this->ParseBlendOp(Token, State.BlendMode.AlphaBlendOperation))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("PolygonMode"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            for (int i = 0; i < PolygonMode::Count; i++)
            {
                if (Token.String() == PolygonMode::Strings[i])
                {
                    State.PolygonMode = (PolygonMode::Enum)i;
                    break;
                }
            }
        }
        else if (Token.MatchesKeyword("LineWidth"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_NUMBER))
            {
                return;
            }
            
            State.LineWidth = (float32)Token.FloatValue;
        }
    }
}

bool ShaderFXParser::ParseBlendFactor(Token Token, BlendFactor::Enum& Factor)
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
        MARK_ERROR("Invalid value " + Token.String() + " for BlendFactor");
        return false;
    }

    return true;
}

bool ShaderFXParser::ParseBlendOp(Token Token, BlendOp::Enum& Op)
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
        MARK_ERROR("Invalid value " + Token.String() + " for BlendOp");
        return false;
    }

    return true;
}

void ShaderFXParser::ParseRenderPassBlock()
{
    // Consume the name of the pass
    Token Token = this->Lexer->NextToken();

    RenderPassDefinition Pass;
    Pass.Name = String(Token.Text, Token.Length);

    // Consume the first brace of the GLSL Block
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TokenType::TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TokenType::TOKEN_TYPE_IDENTIFIER)
        {
            MARK_ERROR_RETURN("Expected identifier");
        }

        if (Token.MatchesKeyword("RenderState"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER)) return;

            StringView Name = Token.StringView();
            bool Valid = false;
            for (int i = 0; i < this->Shader.RenderStates.Length; i++)
            {
                if (this->Shader.RenderStates[i].Name == Name)
                {
                    Pass.RenderState = &this->Shader.RenderStates[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid) MARK_ERROR_WITH_FIELD_RETURN("Unknown render state", Name);
        }
        else if (Token.MatchesKeyword("VertexLayout"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER)) return;
            
            StringView Name = Token.StringView();
            bool Valid = false;
            for (int i = 0; i < this->Shader.VertexLayouts.Length; i++)
            {
                if (this->Shader.VertexLayouts[i].Name == Name)
                {
                    Pass.VertexLayout = &this->Shader.VertexLayouts[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid) MARK_ERROR_WITH_FIELD_RETURN("Unknown vertex layout", Name);
        }
        else if (Token.MatchesKeyword("Resources"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER)) return;

            StringView Name = Token.StringView();
            bool Valid = false;
            for (int i = 0; i < this->Shader.Resources.Length; i++)
            {
                if (this->Shader.Resources[i].Name == Name)
                {
                    Pass.Resources = &this->Shader.Resources[i];
                    Valid = true;
                    break;
                } 
            }

            if (!Valid) MARK_ERROR_WITH_FIELD_RETURN("Unknown resource buffer", Name);
        }
        else if (Token.MatchesKeyword("ConstantBuffer"))
        {
            if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER)) return;

            StringView Name = Token.StringView();
            bool Valid = false;
            for (int i = 0; i < this->Shader.ConstantBuffers.Length; i++)
            {
                if (this->Shader.ConstantBuffers[i].Name == Name)
                {
                    Pass.ConstantBuffers = &this->Shader.ConstantBuffers[i];
                    Valid = true;
                    break;
                } 
            }

            if (!Valid) MARK_ERROR_WITH_FIELD_RETURN("Unknown constant buffer", Name);
        }
        else if (Token.MatchesKeyword("VertexShader"))
        {
            if (!this->ParseRenderPassShaderStage(Pass, ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("FragmentShader"))
        {
            if (!this->ParseRenderPassShaderStage(Pass, ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT))
            {
                return;
            }
        }
    }

    this->Shader.RenderPasses.Push(Pass);
}

bool ShaderFXParser::ParseRenderPassShaderStage(RenderPassDefinition& Pass, renderer::ShaderStageFlags ShaderStage)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TokenType::TOKEN_TYPE_IDENTIFIER))
    {
        return false;
    }

    for (int i = 0; i < this->Shader.CodeFragments.Length; i++)
    {
        if (Token.MatchesKeyword(this->Shader.CodeFragments[i].Name))
        {
            RenderPassDefinition::ShaderDefinition Stage;
            Stage.Stage = ShaderStage;
            Stage.CodeFragment = this->Shader.CodeFragments[i];
            Pass.ShaderStages.Push(Stage);
            
            return true;
        }
    }

    MARK_ERROR("Unknown shader");

    return false;
}

#define KRAFT_VERTEX_DEFINE "VERTEX"
#define KRAFT_GEOMETRY_DEFINE "GEOMETRY"
#define KRAFT_FRAGMENT_DEFINE "FRAGMENT"
#define KRAFT_COMPUTE_DEFINE "COMPUTE"
#define KRAFT_SHADERFX_ENABLE_VAL "1"

bool CompileShaderFX(const String& InputPath, const String& OutputPath)
{
    KINFO("Compiling shaderfx %s", *InputPath);
    
    FileHandle Handle;
    bool Result = filesystem::OpenFile(InputPath, kraft::FILE_OPEN_MODE_READ, true, &Handle);
    if (!Result)
    {
        return false;
    }

    uint64 BufferSize = kraft::filesystem::GetFileSize(&Handle) + 1;
    uint8* FileDataBuffer = (uint8*)Malloc(BufferSize, kraft::MemoryTag::MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&Handle, &FileDataBuffer);
    filesystem::CloseFile(&Handle);

    Lexer Lexer;
    Lexer.Create((char*)FileDataBuffer);
    renderer::ShaderFXParser Parser;
    Parser.Create(InputPath, &Lexer);
    Parser.GenerateAST();

    if (!CompileShaderFX(Parser.Shader, OutputPath))
    {
        Free(FileDataBuffer, BufferSize, MEMORY_TAG_FILE_BUF);
        return false;
    }

    Free(FileDataBuffer, BufferSize, MEMORY_TAG_FILE_BUF);
    return true;
}

bool CompileShaderFX(const ShaderEffect& Shader, const String& OutputPath)
{
    shaderc_compiler_t Compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t Result;
    shaderc_compile_options_t CompileOptions;

    kraft::Buffer BinaryOutput;
    BinaryOutput.Write(Shader.Name);
    BinaryOutput.Write(Shader.ResourcePath);
    BinaryOutput.Write(Shader.VertexLayouts);
    BinaryOutput.Write(Shader.Resources);
    BinaryOutput.Write(Shader.ConstantBuffers);
    BinaryOutput.Write(Shader.RenderStates);
    BinaryOutput.Write(Shader.RenderPasses.Length);

    // We don't have to write the vertex layouts or render states
    // They are written as part of the renderpass definition
    for (int i = 0; i < Shader.RenderPasses.Length; i++)
    {
        const RenderPassDefinition& Pass = Shader.RenderPasses[i];
        BinaryOutput.Write(Pass.Name);

        // Write the vertex layout & render state offsets
        BinaryOutput.Write((uint64)(Pass.VertexLayout - &Shader.VertexLayouts[0]));
        BinaryOutput.Write((uint64)(Pass.Resources - &Shader.Resources[0]));
        BinaryOutput.Write((uint64)(Pass.ConstantBuffers - &Shader.ConstantBuffers[0]));
        BinaryOutput.Write((uint64)(Pass.RenderState - &Shader.RenderStates[0]));

        // Manually write the shader code fragments to the buffer
        BinaryOutput.Write(Pass.ShaderStages.Length);

        for (int j = 0; j < Pass.ShaderStages.Length; j++)
        {
            const RenderPassDefinition::ShaderDefinition& Stage = Pass.ShaderStages[j];
            KDEBUG("Compiling shaderstage %d for %s", Stage.Stage, *Shader.ResourcePath);

            CompileOptions = shaderc_compile_options_initialize();
            shaderc_shader_kind ShaderKind;
            switch (Stage.Stage)
            {
                case ShaderStageFlags::SHADER_STAGE_FLAGS_VERTEX:
                {
                    ShaderKind = shaderc_vertex_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_VERTEX_DEFINE, sizeof(KRAFT_VERTEX_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;

                case ShaderStageFlags::SHADER_STAGE_FLAGS_GEOMETRY:
                {
                    ShaderKind = shaderc_geometry_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_GEOMETRY_DEFINE, sizeof(KRAFT_GEOMETRY_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;

                case ShaderStageFlags::SHADER_STAGE_FLAGS_FRAGMENT:
                {
                    ShaderKind = shaderc_fragment_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_FRAGMENT_DEFINE, sizeof(KRAFT_FRAGMENT_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;

                case ShaderStageFlags::SHADER_STAGE_FLAGS_COMPUTE:
                {
                    ShaderKind = shaderc_compute_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_COMPUTE_DEFINE, sizeof(KRAFT_COMPUTE_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;
            }
            
            Result = shaderc_compile_into_spv(Compiler, *Stage.CodeFragment.Code, Stage.CodeFragment.Code.Length, ShaderKind, "shader", "main", CompileOptions);
            if (shaderc_result_get_compilation_status(Result) == shaderc_compilation_status_success)
            {
                ShaderCodeFragment SpirvBinary;
                SpirvBinary.Name = Stage.CodeFragment.Name;
                SpirvBinary.Code = StringView(shaderc_result_get_bytes(Result), shaderc_result_get_length(Result));
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

    FileHandle Handle;
    if (filesystem::OpenFile(OutputPath, kraft::FILE_OPEN_MODE_WRITE, true, &Handle))
    {
        filesystem::WriteFile(&Handle, BinaryOutput);
        filesystem::CloseFile(&Handle);
    }
    else
    {
        KERROR("[CompileKFX]: Failed to write binary output to file %s", *OutputPath);
        return false;
    }

    return true;
}

bool LoadShaderFX(const String& Path, ShaderEffect& Shader)
{
    FileHandle Handle;

    // Read test
    if (!filesystem::OpenFile(Path, kraft::FILE_OPEN_MODE_READ, true, &Handle))
    {
        KERROR("Failed to read file %s", *Path);
        return false;
    }

    uint64 BinaryBufferSize = kraft::filesystem::GetFileSize(&Handle);
    uint8* BinaryBuffer = (uint8*)kraft::Malloc(BinaryBufferSize + 1, kraft::MemoryTag::MEMORY_TAG_FILE_BUF, true);
    filesystem::ReadAllBytes(&Handle, &BinaryBuffer);
    filesystem::CloseFile(&Handle);

    kraft::Buffer Reader((char*)BinaryBuffer, BinaryBufferSize);
    Reader.Read(&Shader.Name);
    Reader.Read(&Shader.ResourcePath);
    Reader.Read(&Shader.VertexLayouts);
    Reader.Read(&Shader.Resources);
    Reader.Read(&Shader.ConstantBuffers);
    Reader.Read(&Shader.RenderStates);

    uint64 RenderPassesCount;
    Reader.Read(&RenderPassesCount);

    Shader.RenderPasses = Array<RenderPassDefinition>(RenderPassesCount);
    for (int i = 0; i < RenderPassesCount; i++)
    {
        RenderPassDefinition& Pass = Shader.RenderPasses[i];
        Reader.Read(&Pass.Name);

        uint64 VertexLayoutOffset;
        Reader.Read(&VertexLayoutOffset);
        Pass.VertexLayout = &Shader.VertexLayouts[VertexLayoutOffset];

        uint64 ResourcesOffset;
        Reader.Read(&ResourcesOffset);
        Pass.Resources = &Shader.Resources[ResourcesOffset];

        uint64 ConstantBuffersOffset;
        Reader.Read(&ConstantBuffersOffset);
        Pass.ConstantBuffers = &Shader.ConstantBuffers[ConstantBuffersOffset];

        uint64 RenderStateOffset;
        Reader.Read(&RenderStateOffset);
        Pass.RenderState = &Shader.RenderStates[RenderStateOffset];

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
    
    return true;
}

bool ValidateShaderFX(const ShaderEffect& ShaderA, ShaderEffect& ShaderB)
{
    KASSERT(ShaderA.Name == ShaderB.Name);
    KASSERT(ShaderA.ResourcePath == ShaderB.ResourcePath);
    KASSERT(ShaderA.VertexLayouts.Length == ShaderB.VertexLayouts.Length);
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
