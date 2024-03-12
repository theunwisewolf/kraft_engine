#include "kraft_shaderfx.h"

#include "core/kraft_memory.h"
#include "core/kraft_asserts.h"
#include "platform/kraft_filesystem.h"
#include "containers/kraft_buffer.h"

#include "shaderc/shaderc.h"

namespace kraft::renderer
{

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
        if (Token.Type == TOKEN_TYPE_IDENTIFIER)
        {
            this->ParseIdentifier(Token);
        }
        else if (Token.Type == TOKEN_TYPE_END_OF_STREAM)
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
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
    {
        return;
    }

    this->Shader.Name = String(Token.Text, Token.Length);
}

void ShaderFXParser::ParseLayoutBlock()
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TOKEN_TYPE_IDENTIFIER) continue;

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
    }
}

void ShaderFXParser::ParseResourceBindings(ResourceBindingsDefinition& ResourceBindings)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TOKEN_TYPE_IDENTIFIER) continue;

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
            this->Error = true;
            this->ErrorLine = this->Lexer->Line;
            this->ErrorString = "Invalid binding type '" + String(Token.Text, Token.Length) + "'";

            return;
        }

        // Parse the name of the binding
        Token = this->Lexer->NextToken();
        Binding.Name = String(Token.Text, Token.Length);

        while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_SEMICOLON))
        {
            auto NamedToken = this->ParseNamedToken(Token);
            if (NamedToken.Key == "Stage")
            {
                bool Valid = false;
                for (int i = 0; i < ShaderStage::Count; i++)
                {
                    if (NamedToken.Value.StringView() == ShaderStage::Strings[i])
                    {
                        Binding.Stage = (ShaderStage::Enum)i;
                        Valid = true;
                        break;
                    }
                }

                if (!Valid)
                {
                    this->Error = true;
                    this->ErrorLine = this->Lexer->Line;
                    this->ErrorString = "Invalid shader stage '" + NamedToken.Value.String() + "'";

                    return;
                }
            }
            else if (NamedToken.Key == "Binding")
            {
                Binding.Binding = NamedToken.Value.FloatValue;
            }
        }

        ResourceBindings.ResourceBindings.Push(Binding);
    }
}

ShaderFXParser::NamedToken ShaderFXParser::ParseNamedToken(Token Token)
{
    ShaderFXParser::NamedToken Result = {};
    if (Token.Type != TOKEN_TYPE_IDENTIFIER)
    {
        return Result;
    }

    Result.Key = StringView(Token.Text, Token.Length);

    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_PARENTHESIS))
    {
        return Result;
    }

    // Skip past the parenthesis
    Token = this->Lexer->NextToken();
    Result.Value = Token;
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_CLOSE_PARENTHESIS))
    {
        return Result;
    }

    return Result;
}

void ShaderFXParser::ParseVertexLayout(VertexLayoutDefinition& Layout)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
    {
        // TODO: Errors when something other than an identifier is found
        if (Token.Type != TOKEN_TYPE_IDENTIFIER) continue;

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
    Attribute.Format = VertexAttributeFormat::Count;

    // Format identifier
    if (this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
    {
        char Char = Token.Text[0];
        switch (Char)
        {
            case 'f': // "Float", "Float2", "Float3", "Float4"
            {
                if (Token.MatchesKeyword("float"))
                {
                    Attribute.Format = VertexAttributeFormat::Float;
                }
                else if (Token.MatchesKeyword("float2"))
                {
                    Attribute.Format = VertexAttributeFormat::Float2;
                }
                else if (Token.MatchesKeyword("float3"))
                {
                    Attribute.Format = VertexAttributeFormat::Float3;
                }
                else if (Token.MatchesKeyword("float4"))
                {
                    Attribute.Format = VertexAttributeFormat::Float4;
                }
            } break;
            case 'm': // "Mat4"
            {
                if (Token.MatchesKeyword("mat4"))
                {
                    Attribute.Format = VertexAttributeFormat::Mat4;
                }
            }
            case 'b': // "Byte", "Byte4N"
            {
                if (Token.MatchesKeyword("byte"))
                {
                    Attribute.Format = VertexAttributeFormat::Byte;
                }
                else if (Token.MatchesKeyword("byte4n"))
                {
                    Attribute.Format = VertexAttributeFormat::Byte4N;
                }
            } break;
            case 'u': // "UByte", "UByte4N", "UInt", "UInt2", "UInt4"
            {
                if (Token.MatchesKeyword("ubyte"))
                {
                    Attribute.Format = VertexAttributeFormat::UByte;
                }
                else if (Token.MatchesKeyword("ubyte4n"))
                {
                    Attribute.Format = VertexAttributeFormat::UByte4N;
                }
                else if (Token.MatchesKeyword("uint"))
                {
                    Attribute.Format = VertexAttributeFormat::UInt;
                }
                else if (Token.MatchesKeyword("uint2"))
                {
                    Attribute.Format = VertexAttributeFormat::UInt2;
                }
                else if (Token.MatchesKeyword("uint4"))
                {
                    Attribute.Format = VertexAttributeFormat::UInt4;
                }
            } break;
            case 's': // "Short2", "Short2N", "Short4", "Short4N"
            {
                if (Token.MatchesKeyword("short2"))
                {
                    Attribute.Format = VertexAttributeFormat::Short2;
                }
                else if (Token.MatchesKeyword("short2n"))
                {
                    Attribute.Format = VertexAttributeFormat::Short2N;
                }
                else if (Token.MatchesKeyword("short4"))
                {
                    Attribute.Format = VertexAttributeFormat::Short4;
                }
                else if (Token.MatchesKeyword("short4n"))
                {
                    Attribute.Format = VertexAttributeFormat::Short4N;
                }
            } break;
        }
    }

    if (Attribute.Format == VertexAttributeFormat::Count)
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    // Attribute name
    // Skip
    // Attribute.Name = String(Token, Token.Length);

    // Binding
    if (this->Lexer->ExpectToken(Token, TOKEN_TYPE_NUMBER))
    {
        Attribute.Binding = Token.FloatValue;
    }
    else
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    // Location
    if (this->Lexer->ExpectToken(Token, TOKEN_TYPE_NUMBER))
    {
        Attribute.Location = Token.FloatValue;
    }
    else
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    // Offset
    if (this->Lexer->ExpectToken(Token, TOKEN_TYPE_NUMBER))
    {
        Attribute.Offset = Token.FloatValue;
    }
    else
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    Layout.Attributes.Push(Attribute);
}

void ShaderFXParser::ParseVertexInputBinding(VertexLayoutDefinition& Layout)
{
    Token Token;
    VertexInputBinding InputBinding;
    
    // Binding
    if (this->Lexer->ExpectToken(Token, TOKEN_TYPE_NUMBER))
    {
        InputBinding.Binding = Token.FloatValue;
    }
    else
    {
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

        return;
    }

    // Stride
    if (this->Lexer->ExpectToken(Token, TOKEN_TYPE_NUMBER))
    {
        InputBinding.Stride = Token.FloatValue;
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
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;

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
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }
    
    Token = this->Lexer->NextToken();
    CodeFragment.Code.Buffer = Token.Text;

    int OpenBraceCount = 1;
    while (OpenBraceCount)
    {
        if (Token.Type == TOKEN_TYPE_OPEN_BRACE)
        {
            OpenBraceCount++;
        }
        else if (Token.Type == TOKEN_TYPE_CLOSE_BRACE)
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
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    // Parse all the render states
    while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TOKEN_TYPE_IDENTIFIER)
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
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TOKEN_TYPE_IDENTIFIER)
        {
            this->Error = true;
            this->ErrorLine = this->Lexer->Line;
            this->ErrorString = "Expected Identifier";
            
            return;
        }

        if (Token.MatchesKeyword("Cull"))
        {
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }
            
            if (Token.MatchesKeyword("Back"))
            {
                State.CullMode = CullMode::Back;
            }
            else if (Token.MatchesKeyword("Front"))
            {
                State.CullMode = CullMode::Front;
            }
            else if (Token.MatchesKeyword("FrontAndBack"))
            {
                State.CullMode = CullMode::FrontAndBack;
            }
            else if (Token.MatchesKeyword("Off"))
            {
                State.CullMode = CullMode::None;
            }
            else
            {
                this->Error = true;
                this->ErrorLine = this->Lexer->Line;
                this->ErrorString = "Invalid value for CullMode";

                return;
            }
        }
        else if (Token.MatchesKeyword("ZTest"))
        {
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
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
                this->Error = true;
                this->ErrorLine = this->Lexer->Line;
                this->ErrorString = "Invalid value for ZTest";

                return;
            }
        }
        else if (Token.MatchesKeyword("ZWrite"))
        {
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (Token.MatchesKeyword("On"))
            {
                State.ZWriteEnable = true;
            }
            else if (Token.MatchesKeyword("On"))
            {
                State.ZWriteEnable = false;
            }
            else
            {
                this->Error = true;
                this->ErrorLine = this->Lexer->Line;
                this->ErrorString = "Invalid value for ZWrite";

                return;
            }
        }
        else if (Token.MatchesKeyword("Blend"))
        {
            // Lot of tokens to parse here

            // SrcColor
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.SrcColorBlendFactor))
            {
                return;
            }

            // DstColor
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.DstColorBlendFactor))
            {
                return;
            }

            // Comma
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_COMMA))
            {
                return;
            }

            // SrcAlpha
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendFactor(Token, State.BlendMode.SrcAlphaBlendFactor))
            {
                return;
            }

            // DstAlpha
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
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
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
            {
                return;
            }

            if (!this->ParseBlendOp(Token, State.BlendMode.ColorBlendOperation))
            {
                return;
            }

            // Comma
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_COMMA))
            {
                return;
            }

            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
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
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
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
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_NUMBER))
            {
                return;
            }
            
            State.LineWidth = Token.FloatValue;
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
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;
        this->ErrorString = "Invalid value for Blend";

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
        this->Error = true;
        this->ErrorLine = this->Lexer->Line;
        this->ErrorString = "Invalid value " + Token.String() + " for BlendOp";

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
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_OPEN_BRACE))
    {
        return;
    }

    while (!this->Lexer->EqualsToken(Token, TOKEN_TYPE_CLOSE_BRACE))
    {
        if (Token.Type != TOKEN_TYPE_IDENTIFIER)
        {
            this->Error = true;
            this->ErrorLine = this->Lexer->Line;
            this->ErrorString = "Expected identifier";

            return;
        }

        if (Token.MatchesKeyword("RenderState"))
        {
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER)) return;

            bool Valid = false;
            for (int i = 0; i < this->Shader.RenderStates.Length; i++)
            {
                if (this->Shader.RenderStates[i].Name == StringView(Token.Text, Token.Length))
                {
                    Pass.RenderState = &this->Shader.RenderStates[i];
                    Valid = true;
                    break;
                }
            }

            if (!Valid)
            {
                this->Error = true;
                this->ErrorLine = this->Lexer->Line;
                this->ErrorString = "Unknown render state";

                return;
            }
        }
        else if (Token.MatchesKeyword("VertexLayout"))
        {
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER)) return;
            
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

            if (!Valid)
            {
                this->Error = true;
                this->ErrorLine = this->Lexer->Line;
                this->ErrorString = "Unknown vertex layout";

                return;
            }
        }
        else if (Token.MatchesKeyword("Resources"))
        {
            if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER)) return;

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

            if (!Valid)
            {
                this->Error = true;
                this->ErrorLine = this->Lexer->Line;
                this->ErrorString = "Unknown resource block";

                return;
            }
        }
        else if (Token.MatchesKeyword("VertexShader"))
        {
            if (!this->ParseRenderPassShaderStage(Pass, ShaderStage::Vertex))
            {
                return;
            }
        }
        else if (Token.MatchesKeyword("FragmentShader"))
        {
            if (!this->ParseRenderPassShaderStage(Pass, ShaderStage::Fragment))
            {
                return;
            }
        }
    }

    this->Shader.RenderPasses.Push(Pass);
}

bool ShaderFXParser::ParseRenderPassShaderStage(RenderPassDefinition& Pass, renderer::ShaderStage::Enum ShaderStage)
{
    Token Token;
    if (!this->Lexer->ExpectToken(Token, TOKEN_TYPE_IDENTIFIER))
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

    this->Error = true;
    this->ErrorLine = this->Lexer->Line;
    this->ErrorString = "Unknown shader";

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

    Buffer BinaryOutput;
    BinaryOutput.Write(Shader.Name);
    BinaryOutput.Write(Shader.ResourcePath);
    BinaryOutput.Write(Shader.VertexLayouts);
    BinaryOutput.Write(Shader.Resources);
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
        BinaryOutput.Write((uint64)(Pass.RenderState - &Shader.RenderStates[0]));

        // Manually write the shader code fragments to the buffer
        BinaryOutput.Write(Pass.ShaderStages.Length);

        for (int j = 0; j < Pass.ShaderStages.Length; j++)
        {
            const RenderPassDefinition::ShaderDefinition& Stage = Pass.ShaderStages[j];
            KDEBUG("Compiling shaderstage %s for %s", ShaderStage::String(Stage.Stage), *Shader.ResourcePath);

            CompileOptions = shaderc_compile_options_initialize();
            const char* StageName;
            uint64 StageNameLength;

            shaderc_shader_kind ShaderKind;
            switch (Stage.Stage)
            {
                case ShaderStage::Vertex: 
                {
                    ShaderKind = shaderc_vertex_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_VERTEX_DEFINE, sizeof(KRAFT_VERTEX_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;

                case ShaderStage::Geometry: 
                {
                    ShaderKind = shaderc_geometry_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_GEOMETRY_DEFINE, sizeof(KRAFT_GEOMETRY_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;

                case ShaderStage::Fragment: 
                {
                    ShaderKind = shaderc_fragment_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_FRAGMENT_DEFINE, sizeof(KRAFT_FRAGMENT_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;

                case ShaderStage::Compute:
                {
                    ShaderKind = shaderc_compute_shader;
                    shaderc_compile_options_add_macro_definition(CompileOptions, KRAFT_COMPUTE_DEFINE, sizeof(KRAFT_COMPUTE_DEFINE) - 1, KRAFT_SHADERFX_ENABLE_VAL, sizeof(KRAFT_SHADERFX_ENABLE_VAL) - 1);
                }
                break;
            }
            
            Result = shaderc_compile_into_spv(Compiler, *Stage.CodeFragment.Code, Stage.CodeFragment.Code.Length, ShaderKind, ShaderStage::String(Stage.Stage), "main", CompileOptions);
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
                KERROR("%s shader compilation failed with error:\n%s", ShaderStage::String(Stage.Stage), shaderc_result_get_error_message(Result));

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

    Buffer Reader((char*)BinaryBuffer, BinaryBufferSize);
    Reader.Read(&Shader.Name);
    Reader.Read(&Shader.ResourcePath);
    Reader.Read(&Shader.VertexLayouts);
    Reader.Read(&Shader.Resources);
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
